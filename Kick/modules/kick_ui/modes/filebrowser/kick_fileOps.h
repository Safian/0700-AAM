// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE-free) file operations for the file browser and the project operations: copy / move / delete of files and folder
// trees with byte progress, cancellation, clean-up of a partial target and a final sync. Meant to run on a worker thread (never
// the message thread). Host-tested with temp folders: Kick/tests/platform/FileOpsTests.cpp.
#pragma once

#include "kick_fsGuard.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <system_error>
#include <vector>
#if defined( __unix__ ) || defined( __APPLE__ )
    #include <unistd.h>
#endif

namespace kick_fileops
{
    namespace fs = std::filesystem;

    enum class Kind { Copy, Move, Delete };

    struct Job
    {
        Kind                     kind{ Kind::Copy };
        std::vector<std::string> sources;
        std::string              destDir; // Copy / Move only; must exist
    };

    struct Progress
    {
        uint64_t    bytesDone{ 0 }, bytesTotal{ 0 };
        uint32_t    itemsDone{ 0 }, itemsTotal{ 0 };
        std::string current; // file name being handled

        int percent() const { return bytesTotal ? static_cast<int>( bytesDone * 100 / bytesTotal ) : ( itemsTotal ? static_cast<int>( itemsDone * 100 / itemsTotal ) : 0 ); }
    };

    struct Result
    {
        bool                     ok{ true };         // every item was handled
        bool                     cancelled{ false }; // stopped by the user (what was finished stays, the item in progress is cleaned up)
        std::string              error;              // first failure, human readable
        std::vector<std::string> done;               // targets that were created (Copy / Move)
        std::vector<std::string> failed;             // sources that could not be handled
    };

    using ProgressFn = std::function<void( const Progress& )>;
    using ProtectFn  = std::function<bool( const std::string& )>; // true = must not be moved / deleted

    namespace detail
    {
        inline uint64_t treeBytes( const fs::path& p, uint32_t& items, const std::atomic<bool>& cancel )
        {
            std::error_code ec;
            uint64_t        total = 0;
            if( fs::is_symlink( p, ec ) || !fs::is_directory( p, ec ) )
            {
                ++items;
                const auto s = fs::is_symlink( p, ec ) ? 0 : fs::file_size( p, ec );
                return ec ? 0 : s;
            }
            ++items;
            for( fs::recursive_directory_iterator it( p, fs::directory_options::skip_permission_denied, ec ), end; !ec && it != end; it.increment( ec ) )
            {
                if( cancel.load() )
                    return total;
                std::error_code e2;
                if( it->is_symlink( e2 ) )
                {
                    ++items;
                    continue;
                }
                if( it->is_regular_file( e2 ) )
                {
                    const auto s = it->file_size( e2 );
                    total += e2 ? 0 : s;
                }
                ++items;
            }
            return total;
        }

        struct Ctx
        {
            const std::atomic<bool>& cancel;
            const ProgressFn&        progress;
            Progress                 p;
            std::string              error;
            void                     report() const
            {
                if( progress )
                    progress( p );
            }
        };

        // Chunked copy of one file; false on failure or cancel (the partial target is removed by the caller).
        inline bool copyFile( const fs::path& from, const fs::path& to, Ctx& c )
        {
            std::ifstream in( from, std::ios::binary );
            if( !in )
            {
                c.error = "cannot read " + from.filename().string();
                return false;
            }
            std::ofstream out( to, std::ios::binary | std::ios::trunc );
            if( !out )
            {
                c.error = "cannot write " + to.filename().string();
                return false;
            }
            std::vector<char> buf( 1 << 20 );
            c.p.current = from.filename().string();
            while( in )
            {
                if( c.cancel.load() )
                    return false;
                in.read( buf.data(), static_cast<std::streamsize>( buf.size() ) );
                const auto n = in.gcount();
                if( n <= 0 )
                    break;
                out.write( buf.data(), n );
                if( !out )
                {
                    c.error = "no space left or write error on " + to.filename().string();
                    return false;
                }
                c.p.bytesDone += static_cast<uint64_t>( n );
                c.report();
            }
            out.flush();
            if( !out )
            {
                c.error = "write error on " + to.filename().string();
                return false;
            }
            std::error_code ec;
            fs::permissions( to, fs::status( from, ec ).permissions(), ec ); // best effort (FAT ignores it)
            return !in.bad();
        }

