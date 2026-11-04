// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test of the mode-menu logic (pad -> tile, dial stepping, tile table, project gate, back-stack + MENU toggle).
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -o /tmp/t MenuLogicTests.cpp
#include "../../modules/kick_ui_components/source/kick_menu_logic.h"
#include <cstdio>
#include <set>

// Stand-in for kick_ui::Screen: same enumerator names, the logic only needs those.
enum class S
{
    Invalid = -9999, HomeOptions = 1000, HomeListView, HomeGridView, HomeSequencerView, HomeXYView, HomePreferencesView, HomePreferences,
    MenuSampleEdit, MenuProgramEdit, MenuSampler, MenuLooper, MenuPadMute, MenuTrackMute, MenuPadMixer, MenuChannelMixer, MenuKnobEdit,
    MenuListEdit, MenuPadPerform, MenuMidiControl, MenuFileBrowser, MenuNextSequence, MenuSong, MenuColorEdit, MenuAssignSample,
    RecentProject, SWUpdateView, SWUpdateWindow, MenuPiano, Temp
};

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while( 0 )

using namespace ui::menu_logic;

int main()
{
    // Pad 1 (index 0, bottom left) -> bottom row first tile (12); pad 13 (index 12, top left) -> tile 0; pad 16 -> tile 3.
    CHECK( padToTile( 0, 17 ) == 12 );
    CHECK( padToTile( 3, 17 ) == 15 );
    CHECK( padToTile( 12, 17 ) == 0 );
    CHECK( padToTile( 15, 17 ) == 3 );
    CHECK( padToTile( 5, 17 ) == 9 );
    CHECK( padToTile( -1, 17 ) == -1 );
    CHECK( padToTile( 16, 17 ) == -1 );
    CHECK( padToTile( 0, 8 ) == -1 ); // tile 12 does not exist in an 8-tile menu
    {
        std::set<int> seen;
        for( int p = 0; p < 16; ++p ) seen.insert( padToTile( p, 17 ) );
        CHECK( seen.size() == 16 ); // bijective
    }

    // Dial stepping: wraps, Shift = one row.
    CHECK( stepTile( 0, 1, false, 17, 4 ) == 1 );
    CHECK( stepTile( 16, 1, false, 17, 4 ) == 0 );
    CHECK( stepTile( 0, -1, false, 17, 4 ) == 16 );
    CHECK( stepTile( 2, 1, true, 17, 4 ) == 6 );
    CHECK( stepTile( 14, 1, true, 17, 4 ) == 1 );
    CHECK( stepTile( 3, -2, false, 17, 4 ) == 1 );
    CHECK( stepTile( 3, 40, false, 17, 4 ) == ( 3 + 40 ) % 17 );
    CHECK( stepTile( 3, 1, false, 0, 4 ) == 0 );

    // Tile table: every tile has a distinct, valid screen; reverse lookup is consistent; unknown index is Invalid.
    {
        std::set<int> seen;
        for( int i = 0; i < kTileCount; ++i )
        {
            const S s = tileScreen<S>( i );
            CHECK( s != S::Invalid );
            CHECK( seen.insert( (int)s ).second );
            CHECK( screenTile( s ) == i );
        }
    }
    CHECK( tileScreen<S>( 17 ) == S::Invalid );
    CHECK( tileScreen<S>( -1 ) == S::Invalid );
    CHECK( screenTile( S::HomeGridView ) == -1 );

    // Project gate.
    CHECK( screenNeedsProject( S::MenuSampler ) );
    CHECK( screenNeedsProject( S::HomeGridView ) );
    CHECK( screenNeedsProject( S::HomeListView ) );
    CHECK( !screenNeedsProject( S::RecentProject ) );
    CHECK( !screenNeedsProject( S::MenuFileBrowser ) );
    CHECK( !screenNeedsProject( S::HomePreferencesView ) );
    CHECK( !screenNeedsProject( S::HomeOptions ) );
    CHECK( !screenNeedsProject( S::Invalid ) );
    for( int i = 0; i < kTileCount; ++i ) // only the Browser tile works without a project
        CHECK( screenNeedsProject( tileScreen<S>( i ) ) == ( tileScreen<S>( i ) != S::MenuFileBrowser ) );

    // Back-stack and MENU toggle.
    {
        ScreenNav<S> nav;
        S current = S::HomeListView;
        auto open = [&]( S t ) { nav.open( current ); nav.noteLeaving( current ); current = t; };
        auto close = [&]( S t ) { const S l = nav.close( current, t ); nav.noteLeaving( current ); current = l; };

        close( S::HomeOptions );               // MENU button from the main view
        CHECK( current == S::HomeOptions );
        CHECK( nav.lastNonMenu() == S::HomeListView );
        close( S::HomeOptions );               // MENU again: back to where it was opened from
        CHECK( current == S::HomeListView );

        close( S::HomeGridView );
        close( S::HomeOptions );
        CHECK( nav.lastNonMenu() == S::HomeGridView );
        open( S::MenuSampler );                // tile -> screen
        CHECK( current == S::MenuSampler );
        close( S::Invalid );                   // Back returns to the Menu, not the main view
        CHECK( current == S::HomeOptions );
        close( S::HomeOptions );               // MENU toggle: the mode before the Menu was the Sampler
        CHECK( current == S::MenuSampler );

        close( S::Invalid );                   // empty stack: main view
        CHECK( current == S::HomeListView );

        // A screen opened from a screen returns to it.
        close( S::MenuSampler );
        open( S::MenuAssignSample );
        close( S::Invalid );
        CHECK( current == S::MenuSampler );

        // Toggle without any earlier screen: main view.
        ScreenNav<S> fresh;
        CHECK( fresh.close( S::HomeOptions, S::HomeOptions ) == S::HomeListView );
        CHECK( fresh.previous() == S::HomeListView );
    }

    if( g_fail == 0 ) std::printf( "MenuLogicTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
