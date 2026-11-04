// SPDX-License-Identifier: AGPL-3.0-or-later
// File-browser source interface (M0): one abstraction behind the "Location" list. JUCE-free (std only) so the logic around
// it (cache, download queue, WebDAV helpers) is host-testable. All methods except state()/id()/displayName() may block on the
// network or disk and must only be called from a worker thread, never the message thread.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ui
{
    namespace fileSource
    {
        enum class State
        {
            SignedOut,   // no credentials / not authorised yet
            NeedsAuth,   // credentials rejected or an interactive step (device flow) is pending
            Ready,
            Offline,     // transient network failure
            Error,
            Unsupported  // this build cannot talk to the source (e.g. https without TLS)
        };

        enum class Status
        {
            Ok,
            Cancelled,
            NotFound,
            AuthFailed,
            NetworkError,
            Unsupported,
            BadResponse,
            IoError
        };

        struct Result
        {
            Status      status{ Status::Ok };
            int         httpStatus{ 0 };
            std::string message; // never contains credentials, tokens or URLs with user info

            bool ok() const noexcept { return status == Status::Ok; }
            static Result make( Status s, std::string msg = {}, int http = 0 ) { return Result{ s, http, std::move( msg ) }; }
        };

        struct Entry
        {
            std::string name;      // last path component (decoded)
            std::string path;      // path inside the source, always starts with '/', no trailing '/' (except root "/")
            bool        isDir{ false };
            int64_t     size{ -1 }; // -1 = unknown
            int64_t     mtimeMs{ 0 };
            std::string etag;      // opaque validator ("" = none)
        };

        struct ListResult
        {
            Result             result;
            std::vector<Entry> entries;
        };

        // Interactive step of beginAuth. Credentials: the UI collects user/secret itself (WebDAV). DeviceCode: the user opens
        // verificationUrl on another device and types userCode (OAuth device flow; reserved for future sources, no source in
        // this tree uses it yet).
        struct AuthChallenge
        {
            enum class Kind { None, Credentials, DeviceCode };
            Kind        kind{ Kind::None };
            std::string userCode;
            std::string verificationUrl;
            int         expiresInSec{ 0 };
            int         pollIntervalSec{ 5 };
        };

        struct FetchResult
        {
            Result  result;
            bool    complete{ false };
            int64_t totalBytes{ -1 };   // full size of the remote file when known
            int64_t bytesOnDisk{ 0 };   // size of the .part file after this call
            bool    restarted{ false }; // the server ignored the Range request and the file was rewritten from byte 0
        };

        struct Change
        {
            enum class Kind { Added, Removed, Modified };
            Kind  kind{ Kind::Added };
            Entry entry;
        };

        struct ChangeResult
        {
            Result              result;
            std::vector<Change> changes;
        };

        using CancelFn   = std::function<bool()>;
        using ProgressFn = std::function<void( int64_t done, int64_t total )>;

        class FileBrowserSource
        {
        public:
            virtual ~FileBrowserSource() = default;

            virtual std::string id() const          = 0; // stable, filesystem-safe
            virtual std::string displayName() const = 0;
            virtual State       state() const       = 0; // cheap, thread-safe

            // Starts/refreshes authorisation. Blocking. On return `challenge` says what (if anything) the UI still has to do.
            virtual Result beginAuth( AuthChallenge& challenge, const CancelFn& cancel ) = 0;

            virtual ListResult list( const std::string& path, const CancelFn& cancel )         = 0;
            virtual Result     stat( const std::string& path, Entry& out, const CancelFn& cancel ) = 0;

            // Downloads `path` into `partFile`. offset > 0 asks to continue: the implementation sends a Range request and
            // appends only if the server honours it, else truncates and rewrites (FetchResult::restarted). The caller owns the
            // decision whether resuming is valid (see webdav::decideResume). Never writes anywhere but partFile.
            virtual FetchResult fetch( const std::string& path, const std::string& partFile, int64_t offset, const CancelFn& cancel, const ProgressFn& progress ) = 0;

            // Entries added/removed/modified under `path` since the previous call for the same path (first call: everything is Added).
            virtual ChangeResult changes( const std::string& path, const CancelFn& cancel ) = 0;

            // Forgets credentials/tokens (and deletes their secret files).
            virtual void signOut() = 0;

            // Sources that already are plain local files return the file directly (no download, no cache copy).
            virtual bool directFile( const std::string& path, std::string& outLocalPath ) const
            {
                (void)path;
                (void)outLocalPath;
                return false;
            }
        };
    }
}
