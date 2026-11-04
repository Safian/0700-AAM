// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE-free) mount table logic for the file browser: /proc/mounts parsing (SD card, USB sticks, several partitions,
// "\040" escapes, boot partitions skipped), the real eject (sync + umount with a busy / not mounted / failed answer) and the
// command line of a drive format. The system calls are injected so everything is host-tested:
// Kick/tests/platform/MountsTests.cpp.
#pragma once

#include <cerrno>
#include <functional>
#include <set>
#include <string>
#include <vector>
#if defined( __linux__ )
    #include <sys/mount.h>
    #include <unistd.h>
#endif

namespace kick_mounts
{
    enum class Kind { Sd, Usb };

    struct Volume
    {
        std::string device;     // /dev/sdb1
        std::string mountPoint; // /media/PenDrive
        std::string fsType;     // vfat, exfat, ext4 ...
        Kind        kind{ Kind::Usb };
        std::string label() const // the last path component of the mount point
        {
            const auto s = mountPoint.find_last_of( '/' );
            const auto l = s == std::string::npos ? mountPoint : mountPoint.substr( s + 1 );
            return l.empty() ? mountPoint : l;
        }
    };

    // "\040" (space), "\011" (tab), "\012", "\134" (backslash) as written by the kernel.
    inline std::string unescape( const std::string& s )
    {
        std::string out;
        for( size_t i = 0; i < s.size(); ++i )
        {
            if( s[i] == '\\' && i + 3 < s.size() + 0 && s[i + 1] >= '0' && s[i + 1] <= '7' && s[i + 2] >= '0' && s[i + 2] <= '7' && s[i + 3] >= '0' && s[i + 3] <= '7' )
            {
                out += static_cast<char>( ( s[i + 1] - '0' ) * 64 + ( s[i + 2] - '0' ) * 8 + ( s[i + 3] - '0' ) );
                i += 3;
            }
            else
                out += s[i];
        }
        return out;
    }

    inline bool startsWith( const std::string& s, const std::string& p ) { return s.compare( 0, p.size(), p ) == 0; }
    inline bool endsWith( const std::string& s, const std::string& e ) { return s.size() >= e.size() && s.compare( s.size() - e.size(), e.size(), e ) == 0; }

    // SD card = /dev/mmcblk1* (mmcblk0 is the internal flash), USB = /dev/sd* (the MPC numbers its drives from sdb). Boot partitions
    // (a mount point ending in /EFI) and a second mount of the same device are skipped. Every partition is listed.
    inline std::vector<Volume> parse( const std::string& procMounts )
    {
        std::vector<Volume> out;
        size_t              pos = 0;
        while( pos < procMounts.size() )
        {
            auto nl = procMounts.find( '\n', pos );
            if( nl == std::string::npos )
                nl = procMounts.size();
            const auto line = procMounts.substr( pos, nl - pos );
            pos             = nl + 1;
            std::vector<std::string> f;
            size_t                   a = 0;
            while( a < line.size() && f.size() < 3 )
            {
                const auto sp = line.find( ' ', a );
                f.push_back( line.substr( a, sp == std::string::npos ? std::string::npos : sp - a ) );
                if( sp == std::string::npos )
                    break;
                a = sp + 1;
            }
            if( f.size() < 3 )
                continue;
            Volume v;
            v.device     = f[0];
            v.mountPoint = unescape( f[1] );
            v.fsType     = f[2];
            if( startsWith( v.device, "/dev/mmcblk1" ) )
                v.kind = Kind::Sd;
            else if( startsWith( v.device, "/dev/sd" ) )
                v.kind = Kind::Usb;
            else
                continue;
            if( endsWith( v.mountPoint, "/EFI" ) || v.mountPoint.empty() || v.mountPoint[0] != '/' )
                continue;
            bool dup = false;
            for( const auto& o : out )
                dup = dup || o.device == v.device || o.mountPoint == v.mountPoint;
            if( !dup )
                out.push_back( v );
        }
        return out;
    }

    enum class EjectResult { Ok, Busy, NotMounted, Failed };

    inline const char* describe( EjectResult r )
    {
        switch( r )
        {
            case EjectResult::Ok: return "Safe to remove";
            case EjectResult::Busy: return "The drive is in use (a project or sample on it is open): it cannot be ejected";
            case EjectResult::NotMounted: return "The drive is not mounted";
            case EjectResult::Failed: return "The drive could not be ejected";
        }
        return "";
    }

