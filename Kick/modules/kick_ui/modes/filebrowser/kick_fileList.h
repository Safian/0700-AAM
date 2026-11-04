// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE-free) folder listing for the file browser: hidden / system file filter, folders first, sorting by name (natural, case
// insensitive) / date / size, extension filter and a cancellable, capped recursive search. Host-tested:
// Kick/tests/platform/FileListTests.cpp.
#pragma once

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <system_error>
#include <vector>

namespace kick_filelist
{
    namespace fs = std::filesystem;

    struct Entry
    {
        std::string name;
        std::string path;
        bool        isDir{ false };
        bool        isProject{ false }; // a folder that holds <name>.kickPrj (only set when ListOptions::detectProjects)
        int64_t     size{ -1 }; // files only; folders are sized later, asynchronously
        int64_t     mtimeMs{ 0 };
    };

    enum class SortKey { Name, Date, Size };

    inline std::string lower( std::string s )
    {
        for( auto& c : s )
            c = static_cast<char>( std::tolower( static_cast<unsigned char>( c ) ) );
        return s;
    }

    // Dot files, macOS / Windows / Linux bookkeeping that only makes noise in a sample browser.
    inline bool isHiddenOrSystem( const std::string& name )
    {
        if( name.empty() || name[0] == '.' )
            return true;
        const auto l = lower( name );
        return l == "system volume information" || l == "$recycle.bin" || l == "lost+found" || l == "thumbs.db" || l == "desktop.ini" || l == "found.000" || l == "__macosx";
    }

    // "Kick 2" sorts before "Kick 10"; letters compare without case.
    inline int naturalCompare( const std::string& a, const std::string& b )
    {
        size_t i = 0, j = 0;
        while( i < a.size() && j < b.size() )
        {
            if( std::isdigit( static_cast<unsigned char>( a[i] ) ) && std::isdigit( static_cast<unsigned char>( b[j] ) ) )
            {
                size_t ie = i, je = j;
                while( ie < a.size() && std::isdigit( static_cast<unsigned char>( a[ie] ) ) ) ++ie;
                while( je < b.size() && std::isdigit( static_cast<unsigned char>( b[je] ) ) ) ++je;
                auto na = a.substr( i, ie - i ), nb = b.substr( j, je - j );
                na.erase( 0, std::min( na.find_first_not_of( '0' ), na.size() ) );
                nb.erase( 0, std::min( nb.find_first_not_of( '0' ), nb.size() ) );
                if( na.size() != nb.size() )
                    return na.size() < nb.size() ? -1 : 1;
                if( const int c = na.compare( nb ) )
                    return c < 0 ? -1 : 1;
                i = ie;
                j = je;
                continue;
            }
            const int ca = std::tolower( static_cast<unsigned char>( a[i] ) ), cb = std::tolower( static_cast<unsigned char>( b[j] ) );
            if( ca != cb )
                return ca < cb ? -1 : 1;
            ++i;
            ++j;
        }
        if( i < a.size() ) return 1;
        if( j < b.size() ) return -1;
        return 0;
    }

    // Folders first, then by key; ties by name.
    inline void sortEntries( std::vector<Entry>& v, SortKey key, bool ascending = true )
    {
        std::stable_sort( v.begin(), v.end(), [&]( const Entry& a, const Entry& b ) {
            if( a.isDir != b.isDir )
                return a.isDir;
            int c = 0;
            switch( key )
            {
                case SortKey::Name: c = naturalCompare( a.name, b.name ); break;
                case SortKey::Date: c = a.mtimeMs < b.mtimeMs ? -1 : ( a.mtimeMs > b.mtimeMs ? 1 : 0 ); break;
                case SortKey::Size: c = a.isDir ? 0 : ( a.size < b.size ? -1 : ( a.size > b.size ? 1 : 0 ) ); break;
            }
            if( c == 0 )
                c = naturalCompare( a.name, b.name );
            return ascending ? c < 0 : c > 0;
        } );
    }

    // ".wav;bwf;aif" or ".wav;.aif": does `name` end with one of the extensions (no case)?
    inline bool hasExtension( const std::string& name, const std::string& list )
    {
        const auto dot = name.rfind( '.' );
        if( dot == std::string::npos || dot == 0 )
            return false;
        const auto ext = lower( name.substr( dot + 1 ) );
        size_t     pos = 0;
        while( pos <= list.size() )
        {
            auto semi = list.find( ';', pos );
            if( semi == std::string::npos )
                semi = list.size();
            auto e = lower( list.substr( pos, semi - pos ) );
            if( !e.empty() && e[0] == '.' )
                e.erase( 0, 1 );
            if( !e.empty() && e == ext )
                return true;
            pos = semi + 1;
        }
        return false;
    }