        inline bool copyTree( const fs::path& from, const fs::path& to, Ctx& c )
        {
            std::error_code ec;
            if( fs::is_symlink( from, ec ) )
            {
                // Links are not followed (a link loop would never end) and not copied.
                ++c.p.itemsDone;
                return true;
            }
            if( !fs::is_directory( from, ec ) )
            {
                const bool ok = copyFile( from, to, c );
                ++c.p.itemsDone;
                return ok;
            }
            fs::create_directory( to, ec );
            if( ec )
            {
                c.error = "cannot create " + to.filename().string();
                return false;
            }
            ++c.p.itemsDone;
            std::vector<fs::path> children;
            for( fs::directory_iterator it( from, fs::directory_options::skip_permission_denied, ec ), end; !ec && it != end; it.increment( ec ) )
                children.push_back( it->path() );
            for( const auto& child : children )
            {
                if( c.cancel.load() )
                    return false;
                if( !copyTree( child, to / child.filename(), c ) )
                    return false;
            }
            return true;
        }

        // Removes a tree file by file so the cancel flag is honoured between files. false = stopped / failed.
        inline bool removeTree( const fs::path& p, Ctx& c )
        {
            std::error_code ec;
            if( fs::is_symlink( p, ec ) || !fs::is_directory( p, ec ) )
            {
                const auto sz = fs::is_symlink( p, ec ) ? 0 : fs::file_size( p, ec );
                if( !fs::remove( p, ec ) || ec )
                {
                    c.error = "cannot delete " + p.filename().string();
                    return false;
                }
                c.p.bytesDone += ec ? 0 : sz;
                ++c.p.itemsDone;
                c.p.current = p.filename().string();
                c.report();
                return true;
            }
            std::vector<fs::path> children;
            for( fs::directory_iterator it( p, fs::directory_options::skip_permission_denied, ec ), end; !ec && it != end; it.increment( ec ) )
                children.push_back( it->path() );
            for( const auto& child : children )
            {
                if( c.cancel.load() )
                    return false;
                if( !removeTree( child, c ) )
                    return false;
            }
            if( !fs::remove( p, ec ) || ec )
            {
                c.error = "cannot delete " + p.filename().string();
                return false;
            }
            ++c.p.itemsDone;
            c.report();
            return true;
        }

        inline void flushToDisk()
        {
#if defined( __unix__ ) || defined( __APPLE__ )
            ::sync();
#endif
        }
    }

