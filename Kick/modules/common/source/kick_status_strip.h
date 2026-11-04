/*
 * Copyright (c) 2024 MUSIC Tribe / Safian
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

// Status strip data (MIDI devices, audio device, /data free space, pendrive install state) and the project load
// queue / startup project policy. JUCE-free, header only (host test: tests/platform/StartupTests.cpp). Read-only: every
// failure becomes "n/a".

#include <cstdint>
#include <cstdio>
#include <deque>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/statvfs.h>

namespace kick_status
{
    inline std::string formatBytes( int64_t bytes )
    {
        if( bytes < 0 ) return "n/a";
        const char* units[] = { "B", "KB", "MB", "GB", "TB" };
        double      v       = (double)bytes;
        int         u       = 0;
        while( v >= 1024.0 && u < 4 )
        {
            v /= 1024.0;
            ++u;
        }
        char buf[32];
        if( u == 0 ) std::snprintf( buf, sizeof( buf ), "%lld B", (long long)bytes );
        else std::snprintf( buf, sizeof( buf ), v < 10.0 ? "%.1f %s" : "%.0f %s", v, units[u] );
        return buf;
    }

    // Free space of the file system that holds `path`; -1 when unknown.
    inline int64_t freeBytes( const std::string& path )
    {
        struct statvfs s {};
        if( ::statvfs( path.c_str(), &s ) != 0 ) return -1;
        return (int64_t)s.f_bavail * (int64_t)s.f_frsize;
    }

    // Last `maxBytes` of a text file ("" when unreadable).
    inline std::string readTail( const std::string& path, size_t maxBytes = 16384 )
    {
        std::ifstream in( path, std::ios::binary );
        if( !in ) return {};
        in.seekg( 0, std::ios::end );
        const auto size = (std::streamoff)in.tellg();
        if( size <= 0 ) return {};
        const auto start = size > (std::streamoff)maxBytes ? size - (std::streamoff)maxBytes : (std::streamoff)0;
        in.seekg( start );
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    inline std::string readWhole( const std::string& path ) { return readTail( path, 1 << 16 ); }

    inline std::string quotedAfter( const std::string& line, const std::string& marker )
    {
        const auto m = line.find( marker );
        if( m == std::string::npos ) return {};
        const auto a = line.find( '\'', m + marker.size() );
        if( a == std::string::npos ) return {};
        const auto b = line.find( '\'', a + 1 );
        if( b == std::string::npos ) return {};
        return line.substr( a + 1, b - a - 1 );
    }

    // Summary of the LAST tkgl_bootstrap run in /data/kick-boot.log (the script appends; a run starts with
    // "=== Kick bootstrap starting ==="): "installed", "updated A->B", "unchanged", "error: ...", "uninstalled", "n/a".
    inline std::string installStateFromLog( const std::string& logText )
    {
        if( logText.empty() ) return "n/a";
        const std::string startMarker = "=== Kick bootstrap starting ===";
        const auto        runStart    = logText.rfind( startMarker );
        const std::string run         = runStart == std::string::npos ? logText : logText.substr( runStart );

        std::string diffLine, errorLine;
        bool        fresh = false, finished = false, unchanged = false, uninstalled = false;
        std::istringstream in( run );
        std::string        line;
        while( std::getline( in, line ) )
        {
            if( line.find( "Version difference detected" ) != std::string::npos ) diffLine = line;
            else if( line.find( "Fresh installation required" ) != std::string::npos || line.find( "Re-installation / sync required" ) != std::string::npos ) fresh = true;
            else if( line.find( "Installation/update finished successfully" ) != std::string::npos ) finished = true;
            else if( line.find( "Flash write skipped" ) != std::string::npos ) unchanged = true;
            else if( line.find( "Uninstall complete" ) != std::string::npos ) uninstalled = true;
            else if( line.find( "ERROR" ) != std::string::npos && errorLine.empty() )
            {
                const auto p = line.find( "ERROR" );
                errorLine    = line.substr( p );
            }
        }
        if( uninstalled ) return "uninstalled";
        if( !errorLine.empty() && !finished ) return "error: " + errorLine.substr( 0, 60 );
        if( !diffLine.empty() )
        {
            const auto a = quotedAfter( diffLine, "local" );
            const auto b = quotedAfter( diffLine, "package" );
            return "updated " + ( a.empty() ? std::string( "?" ) : a ) + "->" + ( b.empty() ? std::string( "?" ) : b );
        }
        if( fresh ) return "installed";
        if( unchanged ) return "unchanged";
        return "n/a";
    }

    struct StripInfo
    {
        int         midiDevices{ -1 }; // -1 = unknown
        std::string audio;             // empty = unknown
        int64_t     dataFreeBytes{ -1 };
        std::string install;           // empty = unknown
    };

    inline std::string formatStrip( const StripInfo& s )
    {
        auto or_na = []( const std::string& v ) { return v.empty() ? std::string( "n/a" ) : v; };
        return "MIDI: " + ( s.midiDevices < 0 ? std::string( "n/a" ) : std::to_string( s.midiDevices ) ) + "   |   Audio: " + or_na( s.audio ) + "   |   /data: " + ( s.dataFreeBytes < 0 ? std::string( "n/a" ) : formatBytes( s.dataFreeBytes ) + " free" ) + "   |   Install: " + or_na( s.install );
    }

    // The file-system parts of the strip (everything but the MIDI count and the audio device name, which come from JUCE).
    inline StripInfo gatherFsInfo( const std::string& dataDir )
    {
        StripInfo s;
        s.dataFreeBytes = freeBytes( dataDir );
        s.install       = installStateFromLog( readTail( dataDir + "/kick-boot.log" ) );
        return s;
    }
}

namespace kick_load
{
    // Project load queue (message thread only). One load runs at a time; further requests wait their turn; a request for a path
    // that is already loading or queued is dropped (the former double load: auto-load + hardware bridge).
    class LoadQueue
    {
    public:
        enum class Result
        {
            StartNow,
            Queued,
            Duplicate
        };

        Result request( const std::string& path )
        {
            if( !busy_ )
            {
                busy_    = true;
                current_ = path;
                return Result::StartNow;
            }
            if( path == current_ ) return Result::Duplicate;
            for( const auto& q : queue_ )
                if( q == path ) return Result::Duplicate;
            queue_.push_back( path );
            return Result::Queued;
        }

        // The running load ended. Returns true and the next path when one is waiting (it is now the running one).
        bool onFinished( std::string& next )
        {
            if( queue_.empty() )
            {
                busy_ = false;
                current_.clear();
                return false;
            }
            next     = queue_.front();
            current_ = next;
            queue_.pop_front();
            return true;
        }

        bool               busy() const { return busy_; }
        size_t             pending() const { return queue_.size(); }
        const std::string& current() const { return current_; }

    private:
        bool                  busy_{ false };
        std::string           current_;
        std::deque<std::string> queue_;
    };

    struct StartupInputs
    {
        bool        safeMode{ false };
        bool        autoLoad{ false };
        std::string lastProject; // settings.projects.currentProject()
        bool        lastExists{ false };
        std::string fallbackProject; // device default (Demo 01 on the MPC), empty = none
        bool        fallbackExists{ false };
    };

    enum class StartupReason
    {
        SafeMode,
        LastProject,
        Fallback,
        AutoLoadOff,
        NothingToLoad
    };

    struct StartupChoice
    {
        std::string   path; // empty = start without a project
        StartupReason reason{ StartupReason::NothingToLoad };
    };

    // The single start-up decision. Safe mode never loads. Auto-load on and last project present: it. Otherwise the device
    // fallback (Demo 01 on the MPC) when it exists; otherwise the Projects screen.
    inline StartupChoice chooseStartupProject( const StartupInputs& in )
    {
        if( in.safeMode ) return { {}, StartupReason::SafeMode };
        if( in.autoLoad && !in.lastProject.empty() && in.lastExists ) return { in.lastProject, StartupReason::LastProject };
        if( !in.fallbackProject.empty() && in.fallbackExists ) return { in.fallbackProject, StartupReason::Fallback };
        return { {}, in.autoLoad ? StartupReason::NothingToLoad : StartupReason::AutoLoadOff };
    }

    inline const char* reasonText( StartupReason r )
    {
        switch( r )
        {
            case StartupReason::SafeMode: return "safe mode";
            case StartupReason::LastProject: return "last project";
            case StartupReason::Fallback: return "default project";
            case StartupReason::AutoLoadOff: return "auto load off";
            case StartupReason::NothingToLoad: return "no project to load";
        }
        return "";
    }
}
