// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test of the system-resource helpers (CPU delta, MemAvailable, mount parsing, volume lookup, statvfs wrapper).
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -o /tmp/t SysInfoTests.cpp
#include "../../modules/hardware/source/kick_sysinfo.h"
#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while( 0 )

using namespace kick_sysinfo;

int main()
{
    // CPU: guest is part of user, iowait counts as idle, the load comes from the difference of two samples.
    const auto a = parseProcStatCpu( "cpu  100 0 100 800 0 0 0 0 0 0\ncpu0 1 1 1 1 1 1 1 1 1 1\nintr 5\n" );
    const auto b = parseProcStatCpu( "cpu  150 0 150 900 0 0 0 0 0 0\n" );
    CHECK( a.valid && b.valid );
    CHECK( a.total == 1000 && a.busy == 200 );
    CHECK( std::fabs( cpuPercent( a, b ) - 50.0 ) < 1e-9 ); // 100 busy of 200
    // iowait is idle: 50 busy, 150 iowait -> 25 %
    const auto c1 = parseProcStatCpu( "cpu  0 0 0 0 0 0 0 0\n" );
    const auto c2 = parseProcStatCpu( "cpu  25 0 25 0 150 0 0 0 500 500\n" ); // guest columns ignored
    CHECK( std::fabs( cpuPercent( c1, c2 ) - 25.0 ) < 1e-9 );
    CHECK( cpuPercent( a, a ) < 0.0 );                        // no interval
    CHECK( cpuPercent( CpuTimes(), b ) < 0.0 );               // first sample
    CHECK( cpuPercent( b, a ) < 0.0 );                        // counters went backwards (reset)
    CHECK( !parseProcStatCpu( "" ).valid );
    CHECK( !parseProcStatCpu( "cpu  1 2\n" ).valid );
    CHECK( !parseProcStatCpu( "cpu0 1 2 3 4\n" ).valid );     // only the aggregate line counts
    {
        CpuTimes x{ 0, 100, true }, y{ 100, 200, true };      // fully busy
        CHECK( cpuPercent( x, y ) == 100.0 );
    }

    // Memory.
    const auto m = parseMemInfo( "MemTotal:        1000000 kB\nMemFree:          100000 kB\nMemAvailable:     600000 kB\nBuffers: 10 kB\nCached: 20 kB\n" );
    CHECK( m.valid && m.totalKb == 1000000 && m.availableKb == 600000 );
    const auto m2 = parseMemInfo( "MemTotal: 1000 kB\nMemFree: 100 kB\nBuffers: 50 kB\nCached: 150 kB\n" ); // no MemAvailable
    CHECK( m2.valid && m2.availableKb == 300 );
    const auto m3 = parseMemInfo( "MemTotal: 1000 kB\nMemAvailable: 5000 kB\n" ); // clamped to total
    CHECK( m3.availableKb == 1000 );
    CHECK( !parseMemInfo( "" ).valid );
    CHECK( !parseMemInfo( "MemFree: 5 kB\n" ).valid );
    CHECK( percentUsed( 400000, 1000000 ) == 40 );
    CHECK( percentUsed( 1, 0 ) == 0 );
    CHECK( percentUsed( 2000, 1000 ) == 100 );

    // Mount table (MPC-like), with an escaped space and an EFI partition.
    const std::string mounts =
        "rootfs / rootfs rw 0 0\n"
        "/dev/mmcblk0p3 /data ext4 rw 0 0\n"
        "/dev/mmcblk1p1 /sdcard vfat rw 0 0\n"
        "/dev/sdb1 /media/Pen\\040Drive exfat rw 0 0\n"
        "/dev/sdb2 /media/Pen\\040Drive/EFI vfat rw 0 0\n"
        "tmpfs /tmp tmpfs rw 0 0\n";
    const auto mt = parseMounts( mounts );
    CHECK( mt.size() == 6 );
    CHECK( mt[3].mountPoint == "/media/Pen Drive" );
    auto all = []( const std::string& ) { return true; };
    CHECK( findVolumeMount( mt, Volume::Internal, all ) == "/data" );
    CHECK( findVolumeMount( mt, Volume::SdCard, all ) == "/sdcard" );
    CHECK( findVolumeMount( mt, Volume::Usb, all ) == "/media/Pen Drive" );
    // EFI-only USB stick: nothing to show.
    const auto efiOnly = parseMounts( "/dev/sdb1 /media/x/EFI vfat rw 0 0\n" );
    CHECK( findVolumeMount( efiOnly, Volume::Usb, all ).empty() );
    // Stale entry (mount point gone) is ignored.
    auto none = []( const std::string& ) { return false; };
    CHECK( findVolumeMount( mt, Volume::SdCard, none ).empty() );
    // No SD card, no USB.
    const auto bare = parseMounts( "/dev/mmcblk0p3 /data ext4 rw 0 0\n" );
    CHECK( findVolumeMount( bare, Volume::SdCard, all ).empty() );
    CHECK( findVolumeMount( bare, Volume::Usb, all ).empty() );
    CHECK( findVolumeMount( parseMounts( "" ), Volume::Internal, all ).empty() );
    CHECK( parseMounts( "garbage\n\n" ).empty() );
    CHECK( unescapeMountField( "a\\040b\\134c" ) == "a b\\c" );
    CHECK( unescapeMountField( "x\\04" ) == "x\\04" ); // truncated escape is kept

    // statvfs wrapper.
    const auto du = diskUsage( "/" );
    CHECK( du.ok && du.totalMb > 0.0 && du.percent >= 0 && du.percent <= 100 );
    CHECK( du.availMb <= du.totalMb );
    CHECK( !diskUsage( "/definitely/not/here" ).ok );
    CHECK( !diskUsage( "" ).ok );

    if( g_fail == 0 ) std::printf( "SysInfoTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
