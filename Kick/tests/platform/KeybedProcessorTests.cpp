// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for KeybedProcessor (PP-21, PP-21a-e, PP-30b).
// Verifies:
// 1. Octave, semitone transpose, MIDI channel, and soundingPitch clamping (PP-21a).
// 2. Aftertouch enable / disable (PP-21a).
// 3. Velocity curves (Linear, Soft, Hard), gain, fixed velocity, and white/black key sensitivity (PP-21b).
// 4. Scale filter quantization (PP-21c).
// 5. Internal / External routing decisions (PP-21d).
// 6. Pedal polarity and enable handling (PP-21e).
// 7. Full processNoteOn pipeline.

#include "../../modules/kick_ui/modes/padperform/kick_keybed_processor.h"
#include <cstdio>
#include <cstdlib>

static void check( bool ok, const char* why )
{
    if( !ok ) { std::fprintf( stderr, "FAIL: %s\n", why ); std::exit( 1 ); }
}

static void testTransposeAndChannel()
{
    kick_hw::KeybedProcessor kp;

    // Defaults
    check( kp.octave() == 0, "default octave is 0" );
    check( kp.transpose() == 0, "default transpose is 0" );
    check( kp.midiChannel() == 1, "default channel is 1" );
    check( kp.isAftertouchEnabled(), "default aftertouch is enabled" );
    check( kp.soundingPitch( 60 ) == 60, "pitch 60 sounding pitch is 60" );

    // Octave clamp
    kp.setOctave( 4 );
    check( kp.octave() == 3, "octave clamped to +3" );
    kp.setOctave( -5 );
    check( kp.octave() == -3, "octave clamped to -3" );
    kp.setOctave( 1 );

    // Transpose clamp
    kp.setTranspose( 40 );
    check( kp.transpose() == 36, "transpose clamped to +36" );
    kp.setTranspose( -50 );
    check( kp.transpose() == -36, "transpose clamped to -36" );
    kp.setTranspose( -2 );

    // Channel clamp
    kp.setMidiChannel( 0 );
    check( kp.midiChannel() == 1, "channel clamped to min 1" );
    kp.setMidiChannel( 20 );
    check( kp.midiChannel() == 16, "channel clamped to max 16" );
    kp.setMidiChannel( 10 );
    check( kp.midiChannel() == 10, "channel set to 10" );

    // Sounding pitch: 60 + 1*12 - 2 = 70
    check( kp.soundingPitch( 60 ) == 70, "sounding pitch = 60 + 12 - 2 = 70" );

    // Clamping to MIDI range 0..127
    kp.setOctave( 3 );
    kp.setTranspose( 36 );
    check( kp.soundingPitch( 100 ) == 127, "clamped to 127" );
    kp.setOctave( -3 );
    kp.setTranspose( -36 );
    check( kp.soundingPitch( 10 ) == 0, "clamped to 0" );

    // Aftertouch
    kp.setAftertouchEnabled( false );
    check( !kp.isAftertouchEnabled(), "aftertouch disabled" );
    check( kp.processAftertouch( 100 ) == 0, "aftertouch processed to 0 when disabled" );
    kp.setAftertouchEnabled( true );
    check( kp.processAftertouch( 85 ) == 85, "aftertouch passed when enabled" );
    check( kp.processAftertouch( 150 ) == 127, "aftertouch clamped to 127" );
}

static void testVelocityProcessing()
{
    kick_hw::KeybedProcessor kp;

    // Linear curve, default gain 1.0, default sens 1.0
    check( kp.processVelocity( 60, 64 ) == 64, "linear middle velocity" );
    check( kp.processVelocity( 60, 127 ) == 127, "linear max velocity" );
    check( kp.processVelocity( 60, 1 ) == 1, "linear min velocity" );

    // Fixed velocity mode
    kp.setVelocityMode( kick_hw::KeybedProcessor::VelocityMode::Fixed );
    kp.setFixedVelocity( 100 );
    check( kp.processVelocity( 60, 10 ) == 100, "fixed velocity mode always returns fixed val" );
    check( kp.processVelocity( 60, 127 ) == 100, "fixed velocity mode ignores input 127" );

    kp.setVelocityMode( kick_hw::KeybedProcessor::VelocityMode::Variable );

    // Soft curves boost mid velocities
    kp.setVelocityCurve( kick_hw::KeybedProcessor::VelocityCurve::Soft1 );
    int softVal = kp.processVelocity( 60, 32 );
    check( softVal > 32, "soft curve boosts lower/mid velocity" );

    // Hard curves attenuate mid velocities
    kp.setVelocityCurve( kick_hw::KeybedProcessor::VelocityCurve::Hard1 );
    int hardVal = kp.processVelocity( 60, 64 );
    check( hardVal < 64, "hard curve attenuates lower/mid velocity" );

    // Velocity gain
    kp.setVelocityCurve( kick_hw::KeybedProcessor::VelocityCurve::Linear );
    kp.setVelocityGain( 1.5f );
    int boosted = kp.processVelocity( 60, 50 );
    check( boosted == 75, "gain 1.5 boosts 50 to 75" );

    // White vs black key sensitivity
    // 60 is C (white key), 61 is C# (black key)
    check( !kick_hw::KeybedProcessor::isBlackKey( 60 ), "C is white key" );
    check( kick_hw::KeybedProcessor::isBlackKey( 61 ), "C# is black key" );

    kp.setVelocityGain( 1.0f );
    kp.setWhiteKeySensitivity( 1.0f );
    kp.setBlackKeySensitivity( 0.8f );
    int whiteV = kp.processVelocity( 60, 100 );
    int blackV = kp.processVelocity( 61, 100 );
    check( whiteV == 100, "white key normal" );
    check( blackV == 80, "black key scaled by 0.8" );
}

