// SPDX-License-Identifier: AGPL-3.0-or-later
// Google Drive REST API v3 FileBrowserSource using JUCE URL/WebInputStream (NSURLSession TLS on iOS & macOS;
// no proprietary Google SDKs). Credentials live exclusively in <secretDir>/<id>.secret (mode 0600) and are never logged.
#pragma once

#include "kick_googleDriveHelper.h"
#include "kick_secretStore.h"
#include "kick_sourceConfig.h"
#include "kick_webdavHelper.h"

#include <atomic>
#include <map>
#include <mutex>

namespace ui
{
    namespace fileSource
    {
        class GoogleDriveSource : public FileBrowserSource
        {
        public:
            GoogleDriveSource( SourceConfig c, std::string secretFilePath )
                : cfg( std::move( c ) ), secretPath( std::move( secretFilePath ) )
            {
                auto target = gdrive::parseFolderTarget( cfg.location );
                rootFolderId = target.ok ? target.folderId : "root";
                rootResourceKey = target.resourceKey;

#if ! JUCE_USE_CURL && ( JUCE_LINUX || JUCE_BSD )
                st = State::Unsupported;
#else
                if( !target.ok )
                {
                    st = State::Error;
                }
                else
                {
                    auto creds = loadCreds();
                    if( creds.mode == "apikey" )
                        st = creds.apiKey.empty() ? State::SignedOut : State::Ready;
                    else if( !creds.accessToken.empty() || !creds.refreshToken.empty() )
                        st = State::Ready;
                    else
                        st = State::SignedOut;
                }
#endif
            }

            std::string         id() const override { return cfg.id; }
            std::string         displayName() const override { return cfg.name; }
            State               state() const override { return st.load(); }
            const SourceConfig& config() const { return cfg; }

            Result beginAuth( AuthChallenge& challenge, const CancelFn& cancel ) override
            {
                challenge = AuthChallenge{};
                if( st == State::Unsupported )
                    return Result::make( Status::Unsupported, "https is not available in this build" );
                if( st == State::Error )
                    return Result::make( Status::BadResponse, "invalid Google Drive target" );

                auto creds = loadCreds();
                if( creds.mode == "apikey" && creds.apiKey.empty() )
                {
                    st = State::SignedOut;
                    challenge.kind = AuthChallenge::Kind::Credentials;
                    return Result::make( Status::AuthFailed, "missing API key" );
                }
                if( creds.mode != "apikey" && creds.accessToken.empty() && creds.refreshToken.empty() )
                {
                    st = State::SignedOut;
                    challenge.kind = AuthChallenge::Kind::Credentials;
                    return Result::make( Status::AuthFailed, "not signed in" );
                }

                auto lr = list( "/", cancel );
                if( lr.result.status == Status::AuthFailed )
                    challenge.kind = AuthChallenge::Kind::Credentials;
                return lr.result;
            }

