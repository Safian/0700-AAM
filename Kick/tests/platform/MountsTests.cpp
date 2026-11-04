// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test of the mount table logic, eject and format helpers.
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -o /tmp/t MountsTests.cpp
#include "../../modules/kick_ui/modes/filebrowser/kick_mounts.h"
#include <cstdio>

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while( 0 )

using namespace kick_mounts;

int main()
{
    const std::string table =
        "rootfs / rootfs rw 0 0\n"
        "/dev/mmcblk0p2 /data ext4 rw 0 0\n"
        "/dev/mmcblk1p1 /sdcard exfat rw,relatime 0 0\n"
        "/dev/sdb1 /media/PenDrive vfat rw 0 0\n"
        "/dev/sdb2 /media/PenDrive/EFI vfat rw 0 0\n"
        "/dev/sdc1 /media/My\\040Stick exfat rw 0 0\n"
        "/dev/sdc1 /media/My\\040Stick2 exfat rw 0 0\n"     // the same device twice
        "tmpfs /tmp tmpfs rw 0 0\n"
        "broken\n"
        "\n";
    const auto v = parse( table );
    CHECK( v.size() == 3 );
    CHECK( v[0].kind == Kind::Sd && v[0].mountPoint == "/sdcard" && v[0].device == "/dev/mmcblk1p1" && v[0].fsType == "exfat" );
    CHECK( v[1].kind == Kind::Usb && v[1].mountPoint == "/media/PenDrive" && v[1].label() == "PenDrive" );
    CHECK( v[2].mountPoint == "/media/My Stick" && v[2].label() == "My Stick" );

    // No external drive at all, empty text, only internal flash.
    CHECK( parse( "" ).empty() );
    CHECK( parse( "/dev/mmcblk0p2 /data ext4 rw 0 0\n" ).empty() );
    // Several partitions of one stick, each its own volume.
    CHECK( parse( "/dev/sdb1 /media/A vfat rw 0 0\n/dev/sdb3 /media/B vfat rw 0 0\n" ).size() == 2 );
    // Octal escapes.
    CHECK( unescape( "a\\040b\\134c" ) == "a b\\c" );
    CHECK( unescape( "x\\04" ) == "x\\04" ); // too short: left alone

    // Eject.
    int  syncs = 0;
    auto sync  = [&] { ++syncs; };
    CHECK( eject( "/sdcard", sync, []( const std::string& ) { return 0; } ) == EjectResult::Ok && syncs == 1 );
    CHECK( eject( "/sdcard", sync, []( const std::string& ) { return EBUSY; } ) == EjectResult::Busy );
    CHECK( eject( "/x", sync, []( const std::string& ) { return EINVAL; } ) == EjectResult::NotMounted );
    CHECK( eject( "/x", sync, []( const std::string& ) { return EPERM; } ) == EjectResult::Failed );
    CHECK( syncs == 4 ); // the data is flushed before every attempt

    // Format.
    CHECK( safeLabel( "my stick!" ) == "MYSTICK" );
    CHECK( safeLabel( "" ) == "KICK" );
    CHECK( safeLabel( "abcdefghijklmnop" ).size() == 11 );
    CHECK( formatTool( FsKind::ExFat, []( const std::string& t ) { return t == "mkfs.exfat"; } ) == "mkfs.exfat" );
    CHECK( formatTool( FsKind::ExFat, []( const std::string& t ) { return t == "mkexfatfs"; } ) == "mkexfatfs" );
    CHECK( formatTool( FsKind::Fat32, []( const std::string& ) { return false; } ).empty() );
    const auto argv = formatArgv( "mkfs.vfat", FsKind::Fat32, "/dev/sdb1", "a;rm -rf /" );
    CHECK( argv.size() == 6 && argv[4] == "ARMRF" && argv[5] == "/dev/sdb1" );
    CHECK( formattable( v[0] ) && !formattable( Volume{ "/dev/", "/x", "vfat", Kind::Usb } ) );

    std::printf( g_fail ? "FAILED %d\n" : "PASS mounts\n", g_fail );
    return g_fail ? 1 : 0;
}
