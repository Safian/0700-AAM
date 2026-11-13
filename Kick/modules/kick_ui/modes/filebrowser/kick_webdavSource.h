// SPDX-License-Identifier: AGPL-3.0-or-later
// M1: WebDAV source on JUCE's own URL/WebInputStream (no extra dependency). Needs JUCE headers; the pure parts live in
// kick_webdavHelper.h. Credentials come from a 0600 secret file and are only put into the Authorization header; they are
// never logged. Redirects are followed manually and only within the same origin (the Authorization header must not leak).
// Without JUCE_USE_CURL (the Linux/MPC build) JUCE's built-in socket client is used: plain http:// works, https:// does not.
#pragma once

#include "kick_sourceConfig.h"
#include "kick_webdavHelper.h"

#include <atomic>
#include <map>
#include <mutex>

namespace ui
{
    namespace fileSource
    {
        class WebDavSource : public FileBrowserSource
        {
        public:
            WebDavSource( SourceConfig c, std::string secretFilePath ) : cfg( std::move( c ) ), secretPath( std::move( secretFilePath ) )
            {
                auto u = webdav::parseUrl( cfg.location );
#if ! JUCE_USE_CURL && ( JUCE_LINUX || JUCE_BSD )
                const bool tlsOk = false;
#else
                const bool tlsOk = true;
#endif
                if( !u.ok ) st = State::Error;
                else if( u.scheme == "https" && !tlsOk ) st = State::Unsupported;
                else st = hasSecret() ? State::Ready : State::SignedOut;
            }

            std::string id() const override { return cfg.id; }
            std::string displayName() const override { return cfg.name; }
            State       state() const override { return st.load(); }
            const SourceConfig& config() const { return cfg; }

            // Probes the server root; the UI collects user/password itself (Credentials challenge) when none are stored.
            Result beginAuth( AuthChallenge& challenge, const CancelFn& cancel ) override
            {
                challenge = AuthChallenge{};
                if( st == State::Unsupported ) return Result::make( Status::Unsupported, "https is not available in this build" );
                if( st == State::Error ) return Result::make( Status::BadResponse, "invalid server URL" );
                if( cfg.auth == "basic" && cfg.user.empty() && !hasSecret() ) { st = State::SignedOut; challenge.kind = AuthChallenge::Kind::Credentials; return Result{}; }
                auto lr = list( "/", cancel );
                if( lr.result.status == Status::AuthFailed ) { challenge.kind = AuthChallenge::Kind::Credentials; }
                return lr.result;
            }

            ListResult list( const std::string& path, const CancelFn& cancel ) override
            {
                ListResult r;
                Reply      rep;
                auto       sp = webdav::normaliseSourcePath( path );
                r.result      = request( "PROPFIND", sp, true, "Depth: 1\r\nContent-Type: application/xml; charset=utf-8\r\n", webdav::propfindBody(), 0, cancel, rep );
                if( !r.result.ok() )
                {
                    kick_log( "error", "[WEBDAV-DEBUG] request failed: status=%d http=%d msg=%s\n", static_cast<int>( r.result.status ), r.result.httpStatus, r.result.message.c_str() );
                    return r;
                }
                if( rep.status != 207 )
                {
                    kick_log( "error", "[WEBDAV-DEBUG] rep.status != 207 (got %d)\n", rep.status );
                    r.result = Result::make( Status::BadResponse, "unexpected PROPFIND status", rep.status );
                    return r;
                }
                if( !webdav::parsePropfind( rep.body, cfg.location, sp, r.entries ) )
                {
                    kick_log( "error", "[WEBDAV-DEBUG] parsePropfind failed: body_len=%zu\n", rep.body.size() );
                    r.result = Result::make( Status::BadResponse, "unreadable PROPFIND response" );
                    return r;
                }
                std::sort( r.entries.begin(), r.entries.end(), []( const Entry& a, const Entry& b ) { return a.isDir != b.isDir ? a.isDir : a.name < b.name; } );
                return r;
            }

            Result stat( const std::string& path, Entry& out, const CancelFn& cancel ) override
            {
                Reply rep;
                auto  sp = webdav::normaliseSourcePath( path );
                auto  r  = request( "PROPFIND", sp, false, "Depth: 0\r\nContent-Type: application/xml; charset=utf-8\r\n", webdav::propfindBody(), 0, cancel, rep );
                if( !r.ok() ) return r;
                if( rep.status != 207 ) return Result::make( Status::BadResponse, "unexpected PROPFIND status", rep.status );
                std::vector<Entry> es;
                // Depth 0 returns the resource itself: parse with a request path that cannot match so it is kept.
                if( !webdav::parsePropfind( rep.body, cfg.location, "/\x01none", es ) || es.empty() ) return Result::make( Status::BadResponse, "unreadable PROPFIND response" );
                out = es.front();
                return Result{};
            }