            ListResult list( const std::string& path, const CancelFn& cancel ) override
            {
                ListResult out;
                if( st == State::Unsupported )
                {
                    out.result = Result::make( Status::Unsupported, "https is not available in this build" );
                    return out;
                }
                const auto norm = webdav::normaliseSourcePath( path );
                Node       parent;
                auto       r = resolveNode( norm, parent, cancel );
                if( !r.ok() )
                {
                    out.result = r;
                    return out;
                }
                if( !parent.isDir )
                {
                    out.result = Result::make( Status::BadResponse, "not a folder" );
                    return out;
                }

                std::map<std::string, int> nameCounts;
                std::vector<Entry>         collected;
                std::map<std::string, Node> childNodes;
                std::string                pageToken;

                for( int page = 0; page < 10; ++page )
                {
                    if( cancel && cancel() )
                    {
                        out.result = Result::make( Status::Cancelled );
                        return out;
                    }
                    const auto creds = loadCreds();
                    const auto url   = gdrive::buildFilesListUrl( parent.fileId, parent.isSharedRoot, {}, pageToken, creds.mode == "apikey" ? creds.apiKey : std::string() );
                    std::string body;
                    int         httpStatus = 0;
                    r                      = apiGetText( url, {}, cancel, body, httpStatus );
                    if( !r.ok() )
                    {
                        out.result = r;
                        return out;
                    }

                    const auto parsed = juce::JSON::parse( juce::String::fromUTF8( body.c_str() ) );
                    auto*      obj    = parsed.getDynamicObject();
                    if( obj == nullptr )
                    {
                        out.result = Result::make( Status::BadResponse, "invalid Drive JSON response" );
                        return out;
                    }
                    if( auto* files = obj->getProperty( "files" ).getArray() )
                    {
                        for( const auto& itemVar : *files )
                        {
                            Node node;
                            if( !parseFileObject( itemVar, node ) )
                                continue;

                            // Avoid duplicate collisions inside a folder while keeping extensions intact.
                            std::string dispName = sanitiseEntryName( node.name );
                            const int   seen     = ++nameCounts[dispName];
                            if( seen > 1 )
                                dispName = appendIndexBeforeExtension( dispName, seen );

                            const std::string entryPath = webdav::joinPath( norm, dispName );
                            Entry             e;
                            e.name     = dispName;
                            e.path     = entryPath;
                            e.isDir    = node.isDir;
                            e.size     = node.size;
                            e.mtimeMs  = node.mtimeMs;
                            e.etag     = !node.etag.empty() ? node.etag : node.fileId;
                            collected.push_back( e );
                            childNodes[entryPath] = node;
                        }
                    }
                    pageToken = obj->getProperty( "nextPageToken" ).toString().toStdString();
                    if( pageToken.empty() )
                        break;
                }

                // Expose "Shared with me" at root when browsing a user's full Drive via OAuth.
                if( norm == "/" && rootFolderId == "root" )
                {
                    const auto creds = loadCreds();
                    if( creds.mode != "apikey" )
                    {
                        Entry shared;
                        shared.name  = "Shared with me";
                        shared.path  = "/Shared with me";
                        shared.isDir = true;
                        collected.push_back( shared );

                        Node sharedNode;
                        sharedNode.fileId       = "__shared__";
                        sharedNode.name         = shared.name;
                        sharedNode.isDir        = true;
                        sharedNode.isSharedRoot = true;
                        childNodes[shared.path] = sharedNode;
                    }
                }

                {
                    std::lock_guard<std::mutex> l( nodeM );
                    for( const auto& kv : childNodes )
                        nodes[kv.first] = kv.second;
                }

                std::sort( collected.begin(), collected.end(), []( const Entry& a, const Entry& b ) {
                    if( a.isDir != b.isDir )
                        return a.isDir;
                    return a.name < b.name;
                } );
                out.entries = std::move( collected );
                return out;
            }

            Result stat( const std::string& path, Entry& out, const CancelFn& cancel ) override
            {
                const auto norm = webdav::normaliseSourcePath( path );
                Node       n;
                auto       r = resolveNode( norm, n, cancel );
                if( !r.ok() )
                    return r;
                out.name    = norm == "/" ? cfg.name : n.name;
                out.path    = norm;
                out.isDir   = n.isDir;
                out.size    = n.size;
                out.mtimeMs = n.mtimeMs;
                out.etag    = !n.etag.empty() ? n.etag : n.fileId;
                return {};
            }

