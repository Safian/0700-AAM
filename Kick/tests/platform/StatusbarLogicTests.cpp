// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test of the status bar logic (Box Info message / standing state, text limit, tempo and time-signature texts).
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -o /tmp/t StatusbarLogicTests.cpp
#include "../../modules/kick_ui/dialogs/kick_statusbar_logic.h"
#include <cstdio>

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while( 0 )

using namespace kick_ui::statusbar_logic;

int main()
{
    // limitText: short text unchanged, long text cut with "...", exactly the limit unchanged, UTF-8 never split.
    CHECK( limitText( "Project saved" ) == "Project saved" );
    CHECK( limitText( std::string( 64, 'a' ) ) == std::string( 64, 'a' ) );
    CHECK( limitText( std::string( 65, 'a' ) ) == std::string( 64, 'a' ) + "..." );
    CHECK( limitText( "abcdef", 3 ) == "abc..." );
    CHECK( limitText( "ab cdef", 3 ) == "ab..." ); // no space before the dots
    CHECK( limitText( "" ).empty() );
    {
        const std::string e = "\xC3\xA9"; // é (2 bytes)
        CHECK( limitText( e + e + e + e, 2 ) == e + e + "..." );
        CHECK( limitText( e + e, 2 ) == e + e );
    }

    // BoxInfoState: a message shows for the hold time, then the standing state returns.
    {
        BoxInfoState s;
        CHECK( s.textAt( 0 ).empty() );
        s.setStanding( "Unsaved project changes" );
        CHECK( s.textAt( 0 ) == "Unsaved project changes" );
        s.setMessage( "Saved: Beat", 1000.0 );
        CHECK( s.textAt( 1000.0 ) == "Saved: Beat" );
        CHECK( s.textAt( 6999.0 ) == "Saved: Beat" );
        CHECK( s.textAt( 7000.0 ) == "Unsaved project changes" ); // faded back
        CHECK( s.textAt( 1e9 ) == "Unsaved project changes" );
        s.setStanding( "" );
        CHECK( s.textAt( 8000.0 ).empty() ); // the message is gone, nothing stands: empty line
        s.setMessage( "A", 0.0, 100.0 );
        s.setMessage( "B", 50.0, 100.0 ); // a newer message restarts the time
        CHECK( s.textAt( 120.0 ) == "B" && s.textAt( 150.0 ).empty() );
        s.setMessage( "", 200.0 ); // an empty message clears at once
        CHECK( s.textAt( 201.0 ).empty() );
        s.setStanding( "x" );
        s.setMessage( std::string( 200, 'z' ), 0.0 );
        CHECK( s.textAt( 1.0 ).size() == kMaxChars + 3 );
    }

    CHECK( chooseStanding( "AUDIO: x", "Low storage", "Unsaved", "Low memory" ) == "AUDIO: x" );
    CHECK( chooseStanding( "", "Low storage", "Unsaved", "Low memory" ) == "Low storage" );
    CHECK( chooseStanding( "", "", "Unsaved", "Low memory" ) == "Unsaved" );
    CHECK( chooseStanding( "", "", "", "Low memory" ) == "Low memory" );
    CHECK( chooseStanding( "", "", "", "" ).empty() );

    CHECK( tempoText( 120 ) == "120 BPM" && tempoText( 0 ) == "-- BPM" );
    CHECK( timeSignatureText( 6, 8 ) == "6/8" && timeSignatureText( 0, 4 ) == "--" );

    if( g_fail == 0 ) std::printf( "StatusbarLogicTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
