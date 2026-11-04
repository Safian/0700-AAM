// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for Mute / Solo logic (MU-12, MU-14): Time Division grid calculation,
// Clear Solos / Single Solo state transitions, mute group propagation, and DCL bridge
// pad LED command processing (applyPadPacket).
#include "../../modules/engine/audio_engine/engine/mt_time_division_mute.h"
#include "../../modules/common/source/kick_mixer_logic.h"
#include "../../apps/kick/hardware/pad_colour_map.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

static int fails = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL line %d: %s\n", __LINE__, #c ); ++fails; } } while( 0 )

int main()
{
    // --- 1. Time Division Mute: divisionToTicks & nextGridPoint (MU-12, D-080..D-082, D-120..D-121) ---
    using namespace mt::timedivmute;
    // Straight notes (PPQN = 96: quarter note = 96 ticks)
    CHECK( divisionToTicks( "1/16" ) == 24 );
    CHECK( divisionToTicks( "1/8" ) == 48 );
    CHECK( divisionToTicks( "1/4" ) == 96 );
    CHECK( divisionToTicks( "1/2" ) == 192 );
    CHECK( divisionToTicks( "1" ) == 384 );
    CHECK( divisionToTicks( "1 Bar" ) == 384 );
    CHECK( divisionToTicks( "2" ) == 768 );
    CHECK( divisionToTicks( "2 Bar" ) == 768 );

    // Triplets
    CHECK( divisionToTicks( "1/16T" ) == 16 );
    CHECK( divisionToTicks( "1/8T" ) == 32 );
    CHECK( divisionToTicks( "1/4T" ) == 64 );
    CHECK( divisionToTicks( "1/2T" ) == 128 );

    // Unknown defaults to 1/16
    CHECK( divisionToTicks( "unknown" ) == 24 );

    // nextGridPoint: strictly next grid point
    CHECK( nextGridPoint( 0, 24 ) == 24 );
    CHECK( nextGridPoint( 10, 24 ) == 24 );
    CHECK( nextGridPoint( 23, 24 ) == 24 );
    CHECK( nextGridPoint( 24, 24 ) == 48 );
    CHECK( nextGridPoint( 95, 96 ) == 96 );
    CHECK( nextGridPoint( 96, 96 ) == 192 );
    CHECK( nextGridPoint( 100, 384 ) == 384 );
    CHECK( nextGridPoint( 384, 384 ) == 768 );
    CHECK( nextGridPoint( 50, 0 ) == 50 ); // non-positive grid returns tick unchanged
    CHECK( nextGridPoint( -5, 24 ) == 0 );

    // shouldTrigger
    CHECK( !shouldTrigger( 20, 24 ) );
    CHECK( shouldTrigger( 24, 24 ) );
    CHECK( shouldTrigger( 30, 24 ) );
    CHECK( !shouldTrigger( 100, -1 ) ); // negative scheduled tick never triggers

    // --- 2. Solo Model: Single Solo & Clear Solos (MU-01, D-084, D-085, D-091) ---
    using namespace mt::mixlogic;
    std::vector<TrackSoloState> tracks( 8 );

    // Soloing track 1 makes it solo, others become soloMuted
    setSingleSolo( tracks, 1, true );
    CHECK( tracks[1].solo && !tracks[1].soloMuted );
    for( size_t i = 0; i < tracks.size(); ++i )
    {
        if( i != 1 )
            CHECK( !tracks[i].solo && tracks[i].soloMuted );
    }

    // Single solo rule: soloing track 3 clears track 1 solo
    setSingleSolo( tracks, 3, true );
    CHECK( !tracks[1].solo && tracks[1].soloMuted );
    CHECK( tracks[3].solo && !tracks[3].soloMuted );

    // Clear solos resets all solo and soloMuted states
    clearAllSolos( tracks );
    for( const auto& t : tracks )
    {
        CHECK( !t.solo && !t.soloMuted );
    }

    // --- 3. Mute Group Propagation Model (MU-02, MU-08, D-071, D-110..D-112) ---
    struct TrackMuteState
    {
        int  group{ 0 }; // 0 = None, 1..16 = groups
        bool mute{ false };
    };
    std::vector<TrackMuteState> groupTracks( 6 );
    groupTracks[0].group = 1;
    groupTracks[2].group = 1;
    groupTracks[4].group = 1;
    groupTracks[1].group = 2;
    groupTracks[3].group = 0; // independent

    auto setGroupMute = [&]( std::vector<TrackMuteState>& trks, int trackIdx, bool muted ) {
        trks[trackIdx].mute = muted;
        int g = trks[trackIdx].group;
        if( g > 0 )
        {
            for( auto& t : trks )
                if( t.group == g )
                    t.mute = muted;
        }
    };

    // Muting track 0 propagates to tracks 2 and 4 (same group 1)
    setGroupMute( groupTracks, 0, true );
    CHECK( groupTracks[0].mute );
    CHECK( groupTracks[2].mute );
    CHECK( groupTracks[4].mute );
    CHECK( !groupTracks[1].mute ); // group 2 untouched
    CHECK( !groupTracks[3].mute ); // independent untouched

    // Unmuting track 2 un-mutes group 1
    setGroupMute( groupTracks, 2, false );
    CHECK( !groupTracks[0].mute );
    CHECK( !groupTracks[2].mute );
    CHECK( !groupTracks[4].mute );

    // Muting independent track 3 only mutes track 3
    setGroupMute( groupTracks, 3, true );
    CHECK( groupTracks[3].mute );
    CHECK( !groupTracks[0].mute );
    CHECK( !groupTracks[1].mute );

    // --- 4. DCL Bridge Packet Processing (MU-05, MU-14, pad_colour_map.h) ---
    using namespace kick_hw;
    PadStateArray padArr{};

    // Initial state: colorMode
    int modeData[1] = { 1 };
    uint16_t mask = applyPadPacket( kCmdPadColorMode, modeData, 1, padArr );
    CHECK( mask == 0xFFFF );
    CHECK( padArr.colorMode == 1 );

    // kCmdPadSetColourState: full 16 pads colors (data[0..15]) and states (data[16..31])
    int fullState[32];
    for( int i = 0; i < 16; ++i )
    {
        fullState[i]      = i; // colour id i
        fullState[16 + i] = ( i % 2 == 0 ) ? kPadStateOn : kPadStateOff;
    }
    mask = applyPadPacket( kCmdPadSetColourState, fullState, 32, padArr );
    CHECK( mask == 0xFFFF ); // all 16 pads changed
    for( int i = 0; i < 16; ++i )
    {
        CHECK( padArr.colours[i] == i );
        CHECK( padArr.states[i] == ( ( i % 2 == 0 ) ? kPadStateOn : kPadStateOff ) );
    }

    // Sending identical state returns mask 0 (no redundant DCL traffic)
    mask = applyPadPacket( kCmdPadSetColourState, fullState, 32, padArr );
    CHECK( mask == 0 );

    // kCmdPadSetSinglePad: update pad 5
    int singleData[3] = { 5, kPadStateOn, 12 };
    mask = applyPadPacket( kCmdPadSetSinglePad, singleData, 3, padArr );
    CHECK( mask == ( 1u << 5 ) );
    CHECK( padArr.colours[5] == 12 );
    CHECK( padArr.states[5] == kPadStateOn );

    // Repeating single pad update returns 0
    mask = applyPadPacket( kCmdPadSetSinglePad, singleData, 3, padArr );
    CHECK( mask == 0 );

    // Malformed packet checks (undersized or out of bounds)
    int badPad[3] = { 16, kPadStateOn, 1 }; // pad 16 is invalid (valid: 0..15)
    CHECK( applyPadPacket( kCmdPadSetSinglePad, badPad, 3, padArr ) == 0 );
    CHECK( applyPadPacket( kCmdPadSetSinglePad, singleData, 2, padArr ) == 0 ); // size < 3
    CHECK( applyPadPacket( kCmdPadSetColourState, fullState, 31, padArr ) == 0 ); // size < 32
    CHECK( applyPadPacket( kCmdPadSetSinglePad, nullptr, 3, padArr ) == 0 );

    std::puts( fails ? "FAILED" : "OK" );
    return fails ? 1 : 0;
}
