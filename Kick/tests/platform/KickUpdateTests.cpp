// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test of the Kick Update logic (VERSION parsing, comparison, package state, restart guards, USB drive lookup).
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -o /tmp/t KickUpdateTests.cpp
#include "../../modules/hardware/source/kick_sysinfo.h"
#include "../../modules/hardware/source/kick_update_logic.h"
#include <algorithm>
#include <cstdio>

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while( 0 )

using namespace kick_update;

static std::string ver( const char* v, const char* commit = "abc", const char* sha = "deadbeef" )
{
    return std::string( "VERSION=" ) + v + "\nGIT_COMMIT=" + commit + "\nBUILD_DATE=2026-11-04\nKICK_SHA256=" + sha + "\n";
}

int main()
{
    // SHA-256 known answers (FIPS 180-4), chunked updates equal one update.
    {
        Sha256 e;
        CHECK( e.hex() == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" );
        Sha256 a;
        a.update( "abc", 3 );
        CHECK( a.hex() == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" );
        Sha256 m;
        const std::string two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
        m.update( two.data(), two.size() );
        CHECK( m.hex() == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1" );
        Sha256 big, chunked;
        std::string million( 1000000, 'a' );
        big.update( million.data(), million.size() );
        for( size_t i = 0; i < million.size(); i += 777 )
            chunked.update( million.data() + i, std::min<size_t>( 777, million.size() - i ) );
        CHECK( big.hex() == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0" );
        CHECK( chunked.hex() == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0" );
    }

    // Parsing: CRLF, quotes, spaces, unknown keys, missing VERSION.
    const auto p = parseVersionText( "VERSION=\"1.2.3\"\r\nGIT_COMMIT = abc123 \r\nBUILD_DATE=2026-11-04\nKICK_SHA256=ff00\nOTHER=x\n" );
    CHECK( p.valid() && p.version == "1.2.3" && p.commit == "abc123" && p.buildDate == "2026-11-04" && p.sha256 == "ff00" );
    CHECK( !parseVersionText( "" ).valid() );
    CHECK( !parseVersionText( "GIT_COMMIT=abc\n" ).valid() );
    CHECK( !parseVersionText( "garbage without equals" ).valid() );

    // Comparison.
    CHECK( compareVersions( "1.2.3", "1.2.3" ) == 0 );
    CHECK( compareVersions( "1.2.4", "1.2.3" ) > 0 );
    CHECK( compareVersions( "1.10.0", "1.9.0" ) > 0 ); // numeric, not textual
    CHECK( compareVersions( "0.3.3", "0.3.10" ) < 0 );
    CHECK( compareVersions( "2.0", "1.9.9" ) > 0 );
    CHECK( compareVersions( "1.2", "1.2.0" ) == 0 );
    CHECK( compareVersions( "1.2.0-beta", "1.2.0" ) < 0 );
    CHECK( compareVersions( "1.2.0", "1.2.0-beta" ) > 0 );
    CHECK( compareVersions( "1.2.0-alpha", "1.2.0-beta" ) < 0 );
    CHECK( compareVersions( "", "0" ) == 0 );
    CHECK( compareVersions( "x.y", "0.0" ) == 0 ); // non-numeric reads as 0, no crash

    // States.
    Inputs in;
    CHECK( classify( in ) == State::NoDrive );
    in.driveFound = true;
    CHECK( classify( in ) == State::NoPackage );
    in.versionFileFound = true;
    in.binaryFound      = true;
    in.installedText    = ver( "1.0.0" );
    in.packageText      = ver( "1.1.0" );
    CHECK( classify( in ) == State::Newer );
    in.packageText = ver( "0.9.0" );
    CHECK( classify( in ) == State::Older );
    in.packageText = ver( "1.0.0", "other" );
    CHECK( classify( in ) == State::Different );
    in.packageText = in.installedText;
    CHECK( classify( in ) == State::UpToDate );
    in.installedText.clear();                       // Kick not installed yet
    in.packageText = ver( "1.0.0" );
    CHECK( classify( in ) == State::Newer );
    in.installedText = ver( "1.0.0" );

    std::string reason;
    in.packageText = "garbage";
    CHECK( classify( in, &reason ) == State::PackageBroken && !reason.empty() );
    in.packageText = ver( "1.1.0" );
    in.binaryFound = false;
    CHECK( classify( in, &reason ) == State::PackageBroken );
    in.binaryFound = true;
    in.packageText = "VERSION=1.1.0\n"; // no KICK_SHA256: the bootstrap would not verify, so it is not offered
    CHECK( classify( in, &reason ) == State::PackageBroken );
    in.packageText = ver( "1.1.0" );
    in.shaChecked = true;
    in.shaMatches = false;
    CHECK( classify( in, &reason ) == State::PackageBroken );
    in.shaMatches = true;
    CHECK( classify( in ) == State::Newer );

    CHECK( installable( State::Newer ) && installable( State::Older ) && installable( State::Different ) );
    CHECK( !installable( State::UpToDate ) && !installable( State::NoDrive ) && !installable( State::NoPackage ) && !installable( State::PackageBroken ) );

    // Texts.
    const auto i1 = parseVersionText( ver( "1.0.0", "aaa" ) );
    const auto p1 = parseVersionText( ver( "1.1.0", "bbb" ) );
    CHECK( describe( State::Newer, i1, p1 ) == "Update available: 1.0.0 -> 1.1.0" );
    CHECK( describe( State::Newer, PackageInfo(), p1 ) == "Install available: 1.1.0" );
    CHECK( describe( State::Older, p1, i1 ).find( "downgrade" ) != std::string::npos );
    CHECK( describe( State::NoDrive, i1, p1 ) == "No USB drive found" );

    // Restart guards.
    CHECK( restartBlockReason( false, false, false ).empty() );
    CHECK( !restartBlockReason( true, false, false ).empty() );
    CHECK( !restartBlockReason( false, true, false ).empty() );
    CHECK( !restartBlockReason( false, false, true ).empty() );

    // Several USB drives, an EFI partition, a stale entry.
    using namespace kick_sysinfo;
    const auto mounts = parseMounts( "/dev/sdb1 /media/A vfat rw 0 0\n/dev/sdb2 /media/A/EFI vfat rw 0 0\n/dev/sdc1 /media/B exfat rw 0 0\n/dev/sdd1 /media/gone exfat rw 0 0\n" );
    const auto drives = findVolumeMounts( mounts, Volume::Usb, []( const std::string& m ) { return m != "/media/gone"; } );
    CHECK( drives.size() == 2 && drives[0] == "/media/A" && drives[1] == "/media/B" );
    CHECK( findVolumeMounts( parseMounts( "" ), Volume::Usb, nullptr ).empty() );

    // Release notes excerpt (SW-15).
    CHECK( changelogExcerpt( "" ).empty() && changelogExcerpt( "\n \n" ).empty() );
    CHECK( changelogExcerpt( "Fixes the Menu.\n\nAdds rollback.\r\n" ) == "Fixes the Menu.\nAdds rollback." );
    CHECK( changelogExcerpt( "1\n2\n3\n4\n5\n6" ) == "1\n2\n3\n4" );
    CHECK( changelogExcerpt( "1\n2\n3", 2 ) == "1\n2" );
    CHECK( changelogExcerpt( std::string( 100, 'a' ), 4, 10 ) == std::string( 7, 'a' ) + "..." );
    CHECK( changelogExcerpt( std::string( 10, 'a' ), 4, 10 ) == std::string( 10, 'a' ) );

    if( g_fail == 0 ) std::printf( "KickUpdateTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