            FetchResult fetch( const std::string& path, const std::string& partFile, int64_t offset, const CancelFn& cancel, const ProgressFn& progress ) override
            {
                FetchResult fr;
                Node        n;
                auto        r = resolveNode( webdav::normaliseSourcePath( path ), n, cancel );
                if( !r.ok() )
                {
                    fr.result = r;
                    return fr;
                }
                if( n.isDir )
                {
                    fr.result = Result::make( Status::BadResponse, "cannot download a folder" );
                    return fr;
                }

                const auto creds = loadCreds();
                const auto url   = gdrive::buildFileGetUrl( n.fileId, true, creds.mode == "apikey" ? creds.apiKey : std::string() );

                for( int attempt = 0; attempt < 2; ++attempt )
                {
                    if( cancel && cancel() )
                    {
                        fr.result = Result::make( Status::Cancelled );
                        return fr;
                    }
                    std::string authHeader;
                    auto        authRes = ensureAuthHeader( attempt > 0, cancel, authHeader );
                    if( !authRes.ok() )
                    {
                        fr.result = authRes;
                        return fr;
                    }

                    std::string headers;
                    if( !authHeader.empty() )
                        headers += "Authorization: " + authHeader + "\r\n";
                    if( offset > 0 )
                        headers += "Range: " + webdav::rangeHeaderValue( offset ) + "\r\n";
                    if( !creds.resourceKey.empty() )
                        headers += "X-Goog-Drive-Resource-Keys: " + n.fileId + "/" + creds.resourceKey + "\r\n";

                    int                  status = 0;
                    juce::StringPairArray respHeaders;
                    juce::URL            u( juce::String::fromUTF8( url.c_str() ) );
                    auto                 opts = juce::URL::InputStreamOptions( juce::URL::ParameterHandling::inAddress )
                                    .withConnectionTimeoutMs( 20000 )
                                    .withHttpRequestCmd( "GET" )
                                    .withExtraHeaders( juce::String::fromUTF8( headers.c_str() ) )
                                    .withStatusCode( &status )
                                    .withResponseHeaders( &respHeaders )
                                    .withNumRedirectsToFollow( 5 );
                    std::unique_ptr<juce::InputStream> in = u.createInputStream( opts );
                    if( !in )
                    {
                        st        = State::Offline;
                        fr.result = Result::make( Status::NetworkError, "cannot reach Google Drive" );
                        return fr;
                    }
                    if( status == 401 && attempt == 0 && creds.mode == "oauth" && !creds.refreshToken.empty() )
                        continue; // token expired on server side: refresh once and retry
                    if( status == 401 || status == 403 )
                    {
                        st        = State::NeedsAuth;
                        fr.result = Result::make( Status::AuthFailed, "Google Drive authorisation rejected", status );
                        return fr;
                    }
                    if( status == 416 )
                    {
                        fr.restarted = true;
                        fr.result    = Result::make( Status::BadResponse, "range not satisfiable", 416 );
                        return fr;
                    }
                    if( status >= 400 )
                    {
                        fr.result = Result::make( status == 404 ? Status::NotFound : Status::BadResponse, "Drive download failed", status );
                        return fr;
                    }
                    st        = State::Ready;
                    fr.result = streamDownloadToPart( *in, status, respHeaders, offset, partFile, cancel, progress, fr );
                    return fr;
                }
                fr.result = Result::make( Status::AuthFailed, "Google Drive token refresh failed" );
                return fr;
            }

            ChangeResult changes( const std::string& path, const CancelFn& cancel ) override
            {
                ChangeResult cr;
                auto         lr = list( path, cancel );
                cr.result       = lr.result;
                if( !lr.result.ok() )
                    return cr;
                std::lock_guard<std::mutex> l( snapM );
                auto&                       prev = snapshots[webdav::normaliseSourcePath( path )];
                cr.changes                       = webdav::diffListings( prev, lr.entries );
                prev                             = lr.entries;
                return cr;
            }

            void signOut() override
            {
                deleteSecretFile( secretPath );
                {
                    std::lock_guard<std::mutex> l( nodeM );
                    nodes.clear();
                }
                st = State::SignedOut;
            }