static void testScaleFilter()
{
    kick_hw::KeybedProcessor kp;

    // Major scale on C: C D E F G A B (0, 2, 4, 5, 7, 9, 11)
    kp.setScale( 0, { 2, 2, 1, 2, 2, 2, 1 } );

    // Disabled by default: notes pass through untouched
    check( !kp.isScaleFilterEnabled(), "scale filter disabled by default" );
    check( kp.quantizeToScale( 61 ) == 61, "C# passes untouched when filter disabled" );

    // Enable scale filter
    kp.setScaleFilterEnabled( true );
    check( kp.quantizeToScale( 60 ) == 60, "C stays C" );
    // 61 is C#: in C major, distance to C(0) is 1, distance to D(2) is 1. Nearest is 60 or 62.
    int q61 = kp.quantizeToScale( 61 );
    check( q61 == 60 || q61 == 62, "C# quantizes to C or D" );

    // 63 is D# (pc 3): nearest in C Major is D(2) or E(4)
    int q63 = kp.quantizeToScale( 63 );
    check( q63 == 62 || q63 == 64, "D# quantizes to D or E" );

    // Notes already in scale remain exact
    check( kp.quantizeToScale( 64 ) == 64, "E stays E" );
    check( kp.quantizeToScale( 65 ) == 65, "F stays F" );
    check( kp.quantizeToScale( 67 ) == 67, "G stays G" );
}

static void testRoutingAndPedals()
{
    kick_hw::KeybedProcessor kp;

    // Routing
    check( kp.routing() == kick_hw::KeybedProcessor::Routing::Both, "default routing is Both" );
    check( kp.routeToInternal(), "routes to internal" );
    check( kp.routeToExternal(), "routes to external" );

    kp.setRouting( kick_hw::KeybedProcessor::Routing::InternalOnly );
    check( kp.routeToInternal(), "InternalOnly routes internal" );
    check( !kp.routeToExternal(), "InternalOnly does not route external" );

    kp.setRouting( kick_hw::KeybedProcessor::Routing::ExternalOnly );
    check( !kp.routeToInternal(), "ExternalOnly does not route internal" );
    check( kp.routeToExternal(), "ExternalOnly routes external" );

    // Pedal
    check( kp.isSustainPedalEnabled(), "sustain pedal enabled" );
    check( kp.sustainPedalPolarity() == kick_hw::KeybedProcessor::PedalPolarity::Normal, "normal polarity" );
    check( kp.processSustainPedal( true ), "normal polarity closed -> true" );
    check( !kp.processSustainPedal( false ), "normal polarity open -> false" );

    kp.setSustainPedalPolarity( kick_hw::KeybedProcessor::PedalPolarity::Inverted );
    check( !kp.processSustainPedal( true ), "inverted polarity closed -> false" );
    check( kp.processSustainPedal( false ), "inverted polarity open -> true" );

    kp.setSustainPedalEnabled( false );
    check( !kp.processSustainPedal( true ), "disabled pedal returns false" );
    check( !kp.processSustainPedal( false ), "disabled pedal returns false" );
}

static void testProcessNoteOnPipeline()
{
    kick_hw::KeybedProcessor kp;
    kp.setOctave( 1 );
    kp.setMidiChannel( 3 );
    kp.setScale( 0, { 2, 2, 1, 2, 2, 2, 1 } );
    kp.setScaleFilterEnabled( true );

    // rawKey 60 (C4) -> octave +1 = 72 (C5), in scale = 72, channel 3
    auto res = kp.processNoteOn( 60, 100 );
    check( res.pitch == 72, "note pitch is 72" );
    check( res.channel == 3, "note channel is 3" );
    check( res.velocity == 100, "note velocity is 100" );
    check( !res.filtered, "not filtered" );
}

int main()
{
    testTransposeAndChannel();
    testVelocityProcessing();
    testScaleFilter();
    testRoutingAndPedals();
    testProcessNoteOnPipeline();
    std::puts( "KeybedProcessorTests: OK" );
    return 0;
}