    // sync + umount (no lazy / forced unmount: a drive that is in use answers "busy"). `umountFn` returns 0 or an errno value.
    inline EjectResult eject( const std::string& mountPoint, const std::function<void()>& syncFn, const std::function<int( const std::string& )>& umountFn )
    {
        if( syncFn )
            syncFn();
        const int e = umountFn( mountPoint );
        if( e == 0 )
            return EjectResult::Ok;
        if( e == EBUSY )
            return EjectResult::Busy;
        if( e == EINVAL || e == ENOENT )
            return EjectResult::NotMounted; // not a mount point (any more)
        return EjectResult::Failed;
    }

    inline int realUmount( const std::string& mountPoint )
    {
#if defined( __linux__ )
        return ::umount2( mountPoint.c_str(), 0 ) == 0 ? 0 : errno;
#else
        (void)mountPoint;
        return ENOSYS;
#endif
    }

    inline void realSync()
    {
#if defined( __linux__ )
        ::sync();
#endif
    }

    // ---- Format
    enum class FsKind { ExFat, Fat32 };

    inline const char* fsName( FsKind k ) { return k == FsKind::ExFat ? "exFAT" : "FAT32"; }

    // The device node that gets formatted: the partition the volume is mounted from. Never an internal flash device and never the
    // whole disk of a mounted partition.
    inline bool formattable( const Volume& v ) { return ( v.kind == Kind::Sd || v.kind == Kind::Usb ) && v.device.size() > 5; }

    // Label: letters and digits only, 11 characters at most (FAT limit), upper case.
    inline std::string safeLabel( const std::string& s )
    {
        std::string out;
        for( char c : s )
        {
            if( ( c >= 'a' && c <= 'z' ) )
                c = static_cast<char>( c - 'a' + 'A' );
            if( ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) )
                out += c;
            if( out.size() == 11 )
                break;
        }
        return out.empty() ? "KICK" : out;
    }

    // Tool name for the file system, "" when unsupported. `which` tells whether a program exists on this device.
    inline std::string formatTool( FsKind k, const std::function<bool( const std::string& )>& which )
    {
        const char* candidates[] = { k == FsKind::ExFat ? "mkfs.exfat" : "mkfs.vfat", k == FsKind::ExFat ? "mkexfatfs" : "mkfs.fat" };
        for( const char* c : candidates )
            if( which( c ) )
                return c;
        return {};
    }

    // argv for the format (no shell: the device and label never reach a shell).
    inline std::vector<std::string> formatArgv( const std::string& tool, FsKind k, const std::string& device, const std::string& label )
    {
        const auto l = safeLabel( label );
        if( k == FsKind::ExFat )
            return { tool, "-n", l, device }; // mkfs.exfat / mkexfatfs
        return { tool, "-F", "32", "-n", l, device };
    }

    // ---- Keeping the browser's drive list in step with the mount table (1 Hz).
    struct Diff
    {
        std::vector<Volume>      add;            // new drives to show
        std::vector<std::string> removeDevices;  // drives that went away
    };

    // `known` = devices the browser shows now, `ejected` = drives the user ejected: they stay hidden (an automounter may mount them
    // again at once) until they have left the mount table once (unplugged); then they may appear again.
    inline Diff reconcile( const std::vector<Volume>& now, const std::vector<std::string>& known, std::set<std::string>& ejected )
    {
        Diff d;
        for( const auto& v : now )
        {
            if( ejected.count( v.device ) )
                continue;
            bool isKnown = false;
            for( const auto& k : known )
                isKnown = isKnown || k == v.device;
            if( !isKnown )
                d.add.push_back( v );
        }
        for( const auto& k : known )
        {
            bool still = false;
            for( const auto& v : now )
                still = still || v.device == k;
            if( !still )
                d.removeDevices.push_back( k );
        }
        for( auto it = ejected.begin(); it != ejected.end(); )
        {
            bool still = false;
            for( const auto& v : now )
                still = still || v.device == *it;
            it = still ? std::next( it ) : ejected.erase( it );
        }
        return d;
    }
}