            // Exchanges an interactive OAuth authorization code + PKCE verifier for access + refresh tokens,
            // then queries Drive `about` to discover the signed-in user's display name and email.
            static Result exchangeAuthCode( const std::string& clientId,
                                            const std::string& clientSecret,
                                            const std::string& redirectUri,
                                            const std::string& code,
                                            const std::string& codeVerifier,
                                            gdrive::Credentials& outCreds )
            {
                std::string post = "client_id=" + gdrive::encodeQueryComponent( clientId )
                                 + "&code=" + gdrive::encodeQueryComponent( code )
                                 + "&code_verifier=" + gdrive::encodeQueryComponent( codeVerifier )
                                 + "&grant_type=authorization_code"
                                 + "&redirect_uri=" + gdrive::encodeQueryComponent( redirectUri );
                if( !clientSecret.empty() )
                    post += "&client_secret=" + gdrive::encodeQueryComponent( clientSecret );

                std::string body;
                int         status = 0;
                auto        r      = httpFormPost( "https://oauth2.googleapis.com/token", post, nullptr, body, status );
                if( !r.ok() )
                    return r;

                const auto parsed = juce::JSON::parse( juce::String::fromUTF8( body.c_str() ) );
                auto*      obj    = parsed.getDynamicObject();
                if( obj == nullptr )
                    return Result::make( Status::BadResponse, "invalid token response", status );
                if( obj->hasProperty( "error" ) )
                {
                    const auto desc = obj->getProperty( "error_description" ).toString().toStdString();
                    const auto err  = obj->getProperty( "error" ).toString().toStdString();
                    return Result::make( Status::AuthFailed, desc.empty() ? err : desc, status );
                }
                outCreds.mode         = "oauth";
                outCreds.clientId     = clientId;
                outCreds.clientSecret = clientSecret;
                outCreds.redirectUri  = redirectUri;
                outCreds.accessToken  = obj->getProperty( "access_token" ).toString().toStdString();
                const auto ref        = obj->getProperty( "refresh_token" ).toString().toStdString();
                if( !ref.empty() )
                    outCreds.refreshToken = ref;
                const int64_t expSec = static_cast<int64_t>( static_cast<int>( obj->getProperty( "expires_in" ) ) );
                outCreds.expiresAtMs = juce::Time::currentTimeMillis() + ( expSec > 0 ? expSec * 1000 : 3500ll * 1000 );
                if( outCreds.accessToken.empty() )
                    return Result::make( Status::BadResponse, "missing access_token in OAuth reply", status );

                fetchUserAbout( outCreds.accessToken, outCreds.email, outCreds.displayName );
                return Result{};
            }

            static Result fetchUserAbout( const std::string& accessToken, std::string& outEmail, std::string& outDisplayName )
            {
                int                  status = 0;
                juce::URL            u( juce::String::fromUTF8( gdrive::buildAboutUserUrl().c_str() ) );
                const juce::String   hdrs = "Authorization: Bearer " + juce::String::fromUTF8( accessToken.c_str() ) + "\r\n";
                auto                 opts = juce::URL::InputStreamOptions( juce::URL::ParameterHandling::inAddress )
                                .withConnectionTimeoutMs( 15000 )
                                .withHttpRequestCmd( "GET" )
                                .withExtraHeaders( hdrs )
                                .withStatusCode( &status )
                                .withNumRedirectsToFollow( 3 );
                std::unique_ptr<juce::InputStream> in = u.createInputStream( opts );
                if( !in )
                    return Result::make( Status::NetworkError, "cannot reach Google Drive" );
                const auto body = in->readEntireStreamAsString();
                if( status == 401 || status == 403 )
                    return Result::make( Status::AuthFailed, "Google Drive authorisation failed", status );
                if( status < 200 || status >= 300 )
                    return Result::make( Status::BadResponse, "about request failed", status );
                const auto parsed = juce::JSON::parse( body );
                if( auto* obj = parsed.getDynamicObject() )
                {
                    if( auto* usr = obj->getProperty( "user" ).getDynamicObject() )
                    {
                        outEmail       = usr->getProperty( "emailAddress" ).toString().toStdString();
                        outDisplayName = usr->getProperty( "displayName" ).toString().toStdString();
                    }
                }
                return Result{};
            }

