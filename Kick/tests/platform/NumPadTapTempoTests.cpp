// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test of the numpad entry state and the tap tempo estimator.
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -o /tmp/t NumPadTapTempoTests.cpp
#include "../../modules/kick_ui_controls/source/kick_numpad_entry.h"
#include "../../modules/kick_ui_controls/source/kick_tap_tempo.h"
#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while( 0 )
static bool near( double a, double b, double eps = 1e-9 ) { return std::fabs( a - b ) < eps; }

int main()
{
    using ui::NumPadEntry;
    using ui::TapTempo;

    // Digits, no leading zeros, digit limit.
    {
        NumPadEntry e;
        CHECK( e.isEmpty() && e.value() == 0.0 );
        e.digit( 0 ); e.digit( 0 ); e.digit( 5 );
        CHECK( e.text() == "5" && e.value() == 5.0 );
        e.digit( 1 ); e.digit( 2 );
        CHECK( e.value() == 512.0 );
        for( int i = 0; i < 20; ++i ) e.digit( 9 );
        CHECK( e.text().size() == NumPadEntry::kMaxDigits );
        e.digit( -1 ); e.digit( 10 );
        CHECK( e.text().size() == NumPadEntry::kMaxDigits );
    }
    // Decimal point only for decimal fields, once.
    {
        NumPadEntry e;
        e.dot( false );
        CHECK( e.isEmpty() );
        e.dot( true );
        CHECK( e.text() == "0." );
        e.digit( 2 ); e.digit( 5 );
        e.dot( true );
        CHECK( e.text() == "0.25" && near( e.value(), 0.25 ) );
        NumPadEntry i;
        i.digit( 1 ); i.dot( false );
        CHECK( i.text() == "1" );
        NumPadEntry f;
        f.digit( 1 ); f.dot( true ); f.digit( 5 );
        CHECK( near( f.value(), 1.5 ) );
    }
    // Sign: toggles any number of times (the old pad stayed disabled for good), applies to the typed value.
    {
        NumPadEntry e;
        e.digit( 4 ); e.digit( 2 );
        e.toggleSign();
        CHECK( e.value() == -42.0 && e.text() == "-42" );
        e.toggleSign();
        CHECK( e.value() == 42.0 );
        e.toggleSign(); e.backspace(); e.backspace();
        CHECK( e.isEmpty() && e.value() == 0.0 ); // the sign goes with the last digit
        NumPadEntry s;
        s.toggleSign();
        CHECK( s.text() == "-" && s.value() == 0.0 ); // sign first, digits after
        s.digit( 7 );
        CHECK( s.value() == -7.0 );
    }
    // Backspace and clear.
    {
        NumPadEntry e;
        e.digit( 1 ); e.digit( 2 ); e.digit( 3 );
        e.backspace();
        CHECK( e.value() == 12.0 );
        e.clear();
        CHECK( e.isEmpty() && e.value() == 0.0 );
        e.backspace();
        CHECK( e.isEmpty() );
    }
    // Loading a value (inc/dec, x2, /2): trimmed decimals, negative, zero.
    {
        NumPadEntry e;
        e.setValue( 120.0 );
        CHECK( e.text() == "120" );
        e.setValue( 0.5 );
        CHECK( e.text() == "0.5" );
        e.setValue( -2.25 );
        CHECK( e.text() == "-2.25" && near( e.value(), -2.25 ) );
        e.setValue( 0.0 );
        CHECK( e.text() == "0" && !( e.value() < 0 ) );
        e.setValue( -0.0001 ); // rounds to 0: no "-0"
        CHECK( e.text() == "0" );
        e.setValue( 100.0 );
        e.backspace();
        CHECK( e.value() == 10.0 );
    }

    // Tap tempo: 4 taps at 120 BPM (500 ms apart) -> 120 (the old formula said 60*4/1.5 = 160).
    {
        TapTempo t( 4 );
        CHECK( t.tap( 1000 ) == 0.0 );
        CHECK( t.tap( 1500 ) == 0.0 );
        CHECK( t.tap( 2000 ) == 0.0 );
        CHECK( near( t.tap( 2500 ), 120.0, 1e-6 ) );
        CHECK( near( t.tap( 3000 ), 120.0, 1e-6 ) ); // sliding window keeps going
    }
    // Two taps are enough for the hardware button (numTaps 2).
    {
        TapTempo t( 2 );
        CHECK( t.tap( 0 ) == 0.0 );
        CHECK( near( t.tap( 600 ), 100.0, 1e-6 ) );
    }
    // Default 3 taps.
    {
        TapTempo t;
        t.tap( 0 ); t.tap( 500 );
        CHECK( near( t.tap( 1000 ), 120.0, 1e-6 ) );
    }
    // A long pause starts a new series: an old first tap never counts.
    {
        TapTempo t( 3 );
        t.tap( 0 ); t.tap( 500 );
        CHECK( t.tap( 10000 ) == 0.0 );
        CHECK( t.count() == 1 );
        t.tap( 10500 );
        CHECK( near( t.tap( 11000 ), 120.0, 1e-6 ) );
    }
    // Bounce (closer than minGap) is ignored; out of range gives 0.
    {
        TapTempo t( 2 );
        t.tap( 0 );
        CHECK( t.tap( 50 ) == 0.0 && t.count() == 1 );
        TapTempo slow( 2, 5000.0 );
        slow.tap( 0 );
        CHECK( slow.tap( 4000 ) == 0.0 ); // 15 BPM is below the 30 BPM floor
        TapTempo fast( 2, 2000.0, 100.0 );
        fast.tap( 0 );
        CHECK( fast.tap( 150 ) == 0.0 ); // 400 BPM is above the 300 BPM ceiling
    }
    // numTaps below 2 is raised to 2; changing it trims the window.
    {
        TapTempo t( 0 );
        t.tap( 0 );
        CHECK( near( t.tap( 500 ), 120.0, 1e-6 ) );
        t.setNumTaps( 5 );
        CHECK( t.tap( 1000 ) == 0.0 );
        t.reset();
        CHECK( t.count() == 0 );
    }

    if( g_fail == 0 ) std::printf( "NumPadTapTempoTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
