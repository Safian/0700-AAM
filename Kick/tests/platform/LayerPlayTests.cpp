// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for Layer Play (Cycle wraps, Random only picks layers with a sound, Velocity range is closed so a 127 hit
// finds its layer). (The Sample Play label <-> enum order is a static_assert in program_edit_ui/sources/kick_ProgramEditInterface.h.)

#include "../../modules/engine/audio_program/synth/layer_play_select.h"
#include <cstdio>
#include <cstdlib>
#include <set>

using namespace mt::layerplay;

static void check( bool ok, const char* why )
{
    if( !ok ) { std::fprintf( stderr, "FAIL: %s\n", why ); std::exit( 1 ); }
}

static void testCycle()
{
    std::vector<Candidate> c = { { 0, 1, 127 }, { 1, 1, 127 }, { 3, 1, 127 } }; // layer 2 is empty
    int                    st = -1;
    int                    got[7];
    for( int& g : got ) g = select( Mode::Cycle, c, 100, st, 0 );
    const int expect[7] = { 0, 1, 3, 0, 1, 3, 0 };
    for( int i = 0; i < 7; ++i ) check( got[i] == expect[i], "cycle wraps around and skips the empty layer (it used to stick on the last)" );
    int st2 = -1;
    check( select( Mode::Cycle, { { 2, 1, 127 } }, 100, st2, 0 ) == 2 && select( Mode::Cycle, { { 2, 1, 127 } }, 100, st2, 0 ) == 2, "single layer: always that layer" );
}

static void testRandom()
{
    std::vector<Candidate> c = { { 1, 1, 127 }, { 3, 1, 127 } };
    std::set<int>          seen;
    int                    st = -1;
    for( unsigned r = 0; r < 20; ++r ) seen.insert( select( Mode::Random, c, 100, st, r ) );
    check( seen == std::set<int>( { 1, 3 } ), "random picks only layers that have a sound (not the list position 0 / 1)" );
}

static void testVelocity()
{
    std::vector<Candidate> c = { { 0, 1, 63 }, { 1, 64, 127 } };
    int                    st = -1;
    check( select( Mode::Velocity, c, 1, st, 0 ) == 0 && select( Mode::Velocity, c, 63, st, 0 ) == 0, "low range" );
    check( select( Mode::Velocity, c, 64, st, 0 ) == 1, "range start inclusive" );
    check( select( Mode::Velocity, c, 127, st, 0 ) == 1, "a 127 hit (Full Level) finds the layer ending at 127" );
    std::vector<Candidate> hole = { { 2, 1, 60 } };
    check( select( Mode::Velocity, hole, 127, st, 0 ) == 2, "no range matches: the first layer with a sound, never an empty layer 0" );
    check( select( Mode::Velocity, {}, 100, st, 0 ) == -1, "no layers: -1" );
}

int main()
{
    testCycle();
    testRandom();
    testVelocity();
    std::puts( "LayerPlayTests: OK" );
    return 0;
}