    struct ListOptions
    {
        bool        hideSystem{ true };
        std::string extensions; // empty = everything; folders are always kept (they are how the user navigates)
        bool        detectProjects{ false };
        bool        hideProjectData{ false }; // inside a project folder: no samples / export / imported folders
        SortKey     key{ SortKey::Name };
        bool        ascending{ true };
    };

    struct Listing
    {
        std::vector<Entry> entries;
        bool               ok{ true };
        std::string        error; // "cannot read this folder" ...
    };

    inline int64_t toMs( const fs::file_time_type& t )
    {
        // file_time_type -> system clock without C++20 clock_cast
        const auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>( t - fs::file_time_type::clock::now() + std::chrono::system_clock::now() );
        return std::chrono::duration_cast<std::chrono::milliseconds>( sctp.time_since_epoch() ).count();
    }

    inline Entry makeEntry( const fs::directory_entry& de )
    {
        Entry           e;
        std::error_code ec;
        e.name  = de.path().filename().string();
        e.path  = de.path().string();
        e.isDir = de.is_directory( ec );
        if( !e.isDir )
        {
            const auto s = de.file_size( ec );
            e.size       = ec ? -1 : static_cast<int64_t>( s );
        }
        const auto t = de.last_write_time( ec );
        e.mtimeMs    = ec ? 0 : toMs( t );
        return e;
    }

    inline Listing list( const std::string& dir, const ListOptions& o = {} )
    {
        Listing         l;
        std::error_code ec;
        fs::directory_iterator it( dir, fs::directory_options::skip_permission_denied, ec ), end;
        if( ec )
        {
            l.ok    = false;
            l.error = "This folder cannot be read";
            return l;
        }
        for( ; it != end; it.increment( ec ) )
        {
            if( ec )
                break;
            const auto name = it->path().filename().string();
            if( o.hideSystem && isHiddenOrSystem( name ) )
                continue;
            auto e = makeEntry( *it );
            if( !e.isDir && !o.extensions.empty() && !hasExtension( e.name, o.extensions ) )
                continue;
            if( e.isDir && o.hideProjectData )
            {
                const auto n = lower( e.name );
                if( n == "samples" || n == "export" || n == "imported" || n == "exports" )
                    continue;
            }
            if( e.isDir && o.detectProjects )
            {
                std::error_code pe;
                e.isProject = fs::is_regular_file( it->path() / ( e.name + ".kickPrj" ), pe );
            }
            l.entries.push_back( std::move( e ) );
        }
        sortEntries( l.entries, o.key, o.ascending );
        return l;
    }

    struct SearchResult
    {
        std::vector<Entry> entries;
        bool               truncated{ false }; // the cap was hit
        bool               cancelled{ false };
    };

    // Case-insensitive "name contains text". recursive = the whole tree below `dir` (never through links), at most `maxResults`.
    inline SearchResult search( const std::string& dir, const std::string& text, bool recursive, size_t maxResults, const std::atomic<bool>& cancel, const ListOptions& o = {} )
    {
        SearchResult    r;
        std::error_code ec;
        const auto      needle = lower( text );
        auto            consider = [&]( const fs::directory_entry& de ) {
            const auto name = de.path().filename().string();
            if( o.hideSystem && isHiddenOrSystem( name ) )
                return true;
            if( !needle.empty() && lower( name ).find( needle ) == std::string::npos )
                return true;
            auto e = makeEntry( de );
            if( !e.isDir && !o.extensions.empty() && !hasExtension( e.name, o.extensions ) )
                return true;
            if( r.entries.size() >= maxResults )
            {
                r.truncated = true;
                return false;
            }
            r.entries.push_back( std::move( e ) );
            return true;
        };
        if( recursive )
        {
            for( fs::recursive_directory_iterator it( dir, fs::directory_options::skip_permission_denied, ec ), end; !ec && it != end; it.increment( ec ) )
            {
                if( cancel.load() )
                {
                    r.cancelled = true;
                    break;
                }
                const auto name = it->path().filename().string();
                if( o.hideSystem && isHiddenOrSystem( name ) )
                {
                    if( it->is_directory( ec ) )
                        it.disable_recursion_pending();
                    continue;
                }
                std::error_code e2;
                if( it->is_symlink( e2 ) )
                    continue;
                if( !consider( *it ) )
                    break;
            }
        }
        else
        {
            for( fs::directory_iterator it( dir, fs::directory_options::skip_permission_denied, ec ), end; !ec && it != end; it.increment( ec ) )
            {
                if( cancel.load() )
                {
                    r.cancelled = true;
                    break;
                }
                if( !consider( *it ) )
                    break;
            }
        }
        sortEntries( r.entries, o.key, o.ascending );
        return r;
    }
}
