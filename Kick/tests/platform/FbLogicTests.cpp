// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test of the file browser decisions (asset status, held pads, pad text).
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -o /tmp/t FbLogicTests.cpp
#include "../../modules/kick_ui/modes/filebrowser/kick_fbLogic.h"
#include <cstdio>

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while( 0 )

using namespace kick_fblogic;

int main()
{
    // engine codes: 0 exists, 1 added, 2 removed, 3 invalid
    CHECK( mapAssetResult( 1, true, 0 ) == AssetStatus::Added );
    CHECK( mapAssetResult( 0, true, 0 ) == AssetStatus::AlreadyExists );
    CHECK( mapAssetResult( 3, true, 0 ) == AssetStatus::Invalid );   // the old code counted this as a success
    CHECK( mapAssetResult( 2, true, 0 ) == AssetStatus::Removed );
    CHECK( mapAssetResult( 1, false, 0 ) == AssetStatus::NoProject );
    CHECK( mapAssetResult( 1, true, -1 ) == AssetStatus::NoProgram );
    CHECK( mapAssetResult( 99, true, 0 ) == AssetStatus::Invalid );
    CHECK( assignsToPad( AssetStatus::Added ) && assignsToPad( AssetStatus::AlreadyExists ) );
    CHECK( !assignsToPad( AssetStatus::Invalid ) && !assignsToPad( AssetStatus::NoProject ) && !assignsToPad( AssetStatus::NoProgram ) && !assignsToPad( AssetStatus::Removed ) );
    CHECK( describe( AssetStatus::Added, "kick", "A03" ) == "kick loaded on A03" );
    CHECK( describe( AssetStatus::Invalid, "x", "A01" ).find( "cannot be read" ) != std::string::npos );

    CHECK( padText( 0, 0 ) == "A01" && padText( 2, 0 ) == "A03" && padText( 15, 16 ) == "B16" && padText( 9, 48 ) == "D10" );
    CHECK( padText( -1, 0 ) == "A01" && padText( 16, 0 ) == "A01" );
    std::array<bool, 16> a{};
    CHECK( firstFreePad( a ) == 0 );
    a[0] = a[1] = true;
    CHECK( firstFreePad( a ) == 2 );
    a.fill( true );
    CHECK( firstFreePad( a ) == 0 );

    // Pad release: the held note ends, even after a bank change; a pad that was not held ends nothing.
    HeldPads h;
    CHECK( h.press( 3, 3 ) == 3 );
    CHECK( h.press( 3, 19 ) == 3 ); // repeated press keeps the first note
    CHECK( h.release( 3 ) == 3 );
    CHECK( h.release( 3 ) == -1 );
    CHECK( h.press( 4, 4 ) == 4 );
    CHECK( h.press( 5, 21 ) == 21 );
    CHECK( h.release( 4 ) == 4 ); // released in another bank: still note 4
    auto rest = h.releaseAll();
    CHECK( rest.size() == 1 && rest[0] == 21 );
    CHECK( h.release( 99 ) == -1 );

    CHECK( padAction( true, true ) == PadAction::NoteOn );
    CHECK( padAction( true, false ) == PadAction::NoteOff ); // the release used to be lost
    CHECK( padAction( false, true ) == PadAction::None );

    CHECK( listingStatus( false, 0, false, false ) == "This folder cannot be read" );
    CHECK( listingStatus( true, 0, false, false ) == "Empty folder" );
    CHECK( listingStatus( true, 0, true, false ) == "Nothing found" );
    CHECK( listingStatus( true, 500, true, true ).find( "500" ) != std::string::npos );
    CHECK( listingStatus( true, 3, false, false ).empty() );

    std::printf( g_fail ? "FAILED %d\n" : "PASS fb logic\n", g_fail );
    return g_fail ? 1 : 0;
}