        private:
            struct Node
            {
                std::string fileId;
                std::string name;
                bool        isDir{ false };
                bool        isSharedRoot{ false };
                int64_t     size{ -1 };
                int64_t     mtimeMs{ 0 };
                std::string etag;
            };

            gdrive::Credentials loadCreds() const
            {
                std::lock_guard<std::mutex> l( credM );
                std::string                 raw;
                if( !readSecretFile( secretPath, raw ) )
                    return {};
                return gdrive::parseCredentials( raw );
            }

            void saveCreds( const gdrive::Credentials& c ) const
            {
                std::lock_guard<std::mutex> l( credM );
                writeSecretFile( secretPath, gdrive::serializeCredentials( c ) );
            }

            static Result httpFormPost( const std::string& url, const std::string& formBody, const CancelFn& cancel, std::string& outBody, int& outStatus )
            {
                if( cancel && cancel() )
                    return Result::make( Status::Cancelled );
                juce::URL u = juce::URL( juce::String::fromUTF8( url.c_str() ) ).withPOSTData( juce::String::fromUTF8( formBody.c_str() ) );
                int       status = 0;
                auto      opts   = juce::URL::InputStreamOptions( juce::URL::ParameterHandling::inPostData )
                              .withConnectionTimeoutMs( 15000 )
                              .withHttpRequestCmd( "POST" )
                              .withExtraHeaders( "Content-Type: application/x-www-form-urlencoded\r\n" )
                              .withStatusCode( &status )
                              .withNumRedirectsToFollow( 2 );
                std::unique_ptr<juce::InputStream> in = u.createInputStream( opts );
                outStatus                             = status;
                if( !in )
                    return Result::make( Status::NetworkError, "cannot reach Google OAuth server" );
                outBody = in->readEntireStreamAsString().toStdString();
                if( status >= 500 )
                    return Result::make( Status::BadResponse, "Google OAuth server error", status );
                return Result{};
            }

            Result ensureAuthHeader( bool forceRefresh, const CancelFn& cancel, std::string& outAuth )
            {
                outAuth.clear();
                auto creds = loadCreds();
                if( creds.mode == "apikey" )
                {
                    if( creds.apiKey.empty() )
                    {
                        st = State::SignedOut;
                        return Result::make( Status::AuthFailed, "missing API key" );
                    }
                    return Result{};
                }

                const int64_t nowMs   = juce::Time::currentTimeMillis();
                const bool    expired = creds.expiresAtMs > 0 && nowMs >= ( creds.expiresAtMs - 60000 );
                if( !forceRefresh && !expired && !creds.accessToken.empty() )
                {
                    outAuth = "Bearer " + creds.accessToken;
                    return Result{};
                }
                if( creds.refreshToken.empty() || creds.clientId.empty() )
                {
                    if( !forceRefresh && !creds.accessToken.empty() && creds.expiresAtMs == 0 )
                    {
                        outAuth = "Bearer " + creds.accessToken;
                        return Result{};
                    }
                    st = State::NeedsAuth;
                    return Result::make( Status::AuthFailed, "Google Drive session expired" );
                }

                std::string post = "client_id=" + gdrive::encodeQueryComponent( creds.clientId )
                                 + "&refresh_token=" + gdrive::encodeQueryComponent( creds.refreshToken )
                                 + "&grant_type=refresh_token";
                if( !creds.clientSecret.empty() )
                    post += "&client_secret=" + gdrive::encodeQueryComponent( creds.clientSecret );

                std::string body;
                int         status = 0;
                auto        r      = httpFormPost( "https://oauth2.googleapis.com/token", post, cancel, body, status );
                if( !r.ok() )
                {
                    if( r.status == Status::NetworkError )
                        st = State::Offline;
                    return r;
                }
                const auto parsed = juce::JSON::parse( juce::String::fromUTF8( body.c_str() ) );
                auto*      obj    = parsed.getDynamicObject();
                if( obj == nullptr || obj->hasProperty( "error" ) )
                {
                    st = State::NeedsAuth;
                    return Result::make( Status::AuthFailed, "Google Drive token refresh rejected", status );
                }
                const auto tok = obj->getProperty( "access_token" ).toString().toStdString();
                if( tok.empty() )
                {
                    st = State::NeedsAuth;
                    return Result::make( Status::AuthFailed, "empty refreshed token", status );
                }
                creds.accessToken = tok;
                const int64_t exp = static_cast<int64_t>( static_cast<int>( obj->getProperty( "expires_in" ) ) );
                creds.expiresAtMs = nowMs + ( exp > 0 ? exp * 1000 : 3500ll * 1000 );
                saveCreds( creds );
                st      = State::Ready;
                outAuth = "Bearer " + creds.accessToken;
                return Result{};
            }