    // Runs the job. `protect` is asked for every source of a Move / Delete; a protected one is skipped and reported as failed.
    // Copy / Move targets get a free name in destDir ("a (2).wav"): nothing that exists is ever replaced. A folder is never
    // copied or moved into itself.
    inline Result run( const Job& job, const std::atomic<bool>& cancel, const ProgressFn& progress = {}, const ProtectFn& protect = {} )
    {
        Result      r;
        detail::Ctx c{ cancel, progress, {}, {} };
        std::error_code ec;

        for( const auto& s : job.sources )
            c.p.bytesTotal += detail::treeBytes( s, c.p.itemsTotal, cancel );
        c.report();

        auto fail = [&]( const std::string& source, const std::string& why ) {
            r.ok = false;
            r.failed.push_back( source );
            if( r.error.empty() )
                r.error = why;
        };

        for( const auto& source : job.sources )
        {
            if( cancel.load() )
            {
                r.cancelled = true;
                r.ok        = false;
                break;
            }
            const fs::path src( source );
            if( !fs::exists( fs::symlink_status( src, ec ) ) )
            {
                fail( source, "not found: " + src.filename().string() );
                continue;
            }
            if( job.kind != Kind::Copy && protect && protect( source ) )
            {
                fail( source, "protected: " + src.filename().string() );
                continue;
            }

            if( job.kind == Kind::Delete )
            {
                if( !detail::removeTree( src, c ) )
                {
                    if( cancel.load() )
                    {
                        r.cancelled = true;
                        r.ok        = false;
                        break;
                    }
                    fail( source, c.error.empty() ? "cannot delete " + src.filename().string() : c.error );
                }
                continue;
            }

            // Copy / Move
            const bool isDir = fs::is_directory( src, ec ) && !fs::is_symlink( src, ec );
            if( isDir && kick_fsguard::pasteIntoItself( source, job.destDir ) )
            {
                fail( source, "a folder cannot be placed inside itself: " + src.filename().string() );
                continue;
            }
            if( job.kind == Kind::Move && src.parent_path() == fs::path( job.destDir ) )
                continue; // already there
            const auto name = kick_fsguard::uniqueName( src.filename().string(), [&]( const std::string& n ) { return fs::exists( fs::symlink_status( fs::path( job.destDir ) / n, ec ) ); } );
            if( name.empty() )
            {
                fail( source, "no free name for " + src.filename().string() );
                continue;
            }
            const fs::path target = fs::path( job.destDir ) / name;

            if( job.kind == Kind::Move )
            {
                // Same file system: a rename, instant. Otherwise (SD <-> internal) copy and delete after the copy is complete.
                std::error_code rn;
                fs::rename( src, target, rn );
                if( !rn )
                {
                    uint32_t items = 0;
                    c.p.bytesDone += detail::treeBytes( target, items, cancel );
                    c.p.itemsDone += items;
                    c.report();
                    r.done.push_back( target.string() );
                    continue;
                }
            }

            const bool copied = detail::copyTree( src, target, c );
            if( !copied )
            {
                fs::remove_all( target, ec ); // the partial target (it did not exist before: the name was free)
                if( cancel.load() )
                {
                    r.cancelled = true;
                    r.ok        = false;
                    break;
                }
                fail( source, c.error.empty() ? "cannot copy " + src.filename().string() : c.error );
                continue;
            }
            if( job.kind == Kind::Move )
            {
                detail::Ctx dummy{ cancel, {}, {}, {} };
                if( !detail::removeTree( src, dummy ) )
                {
                    // The copy is complete; the original could not be removed. Keep both, say so.
                    fail( source, "copied, but the original could not be removed: " + src.filename().string() );
                    r.done.push_back( target.string() );
                    continue;
                }
            }
            r.done.push_back( target.string() );
        }

        detail::flushToDisk(); // the data is on the stick / card before "done" is shown
        c.p.current.clear();
        if( !r.cancelled )
        {
            c.p.bytesDone = c.p.bytesTotal;
            c.p.itemsDone = c.p.itemsTotal;
        }
        c.report();
        return r;
    }

    // Copies the tree `from` to the not yet existing folder `to` (exactly that name, no "(2)"). A failed / cancelled copy leaves
    inline Result copyFolderTo( const std::string& from, const std::string& to, const std::atomic<bool>& cancel, const ProgressFn& progress = {} )
    {
        Result          r;
        std::error_code ec;
        if( fs::exists( fs::symlink_status( to, ec ) ) )
        {
            r.ok    = false;
            r.error = "the target already exists";
            return r;
        }
        if( kick_fsguard::pasteIntoItself( from, to ) )
        {
            r.ok    = false;
            r.error = "a folder cannot be copied into itself";
            return r;
        }
        detail::Ctx c{ cancel, progress, {}, {} };
        c.p.bytesTotal = detail::treeBytes( from, c.p.itemsTotal, cancel );
        c.report();
        if( !detail::copyTree( from, to, c ) )
        {
            fs::remove_all( to, ec );
            r.ok        = false;
            r.cancelled = cancel.load();
            r.error     = r.cancelled ? "cancelled" : ( c.error.empty() ? "copy failed" : c.error );
            r.failed.push_back( from );
            return r;
        }
        detail::flushToDisk();
        r.done.push_back( to );
        return r;
    }
}
