// Host tests for the pure mixer / mute logic (Kick/modules/common/source/kick_mixer_logic.h):
// Q-Link steps, switch direction, row guards, gain <-> MIDI <-> dB, Midi view row <-> track, Clear Solos model.
#include "../../modules/common/source/kick_mixer_logic.h"
#include <cstdio>
#include <cstdlib>
using namespace mt::mixlogic;
static int fails = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL line %d: %s\n", __LINE__, #c ); ++fails; } } while( 0 )
static bool near( double a, double b ) { return std::abs( a - b ) < 1e-9; }

int main()
{
    // Q-Link step: one tick = one increment, never the whole scale.
    CHECK( near( qlinkDelta( 1, 0.01 ), 0.01 ) );
    CHECK( near( qlinkDelta( -3, 0.01 ), -0.03 ) );
    CHECK( near( qlinkStep( 0.5, 1, 0.01, 0.0, 1.0 ), 0.51 ) );
    CHECK( near( qlinkStep( 0.995, 5, 0.01, 0.0, 1.0 ), 1.0 ) );   // clamped
    CHECK( near( qlinkStep( 0.0, -1, 0.01, -1.0, 1.0 ), -0.01 ) ); // pan through zero
    CHECK( near( qlinkStep( -1.0, -1, 0.01, -1.0, 1.0 ), -1.0 ) );
    CHECK( qlinkStepInt( 126, 5, 0, 127 ) == 127 );
    CHECK( qlinkStepInt( 1, -5, 0, 127 ) == 0 );
    CHECK( qlinkStepInt( 10, 1, 0, 127 ) == 11 );

    // Switch direction: right = on, left = off, 0 = unchanged.
    CHECK( toggleFromDelta( false, 1 ) );
    CHECK( toggleFromDelta( true, 3 ) );
    CHECK( !toggleFromDelta( true, -1 ) );
    CHECK( !toggleFromDelta( false, -2 ) );
    CHECK( toggleFromDelta( true, 0 ) );
    CHECK( !toggleFromDelta( false, 0 ) );

    // Mute group: 0 = None reachable, upper limit respected.
    CHECK( stepMuteGroup( 1, -1, 16 ) == 0 );
    CHECK( stepMuteGroup( 0, -1, 16 ) == 0 );
    CHECK( stepMuteGroup( 16, 1, 16 ) == 16 );
    CHECK( stepMuteGroup( 3, 2, 16 ) == 5 );

    // Row guards (empty list, -1, >= size).
    CHECK( safeRow( -1, 4 ) == -1 );
    CHECK( safeRow( 4, 4 ) == -1 );
    CHECK( safeRow( 3, 4 ) == 3 );
    CHECK( qlinkRow( 2, 0 ) == -1 );   // empty list: no row, no crash
    CHECK( qlinkRow( -1, 5 ) == 0 );   // nothing selected: first row
    CHECK( qlinkRow( 7, 5 ) == 0 );
    CHECK( qlinkRow( 3, 5 ) == 3 );

    // dB / pan text.
    CHECK( formatGainDb( 1.0 ) == "0.0 dB" );
    CHECK( formatGainDb( 0.5 ) == "-6.0 dB" );
    CHECK( formatGainDb( 0.0 ) == "-inf dB" );
    CHECK( formatGainDb( 0.300000011920929 ) == "-10.5 dB" );
    CHECK( formatPan( 0.0 ) == "C" );
    CHECK( formatPan( -1.0 ) == "L100" );
    CHECK( formatPan( 0.25 ) == "R25" );

    // gain <-> MIDI round trip over the whole scale (the old truncating version lost 1 on 9, 13, 18, ...).
    for( int m = 0; m <= 127; ++m )
        CHECK( gainToMidi127( midiToGain127( m ) ) == m );
    CHECK( gainToMidi127( 2.0f ) == 127 );
    CHECK( gainToMidi127( -1.0f ) == 0 );

    // Midi view: rows are the non-audio tracks, selection maps row <-> track index (not row == track).
    std::vector<bool> isAudio{ false, true, false, false, true };
    auto              rows = midiTrackRows( isAudio );
    CHECK( rows.size() == 3 );
    CHECK( rowToTrack( rows, 0 ) == 0 );
    CHECK( rowToTrack( rows, 1 ) == 2 );
    CHECK( rowToTrack( rows, 2 ) == 3 );
    CHECK( rowToTrack( rows, 3 ) == -1 );
    CHECK( rowToTrack( rows, -1 ) == -1 );
    CHECK( trackToRow( rows, 2 ) == 1 );
    CHECK( trackToRow( rows, 1 ) == -1 ); // audio track has no row
    CHECK( trackToRow( rows, 3 ) == 2 );
    CHECK( rowToTrack( std::vector<int>{}, 0 ) == -1 );

    // Q-Link addressed pad: bank offset included (MU-H2b).
    CHECK( bankPadIndex( 0, 0 ) == 0 );
    CHECK( bankPadIndex( 1, 3 ) == 19 );
    CHECK( bankPadIndex( 7, 3 ) == 115 );

    // Solo model: single solo, then Clear Solos leaves nothing solo and nothing solo-muted.
    std::vector<TrackSoloState> t( 4 );
    setSingleSolo( t, 1, true );
    CHECK( t[1].solo && !t[1].soloMuted );
    CHECK( !t[0].solo && t[0].soloMuted && t[2].soloMuted && t[3].soloMuted );
    setSingleSolo( t, 2, true ); // new solo clears the previous one
    CHECK( !t[1].solo && t[1].soloMuted && t[2].solo && !t[2].soloMuted );
    clearAllSolos( t );
    for( auto& s : t )
        CHECK( !s.solo && !s.soloMuted );
    setSingleSolo( t, 0, true );
    setSingleSolo( t, 0, false );
    for( auto& s : t )
        CHECK( !s.solo && !s.soloMuted );

    std::puts( fails ? "FAILED" : "OK" );
    return fails ? 1 : 0;
}