            Result apiGetText( const std::string& url, const std::string& targetIdForKey, const CancelFn& cancel, std::string& outBody, int& outHttpStatus )
            {
                const auto creds = loadCreds();
                for( int attempt = 0; attempt < 2; ++attempt )
                {
                    if( cancel && cancel() )
                        return Result::make( Status::Cancelled );
                    std::string authHeader;
                    auto        ar = ensureAuthHeader( attempt > 0, cancel, authHeader );
                    if( !ar.ok() )
                        return ar;

                    std::string headers;
                    if( !authHeader.empty() )
                        headers += "Authorization: " + authHeader + "\r\n";
                    if( !creds.resourceKey.empty() && !targetIdForKey.empty() )
                        headers += "X-Goog-Drive-Resource-Keys: " + targetIdForKey + "/" + creds.resourceKey + "\r\n";
                    else if( !creds.resourceKey.empty() && rootFolderId != "root" )
                        headers += "X-Goog-Drive-Resource-Keys: " + rootFolderId + "/" + creds.resourceKey + "\r\n";

                    int       status = 0;
                    juce::URL u( juce::String::fromUTF8( url.c_str() ) );
                    auto      opts = juce::URL::InputStreamOptions( juce::URL::ParameterHandling::inAddress )
                                    .withConnectionTimeoutMs( 15000 )
                                    .withHttpRequestCmd( "GET" )
                                    .withExtraHeaders( juce::String::fromUTF8( headers.c_str() ) )
                                    .withStatusCode( &status )
                                    .withNumRedirectsToFollow( 3 );
                    std::unique_ptr<juce::InputStream> in = u.createInputStream( opts );
                    outHttpStatus                         = status;
                    if( !in )
                    {
                        st = State::Offline;
                        return Result::make( Status::NetworkError, "cannot reach Google Drive" );
                    }
                    outBody = in->readEntireStreamAsString().toStdString();
                    if( status == 401 && attempt == 0 && creds.mode == "oauth" && !creds.refreshToken.empty() )
                        continue;
                    if( status == 401 || status == 403 )
                    {
                        st = State::NeedsAuth;
                        return Result::make( Status::AuthFailed, "Google Drive authorisation failed", status );
                    }
                    if( status == 404 )
                        return Result::make( Status::NotFound, "folder or file not found", status );
                    if( status < 200 || status >= 300 )
                        return Result::make( Status::BadResponse, "Google Drive API error", status );
                    st = State::Ready;
                    return Result{};
                }
                return Result::make( Status::AuthFailed, "Google Drive authorisation failed" );
            }