            FetchResult fetch( const std::string& path, const std::string& partFile, int64_t offset, const CancelFn& cancel, const ProgressFn& progress ) override
            {
                FetchResult fr;
                auto        sp = webdav::normaliseSourcePath( path );
                Reply       rep;
                rep.streamTo  = juce::File( partFile );
                rep.offset    = offset > 0 ? offset : 0;
                rep.progress  = progress;
                std::string hdr = offset > 0 ? "Range: " + webdav::rangeHeaderValue( offset ) + "\r\n" : std::string();
                fr.result       = request( "GET", sp, false, hdr, {}, offset, cancel, rep );
                fr.complete     = rep.complete;
                fr.totalBytes   = rep.total;
                fr.bytesOnDisk  = rep.onDisk;
                fr.restarted    = rep.restarted;
                return fr;
            }

            ChangeResult changes( const std::string& path, const CancelFn& cancel ) override
            {
                ChangeResult cr;
                auto         lr = list( path, cancel );
                cr.result       = lr.result;
                if( !lr.result.ok() ) return cr;
                std::lock_guard<std::mutex> l( snapM );
                auto&                       prev = snapshots[webdav::normaliseSourcePath( path )];
                cr.changes                       = webdav::diffListings( prev, lr.entries );
                prev                             = lr.entries;
                return cr;
            }

            void signOut() override
            {
                deleteSecretFile( secretPath );
                st = State::SignedOut;
            }

            // The UI stores the password/token here (0600). Returns false on I/O failure.
            bool storeSecret( const std::string& secret )
            {
                if( !writeSecretFile( secretPath, secret ) ) return false;
                if( st == State::SignedOut || st == State::NeedsAuth ) st = State::Ready;
                return true;
            }
            bool hasSecret() const { std::string s; return readSecretFile( secretPath, s ) && !s.empty(); }

        private:
            struct Reply
            {
                int         status{ 0 };
                std::string body;                 // for non-streaming requests (capped)
                juce::File  streamTo;             // GET: write the body here
                int64_t     offset{ 0 };
                ProgressFn  progress;
                bool        complete{ false };
                bool        restarted{ false };
                int64_t     total{ -1 };
                int64_t     onDisk{ 0 };
            };

            std::string authorization() const
            {
                if( cfg.auth == "none" ) return {};
                std::string secret;
                if( !readSecretFile( secretPath, secret ) ) return {};
                while( !secret.empty() && ( secret.back() == '\n' || secret.back() == '\r' ) ) secret.pop_back();
                return cfg.auth == "bearer" ? webdav::bearerAuthValue( secret ) : webdav::basicAuthValue( cfg.user, secret );
            }

            static Result mapStatus( int http )
            {
                if( http == 401 || http == 403 ) return Result::make( Status::AuthFailed, "authentication failed", http );
                if( http == 404 || http == 410 ) return Result::make( Status::NotFound, "not found", http );
                return Result::make( Status::BadResponse, "HTTP error", http );
            }

