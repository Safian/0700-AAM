// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE-free) system-resource helpers: /proc/stat CPU delta, /proc/meminfo, /proc/mounts volume lookup and a statvfs
// wrapper. Host-tested in Kick/tests/platform/SysInfoTests.cpp.
#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <sstream>
#include <string>
#include <sys/statvfs.h>
#include <vector>

namespace kick_sysinfo
{
    // ---- CPU -------------------------------------------------------------------------------------------------------
    struct CpuTimes
    {
        unsigned long long busy  = 0;
        unsigned long long total = 0;
        bool               valid = false;
    };

    // First "cpu " line of /proc/stat: user nice system idle iowait irq softirq steal guest guest_nice. guest and guest_nice are
    // already part of user / nice, so they are not added again; iowait counts as idle (the CPU waits, it does not work).
    inline CpuTimes parseProcStatCpu( const std::string& text )
    {
        CpuTimes           out;
        std::istringstream lines( text );
        std::string        line;
        while( std::getline( lines, line ) )
        {
            if( line.compare( 0, 4, "cpu " ) != 0 )
                continue;
            std::istringstream iss( line.substr( 4 ) );
            unsigned long long v[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
            int                n    = 0;
            while( n < 8 && ( iss >> v[n] ) )
                ++n;
            if( n < 4 )
                return out; // user nice system idle are the minimum
            const unsigned long long idle = v[3] + v[4];
            out.total                     = v[0] + v[1] + v[2] + v[3] + v[4] + v[5] + v[6] + v[7];
            out.busy                      = out.total - idle;
            out.valid                     = true;
            return out;
        }
        return out;
    }

    // Load between two samples in percent (0..100); negative when there is no usable interval yet.
    inline double cpuPercent( const CpuTimes& prev, const CpuTimes& cur )
    {
        if( !prev.valid || !cur.valid || cur.total <= prev.total || cur.busy < prev.busy )
            return -1.0;
        const double d = (double)( cur.total - prev.total );
        return std::min( 100.0, std::max( 0.0, 100.0 * (double)( cur.busy - prev.busy ) / d ) );
    }

    // ---- Memory ----------------------------------------------------------------------------------------------------
    struct MemInfo
    {
        unsigned long long totalKb     = 0;
        unsigned long long availableKb = 0;
        bool               valid       = false;
    };

    // MemAvailable is what can be handed out without swapping (page cache counts as free); MemFree + Buffers + Cached is the
    // fallback of kernels that do not report it.
    inline MemInfo parseMemInfo( const std::string& text )
    {
        unsigned long long total = 0, avail = 0, freeKb = 0, buffers = 0, cached = 0;
        bool               haveTotal = false, haveAvail = false;
        std::istringstream lines( text );
        std::string        line;
        while( std::getline( lines, line ) )
        {
            const auto colon = line.find( ':' );
            if( colon == std::string::npos )
                continue;
            const std::string  key = line.substr( 0, colon );
            std::istringstream iss( line.substr( colon + 1 ) );
            unsigned long long value = 0;
            if( !( iss >> value ) )
                continue;
            if( key == "MemTotal" ) { total = value; haveTotal = true; }
            else if( key == "MemAvailable" ) { avail = value; haveAvail = true; }
            else if( key == "MemFree" ) freeKb = value;
            else if( key == "Buffers" ) buffers = value;
            else if( key == "Cached" ) cached = value;
        }
        MemInfo out;
        if( !haveTotal || total == 0 )
            return out;
        out.totalKb     = total;
        out.availableKb = std::min( total, haveAvail ? avail : freeKb + buffers + cached );
        out.valid       = true;
        return out;
    }

    // Used percentage (0..100), 0 when total is 0.
    inline int percentUsed( double used, double total )
    {
        if( total <= 0.0 )
            return 0;
        return (int)std::min( 100.0, std::max( 0.0, 100.0 * used / total + 0.5 ) );
    }

    // ---- Mounts ----------------------------------------------------------------------------------------------------
    struct Mount
    {
        std::string device;
        std::string mountPoint;
        std::string fsType;
    };

    inline std::string unescapeMountField( const std::string& s )
    {
        std::string out;
        for( size_t i = 0; i < s.size(); ++i )
        {
            if( s[i] == '\\' && i + 3 < s.size() && s[i + 1] >= '0' && s[i + 1] <= '3' && s[i + 2] >= '0' && s[i + 2] <= '7' && s[i + 3] >= '0' && s[i + 3] <= '7' )
            {
                out += (char)( ( s[i + 1] - '0' ) * 64 + ( s[i + 2] - '0' ) * 8 + ( s[i + 3] - '0' ) );
                i += 3;
            }
            else
                out += s[i];
        }
        return out;
    }

    // /proc/mounts: "device mountpoint fstype options dump pass"; spaces in the fields are written as \040.
    inline std::vector<Mount> parseMounts( const std::string& text )
    {
        std::vector<Mount> out;
        std::istringstream lines( text );
        std::string        line;
        while( std::getline( lines, line ) )
        {
            std::istringstream iss( line );
            std::string        dev, mnt, fs;
            if( !( iss >> dev >> mnt >> fs ) )
                continue;
            out.push_back( { unescapeMountField( dev ), unescapeMountField( mnt ), fs } );
        }
        return out;
    }

    enum class Volume { Internal, SdCard, Usb };

    using ExistsFn = std::function<bool( const std::string& )>;

    // Mount point of a volume or "" when it is not there. The MPC: internal storage /data; SD card /dev/mmcblk1* (mounted at
    // /sdcard); USB drives /dev/sd* (mounted at /media/<label>). A mount point that only holds a boot partition (.../EFI) is
    // skipped. `exists` tells whether the mount point is a directory (a stale table entry is ignored).
    inline std::string findVolumeMount( const std::vector<Mount>& mounts, Volume vol, const ExistsFn& exists )
    {
        auto ok = [&]( const std::string& p ) { return !p.empty() && ( !exists || exists( p ) ); };
        auto endsWith = []( const std::string& s, const char* suffix ) {
            const std::string x( suffix );
            return s.size() >= x.size() && s.compare( s.size() - x.size(), x.size(), x ) == 0;
        };
        for( const auto& m : mounts )
        {
            switch( vol )
            {
                case Volume::Internal:
                    if( m.mountPoint == "/data" && ok( m.mountPoint ) ) return m.mountPoint;
                    break;
                case Volume::SdCard:
                    if( m.device.compare( 0, 12, "/dev/mmcblk1" ) == 0 && !endsWith( m.mountPoint, "/EFI" ) && ok( m.mountPoint ) ) return m.mountPoint;
                    break;
                case Volume::Usb:
                    if( m.device.compare( 0, 7, "/dev/sd" ) == 0 && !endsWith( m.mountPoint, "/EFI" ) && ok( m.mountPoint ) ) return m.mountPoint;
                    break;
            }
        }
        return {};
    }

    // All mount points of a volume kind (several USB drives / partitions), in table order.
    inline std::vector<std::string> findVolumeMounts( const std::vector<Mount>& mounts, Volume vol, const ExistsFn& exists )
    {
        std::vector<std::string> out;
        std::vector<Mount>       rest = mounts;
        // findVolumeMount returns the first hit: remove it and ask again.
        for( ;; )
        {
            const auto hit = findVolumeMount( rest, vol, exists );
            if( hit.empty() )
                break;
            out.push_back( hit );
            rest.erase( std::remove_if( rest.begin(), rest.end(), [&]( const Mount& m ) { return m.mountPoint == hit; } ), rest.end() );
        }
        return out;
    }

    // ---- Disk usage ------------------------------------------------------------------------------------------------
    struct DiskUsage
    {
        bool   ok        = false;
        double totalMb   = 0.0;
        double usedMb    = 0.0;
        double availMb   = 0.0;
        int    percent   = 0; // used share of the space a user can fill (df semantics)
    };

    inline DiskUsage diskUsage( const std::string& path )
    {
        DiskUsage out;
        struct statvfs sv;
        if( path.empty() || ::statvfs( path.c_str(), &sv ) != 0 || sv.f_blocks == 0 )
            return out;
        const double unit  = (double)sv.f_frsize / ( 1024.0 * 1024.0 );
        out.totalMb        = (double)sv.f_blocks * unit;
        out.availMb        = (double)sv.f_bavail * unit;
        const double freeMb = (double)sv.f_bfree * unit;
        out.usedMb         = out.totalMb - freeMb;
        out.percent        = percentUsed( out.usedMb, out.usedMb + out.availMb );
        out.ok             = true;
        return out;
    }

    // ---- Warnings for the status bar -------------------------------------------------------------------------------
    constexpr int    kDiskWarnPercent   = 95;     // used share of the internal storage at which saving gets risky (< 5 % free)
    constexpr double kDiskWarnMinFreeMb = 2048.0; // do not cry low-storage on multi-hundred-GB drives that still have >= 2 GB free
    constexpr int    kRamWarnPercent    = 92;     // used share of the memory at which a big project load may fail

    struct ResourceWarnings
    {
        std::string disk, ram; // empty = fine
    };

    // The internal storage and the memory against the limits above. A value that could not be read never warns.
    inline ResourceWarnings resourceWarnings( bool haveDisk, int diskPercent, double diskFreeMb, bool haveRam, int ramPercent )
    {
        ResourceWarnings w;
        if( haveDisk && diskPercent >= kDiskWarnPercent && diskFreeMb < kDiskWarnMinFreeMb )
            w.disk = "Low storage: " + std::to_string( (long long)( diskFreeMb < 0.0 ? 0.0 : diskFreeMb ) ) + " MB free";
        if( haveRam && ramPercent >= kRamWarnPercent )
            w.ram = "Low memory: " + std::to_string( ramPercent ) + " % used";
        return w;
    }

    // ---- The CPU badge on the header's Resources icon (HDR-23) -------------------------------------------------------
    constexpr double kCpuCritical = 85.0; // percent of the CPU at which the badge turns red

    // "23%" (rounded, 0..100); empty when the CPU could not be read.
    inline std::string cpuBadgeText( bool haveCpu, double cpuPercent )
    {
        if( !haveCpu || cpuPercent < 0.0 )
            return {};
        const int v = (int)( cpuPercent + 0.5 );
        return std::to_string( v > 100 ? 100 : v ) + "%";
    }

    // The badge is red when the CPU is nearly used up or the memory is short.
    inline bool cpuBadgeCritical( bool haveCpu, double cpuPercent, bool haveRam, int ramPercent )
    {
        return ( haveCpu && cpuPercent >= kCpuCritical ) || ( haveRam && ramPercent >= kRamWarnPercent );
    }
}