            Result resolveNode( const std::string& normPath, Node& out, const CancelFn& cancel )
            {
                if( normPath == "/" )
                {
                    out.fileId = rootFolderId;
                    out.name   = cfg.name;
                    out.isDir  = true;
                    return Result{};
                }
                {
                    std::lock_guard<std::mutex> l( nodeM );
                    auto                        it = nodes.find( normPath );
                    if( it != nodes.end() )
                    {
                        out = it->second;
                        return Result{};
                    }
                }

                // Walk segments sequentially from root when cache does not contain this path yet.
                std::string curPath   = "/";
                std::string parentId  = rootFolderId;
                bool        shared    = false;
                size_t      pos       = 1;
                while( pos < normPath.size() )
                {
                    const size_t sl   = normPath.find( '/', pos );
                    const auto   part = normPath.substr( pos, sl == std::string::npos ? std::string::npos : sl - pos );
                    curPath           = webdav::joinPath( curPath, part );

                    Node cached;
                    bool found = false;
                    {
                        std::lock_guard<std::mutex> l( nodeM );
                        auto                        it = nodes.find( curPath );
                        if( it != nodes.end() )
                        {
                            cached = it->second;
                            found  = true;
                        }
                    }
                    if( !found )
                    {
                        if( curPath == "/Shared with me" && rootFolderId == "root" )
                        {
                            cached.fileId       = "__shared__";
                            cached.name         = "Shared with me";
                            cached.isDir        = true;
                            cached.isSharedRoot = true;
                            found               = true;
                        }
                        else
                        {
                            const auto creds = loadCreds();
                            const auto url   = gdrive::buildFilesListUrl( parentId, shared, part, {}, creds.mode == "apikey" ? creds.apiKey : std::string() );
                            std::string body;
                            int         httpStatus = 0;
                            auto        r          = apiGetText( url, parentId, cancel, body, httpStatus );
                            if( !r.ok() )
                                return r;
                            const auto parsed = juce::JSON::parse( juce::String::fromUTF8( body.c_str() ) );
                            auto*      obj    = parsed.getDynamicObject();
                            auto*      files  = obj != nullptr ? obj->getProperty( "files" ).getArray() : nullptr;
                            if( files != nullptr )
                            {
                                for( const auto& f : *files )
                                {
                                    if( parseFileObject( f, cached ) )
                                    {
                                        found = true;
                                        break;
                                    }
                                }
                            }
                        }
                        if( !found )
                            return Result::make( Status::NotFound, "not found on Google Drive" );
                        std::lock_guard<std::mutex> l( nodeM );
                        nodes[curPath] = cached;
                    }
                    out      = cached;
                    parentId = cached.fileId;
                    shared   = cached.isSharedRoot;
                    if( sl == std::string::npos )
                        break;
                    pos = sl + 1;
                }
                return Result{};
            }

            static bool parseFileObject( const juce::var& v, Node& out )
            {
                auto* obj = v.getDynamicObject();
                if( obj == nullptr )
                    return false;
                std::string fileId   = obj->getProperty( "id" ).toString().toStdString();
                std::string name     = obj->getProperty( "name" ).toString().toStdString();
                std::string mime     = obj->getProperty( "mimeType" ).toString().toStdString();
                if( fileId.empty() || name.empty() )
                    return false;

                if( mime == "application/vnd.google-apps.shortcut" )
                {
                    if( auto* sc = obj->getProperty( "shortcutDetails" ).getDynamicObject() )
                    {
                        const auto tid   = sc->getProperty( "targetId" ).toString().toStdString();
                        const auto tmime = sc->getProperty( "targetMimeType" ).toString().toStdString();
                        if( !tid.empty() )
                            fileId = tid;
                        if( !tmime.empty() )
                            mime = tmime;
                    }
                }

                const bool isFolder = ( mime == "application/vnd.google-apps.folder" );
                // Skip native Google Docs / Sheets / Slides that are not downloadable audio or folders.
                if( !isFolder && mime.rfind( "application/vnd.google-apps.", 0 ) == 0 )
                    return false;

                out.fileId       = std::move( fileId );
                out.name         = std::move( name );
                out.isDir        = isFolder;
                out.isSharedRoot = false;
                out.size         = isFolder || !obj->hasProperty( "size" ) ? -1 : static_cast<int64_t>( obj->getProperty( "size" ).toString().getLargeIntValue() );
                out.mtimeMs      = gdrive::parseRfc3339Ms( obj->getProperty( "modifiedTime" ).toString().toStdString() );
                out.etag         = obj->getProperty( "md5Checksum" ).toString().toStdString();
                return true;
            }