            Result request( const char* method, const std::string& sourcePath, bool collection, const std::string& extraHeaders, const std::string& body, int64_t offset, const CancelFn& cancel, Reply& rep )
            {
                if( st == State::Unsupported ) return Result::make( Status::Unsupported, "https is not available in this build" );
                if( st == State::Error ) return Result::make( Status::BadResponse, "invalid server URL" );
                auto url    = webdav::buildUrl( cfg.location, sourcePath, collection );
                auto origin = webdav::parseUrl( cfg.location );
                if( url.empty() ) return Result::make( Status::BadResponse, "invalid URL" );
                auto auth = authorization();
                if( auth.empty() && ( cfg.auth == "bearer" || !cfg.user.empty() ) )
                {
                    st = State::SignedOut;
                    return Result::make( Status::AuthFailed, "no credentials stored" );
                }

                for( int hop = 0; hop < 4; ++hop )
                {
                    if( cancel && cancel() ) return Result::make( Status::Cancelled );
                    std::string headers = extraHeaders;
                    if( !auth.empty() ) headers += "Authorization: " + auth + "\r\n";
                    kick_log( "error", "[WEBDAV-DEBUG] req method=%s url=%s headers='%s' body_len=%zu\n", method, url.c_str(), headers.c_str(), body.size() );
                    juce::URL u( juce::String::fromUTF8( url.c_str() ) );
                    if( !body.empty() ) u = u.withPOSTData( juce::String::fromUTF8( body.c_str() ) );
                    int                  status = 0;
                    juce::StringPairArray respHeaders;
                    auto opts = juce::URL::InputStreamOptions( body.empty() ? juce::URL::ParameterHandling::inAddress : juce::URL::ParameterHandling::inPostData )
                                    .withConnectionTimeoutMs( 15000 )
                                    .withHttpRequestCmd( method )
                                    .withExtraHeaders( juce::String::fromUTF8( headers.c_str() ) )
                                    .withStatusCode( &status )
                                    .withResponseHeaders( &respHeaders )
                                    .withNumRedirectsToFollow( 0 );
                    std::unique_ptr<juce::InputStream> in = u.createInputStream( opts );
                    if( !in )
                    {
                        st = State::Offline;
                        kick_log( "error", "[WEBDAV-DEBUG] createInputStream null: %s\n", url.c_str() );
                        return Result::make( Status::NetworkError, "cannot connect to " + webdav::redactedUrl( url ) );
                    }
                    kick_log( "error", "[WEBDAV-DEBUG] createInputStream ok: status=%d url=%s\n", status, url.c_str() );
                    rep.status = status;
                    if( status == 301 || status == 302 || status == 307 || status == 308 )
                    {
                        auto loc = respHeaders["Location"].toStdString();
                        auto lu  = webdav::parseUrl( loc.find( "://" ) == std::string::npos && !loc.empty() && loc[0] == '/' ? webdav::originOf( origin ) + loc : loc );
                        if( !lu.ok || !webdav::sameOrigin( lu, origin ) ) return Result::make( Status::BadResponse, "redirect to another origin refused", status );
                        url = webdav::originOf( lu ) + lu.path;
                        continue;
                    }
                    if( status == 401 || status == 403 ) { st = State::NeedsAuth; return mapStatus( status ); }
                    st = State::Ready;
                    if( std::string( method ) == "GET" ) return readFile( *in, status, respHeaders, offset, cancel, rep );
                    if( status != 207 && ( status < 200 || status >= 300 ) ) return mapStatus( status );
                    // PROPFIND body (capped)
                    juce::MemoryBlock mb;
                    char              buf[8192];
                    for( ;; )
                    {
                        if( cancel && cancel() ) return Result::make( Status::Cancelled );
                        auto n = in->read( buf, sizeof buf );
                        if( n <= 0 ) break;
                        mb.append( buf, (size_t)n );
                        if( mb.getSize() > 16u * 1024 * 1024 ) return Result::make( Status::BadResponse, "response too large" );
                    }
                    rep.body.assign( (const char*)mb.getData(), mb.getSize() );
                    return Result{};
                }
                return Result::make( Status::BadResponse, "too many redirects" );
            }

            Result readFile( juce::InputStream& in, int status, const juce::StringPairArray& h, int64_t offset, const CancelFn& cancel, Reply& rep )
            {
                const int64_t clen = h["Content-Length"].isNotEmpty() ? h["Content-Length"].getLargeIntValue() : -1;
                auto          dec  = webdav::classifyGetResponse( offset, status, h["Content-Range"].toStdString(), clen );
                if( dec.action == webdav::RangeAction::AlreadyComplete )
                {
                    rep.complete = true;
                    rep.total    = dec.total;
                    rep.onDisk   = offset;
                    return Result{};
                }
                if( dec.action == webdav::RangeAction::Fail ) return status >= 400 ? mapStatus( status ) : Result::make( Status::BadResponse, "unexpected response", status );
                if( status == 416 ) // part file does not fit the remote file: caller restarts from 0
                {
                    rep.restarted = true;
                    return Result::make( Status::BadResponse, "range not satisfiable", 416 );
                }
                const bool append = dec.action == webdav::RangeAction::Append;
                rep.restarted     = offset > 0 && !append;
                rep.total         = dec.total;
                std::unique_ptr<juce::FileOutputStream> out( rep.streamTo.createOutputStream() );
                if( !out || out->failedToOpen() ) return Result::make( Status::IoError, "cannot open part file" );
                if( append ) out->setPosition( offset ), out->truncate(); else { out->setPosition( 0 ); out->truncate(); }
                int64_t pos = append ? offset : 0;
                char    buf[65536];
                for( ;; )
                {
                    if( cancel && cancel() ) { out->flush(); rep.onDisk = pos; return Result::make( Status::Cancelled ); }
                    auto n = in.read( buf, sizeof buf );
                    if( n <= 0 ) break;
                    if( !out->write( buf, (size_t)n ) ) { rep.onDisk = pos; return Result::make( Status::IoError, "disk write failed" ); }
                    pos += n;
                    if( rep.progress ) rep.progress( pos, rep.total );
                }
                out->flush();
                rep.onDisk   = pos;
                rep.complete = rep.total < 0 ? in.isExhausted() : pos == rep.total;
                if( !rep.complete ) { st = State::Offline; return Result::make( Status::NetworkError, "connection ended early" ); }
                return Result{};
            }

            SourceConfig                               cfg;
            std::string                                secretPath;
            std::atomic<State>                         st{ State::SignedOut };
            std::mutex                                 snapM;
            std::map<std::string, std::vector<Entry>>  snapshots;
        };
    }
}
