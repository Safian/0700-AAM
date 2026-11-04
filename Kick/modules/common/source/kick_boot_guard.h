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

// Safe mode / boot-loop guard. JUCE-free, header only (host test: tests/platform/StartupTests.cpp).
//
// The crash handler (Main.cpp) writes /data/kick.crash. On every start Kick increments a counter file
// (/data/kick.boot-count, written atomically); a start that stayed up for kStableSeconds deletes it. So the counter is
// non-zero only if the previous start(s) ended before they were stable. When the counter already says that the last two
// starts did not become stable AND a crash file exists, this start is a safe-mode start: no project is loaded and the user
// is told why and where the crash file is.

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

namespace kick_boot
{
    constexpr int kSafeModeThreshold = 2; // unstable starts in a row (counter value before this start) that trigger safe mode
    constexpr int kStableSeconds     = 30;
    constexpr int kCounterCap        = 99;

    struct Paths
    {
        std::string countFile{ "/data/kick.boot-count" };
        std::string crashFile{ "/data/kick.crash" };

        static Paths inDir( const std::string& dir )
        {
            Paths p;
            p.countFile = dir + "/kick.boot-count";
            p.crashFile = dir + "/kick.crash";
            return p;
        }
    };

    struct Decision
    {
        bool        safeMode{ false };
        int         previousCount{ 0 };   // unstable starts before this one
        bool        crashFileExists{ false };
        bool        counterWritten{ true }; // false: the counter could not be saved, so a boot loop could not be detected
        std::string crashPath;               // set in safe mode
        std::string reason;                  // human readable, for the log and the dialog
    };

    inline bool fileExists( const std::string& path )
    {
        struct stat st {};
        return ::stat( path.c_str(), &st ) == 0;
    }

    // Counter file: one decimal number. Missing, empty or garbage = 0.
    inline int readCount( const std::string& path )
    {
        std::ifstream in( path );
        if( !in ) return 0;
        long v = 0;
        in >> v;
        if( !in || v < 0 ) return 0;
        return v > kCounterCap ? kCounterCap : (int)v;
    }

    // Atomic replace: write <path>.tmp, fsync, rename over <path>. A power cut leaves the old or the new file, never half of one.
    inline bool writeFileAtomic( const std::string& path, const std::string& contents )
    {
        const std::string tmp = path + ".tmp";
        const int         fd  = ::open( tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644 );
        if( fd < 0 ) return false;
        size_t done = 0;
        while( done < contents.size() )
        {
            const auto n = ::write( fd, contents.data() + done, contents.size() - done );
            if( n <= 0 )
            {
                ::close( fd );
                ::unlink( tmp.c_str() );
                return false;
            }
            done += (size_t)n;
        }
        const bool synced = ( ::fsync( fd ) == 0 );
        ::close( fd );
        if( !synced || ::rename( tmp.c_str(), path.c_str() ) != 0 )
        {
            ::unlink( tmp.c_str() );
            return false;
        }
        return true;
    }

    // Pure decision from the inputs (no file access).
    inline bool shouldEnterSafeMode( int previousCount, bool crashFileExists )
    {
        return crashFileExists && previousCount >= kSafeModeThreshold;
    }

    // Call once at start, before anything that can crash. Increments the counter (atomically).
    inline Decision onBoot( const Paths& paths )
    {
        Decision d;
        d.previousCount   = readCount( paths.countFile );
        d.crashFileExists = fileExists( paths.crashFile );
        d.safeMode        = shouldEnterSafeMode( d.previousCount, d.crashFileExists );
        const int next    = d.previousCount >= kCounterCap ? kCounterCap : d.previousCount + 1;
        d.counterWritten  = writeFileAtomic( paths.countFile, std::to_string( next ) + "\n" );
        if( d.safeMode )
        {
            d.crashPath = paths.crashFile;
            d.reason    = "the previous " + std::to_string( d.previousCount ) + " starts did not stay up for " + std::to_string( kStableSeconds ) + " s and a crash file exists";
        }
        return d;
    }

    // The start stayed up long enough: forget the unstable starts.
    inline bool markStable( const Paths& paths )
    {
        return ::unlink( paths.countFile.c_str() ) == 0 || !fileExists( paths.countFile );
    }
}