            static std::string sanitiseEntryName( const std::string& raw )
            {
                std::string s = raw;
                std::replace( s.begin(), s.end(), '/', '_' );
                return s.empty() ? "item" : s;
            }

            static std::string appendIndexBeforeExtension( const std::string& name, int idx )
            {
                const auto dot = name.rfind( '.' );
                const auto tag = " (" + std::to_string( idx ) + ")";
                if( dot == std::string::npos || dot == 0 )
                    return name + tag;
                return name.substr( 0, dot ) + tag + name.substr( dot );
            }

            Result streamDownloadToPart( juce::InputStream& in, int status, const juce::StringPairArray& h, int64_t offset, const std::string& partFile, const CancelFn& cancel, const ProgressFn& progress, FetchResult& fr )
            {
                const int64_t clen = h["Content-Length"].isNotEmpty() ? h["Content-Length"].getLargeIntValue() : -1;
                auto          dec  = webdav::classifyGetResponse( offset, status, h["Content-Range"].toStdString(), clen );
                if( dec.action == webdav::RangeAction::AlreadyComplete )
                {
                    fr.complete    = true;
                    fr.totalBytes  = dec.total;
                    fr.bytesOnDisk = offset;
                    return Result{};
                }
                if( dec.action == webdav::RangeAction::Fail )
                    return Result::make( Status::BadResponse, "unexpected download response", status );

                const bool append = ( dec.action == webdav::RangeAction::Append );
                fr.restarted      = offset > 0 && !append;
                fr.totalBytes     = dec.total;
                juce::File f( juce::String::fromUTF8( partFile.c_str() ) );
                std::unique_ptr<juce::FileOutputStream> out( f.createOutputStream() );
                if( !out || out->failedToOpen() )
                    return Result::make( Status::IoError, "cannot open part file" );
                if( append )
                {
                    out->setPosition( offset );
                    out->truncate();
                }
                else
                {
                    out->setPosition( 0 );
                    out->truncate();
                }

                int64_t pos = append ? offset : 0;
                char    buf[65536];
                for( ;; )
                {
                    if( cancel && cancel() )
                    {
                        out->flush();
                        fr.bytesOnDisk = pos;
                        return Result::make( Status::Cancelled );
                    }
                    auto n = in.read( buf, sizeof buf );
                    if( n <= 0 )
                        break;
                    if( !out->write( buf, static_cast<size_t>( n ) ) )
                    {
                        fr.bytesOnDisk = pos;
                        return Result::make( Status::IoError, "disk write failed" );
                    }
                    pos += n;
                    if( progress )
                        progress( pos, fr.totalBytes );
                }
                out->flush();
                fr.bytesOnDisk = pos;
                fr.complete    = fr.totalBytes < 0 ? in.isExhausted() : pos == fr.totalBytes;
                if( !fr.complete )
                {
                    st = State::Offline;
                    return Result::make( Status::NetworkError, "Google Drive download interrupted" );
                }
                return Result{};
            }

            SourceConfig                               cfg;
            std::string                                secretPath;
            std::string                                rootFolderId{ "root" };
            std::string                                rootResourceKey;
            std::atomic<State>                         st{ State::SignedOut };
            mutable std::mutex                         credM;
            std::mutex                                 nodeM;
            std::map<std::string, Node>                nodes;
            std::mutex                                 snapM;
            std::map<std::string, std::vector<Entry>>  snapshots;
        };
    } // namespace fileSource
} // namespace ui
