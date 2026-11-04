// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the touch piano pure logic: multi-finger state machine, glissando (slide / legato) note-offs,
// velocity modes, bend / vibrato mapping, sustain held-note logic, transposition and scale marking.

#include "../../modules/kick_ui_components/source/kick_piano_logic.h"
#include <cstdio>
#include <cstdlib>

using namespace ui::piano;

static void check( bool ok, const char* why )
{
    if( !ok ) { std::fprintf( stderr, "FAIL: %s\n", why ); std::exit( 1 ); }
}

static int count( const Events& e, Event::Type t, int note = -1 )
{
    int n = 0;
    for( auto& x : e ) if( x.type == t && ( note < 0 || x.note == note ) ) ++n;
    return n;
}

static void testTwoFingers()
{
    TouchPiano p; p.cfg.velocityMode = VelocityMode::Fixed; p.cfg.fixedVelocity = 90;
    Events e;
    p.down( 0, 60, 0.5f, 10, 10, 0, e );
    p.down( 1, 64, 0.5f, 50, 10, 5, e );
    check( e.size() == 2 && e[0].type == Event::NoteOn && e[0].note == 60 && e[0].slot == 0, "finger 0 plays 60" );
    check( e[1].type == Event::NoteOn && e[1].note == 64 && e[1].slot == 1 && e[1].velocity == 90, "finger 1 plays 64 as well (second finger does not overwrite the first)" );
    e.clear();
    p.up( 0, e );
    check( e.size() == 1 && e[0].type == Event::NoteOff && e[0].note == 60, "lifting finger 0 ends only 60" );
    check( p.noteOfSlot( 1 ) == 64 && p.fingersOn( 64 ) == 1, "finger 1 still holds 64" );
    e.clear();
    p.up( 1, e );
    check( count( e, Event::NoteOff, 64 ) == 1, "finger 1 up ends 64" );
}

static void testSameNoteTwoFingers()
{
    TouchPiano p; Events e;
    p.down( 0, 60, 0.5f, 0, 0, 0, e );
    p.down( 1, 60, 0.5f, 0, 0, 1, e );
    check( count( e, Event::NoteOn, 60 ) == 1, "second finger on a held key does not retrigger" );
    e.clear();
    p.up( 0, e );
    check( e.empty(), "no note-off while the other finger still holds the key" );
    p.up( 1, e );
    check( count( e, Event::NoteOff, 60 ) == 1, "note-off when the last finger leaves" );
}

static void testLostTouchUp()
{
    TouchPiano p; Events e;
    p.down( 3, 60, 0.5f, 0, 0, 0, e );
    e.clear();
    p.down( 3, 62, 0.5f, 0, 0, 10, e ); // down again without an up
    check( e.size() == 2 && e[0].type == Event::NoteOff && e[0].note == 60 && e[1].type == Event::NoteOn && e[1].note == 62, "a second down on the same slot ends the old note first" );
}

static void testGlissandoSlide()
{
    TouchPiano p; p.cfg.dragMode = DragMode::Glide; Events e;
    p.down( 0, 60, 0.5f, 0, 0, 0, e );
    e.clear();
    p.move( 0, 60, 0.5f, 2, 0, 5, e );
    check( e.empty(), "move inside the same key: nothing" );
    p.move( 0, 62, 0.5f, 20, 0, 10, e );
    check( e.size() == 2 && e[0].type == Event::NoteOff && e[0].note == 60 && e[1].type == Event::NoteOn && e[1].note == 62, "slide: note-off of the old key BEFORE the note-on of the new one" );
    e.clear();
    p.move( 0, -1, 0.5f, 25, 0, 15, e );
    check( e.empty() && p.noteOfSlot( 0 ) == 62, "finger between / off keys keeps the held note" );
    p.up( 0, e );
    check( e.size() == 1 && e[0].type == Event::NoteOff && e[0].note == 62, "glissando end: only the last key gets a note-off (no stuck note)" );
    for( int n = 0; n < 128; ++n ) check( ! p.isSounding( n ), "nothing sounds after the glissando" );
}

static void testGlissandoLegato()
{
    TouchPiano p; p.cfg.dragMode = DragMode::Legato; Events e;
    p.down( 0, 60, 0.5f, 0, 0, 0, e );
    e.clear();
    p.move( 0, 62, 0.5f, 20, 0, 10, e );
    check( e.size() == 2 && e[0].type == Event::NoteOn && e[0].note == 62 && e[1].type == Event::NoteOff && e[1].note == 60, "legato: new note-on BEFORE the old note-off" );
    e.clear();
    p.up( 0, e );
    check( count( e, Event::NoteOff, 62 ) == 1 && e.size() == 1, "legato end: one note-off" );
}

static void testGlissandoOverHeldKey()
{
    TouchPiano p; Events e;
    p.down( 1, 62, 0.5f, 0, 0, 0, e );
    p.down( 0, 60, 0.5f, 0, 0, 1, e );
    e.clear();
    p.move( 0, 62, 0.5f, 5, 0, 5, e ); // finger 0 slides onto the key finger 1 holds
    check( count( e, Event::NoteOff, 60 ) == 1 && count( e, Event::NoteOn ) == 0, "sliding onto a held key: old key off, no retrigger" );
    e.clear();
    p.up( 0, e );
    check( e.empty(), "finger 1 still holds 62" );
    p.up( 1, e );
    check( count( e, Event::NoteOff, 62 ) == 1, "62 ends with the last finger" );
}

static void testDragBendKeepsKey()
{
    TouchPiano p; p.cfg.dragMode = DragMode::Bend; Events e;
    p.down( 0, 60, 0.5f, 0, 0, 0, e );
    e.clear();
    p.move( 0, 64, 0.5f, 100, 0, 10, e );
    check( e.empty() && p.noteOfSlot( 0 ) == 60, "bend mode: horizontal slide does not change the note" );
    p.cfg.dragMode = DragMode::Vibrato;
    p.move( 0, 65, 0.5f, 120, 0, 20, e );
    check( e.empty() && p.noteOfSlot( 0 ) == 60, "vibrato mode: same" );
}

static void testVelocityModes()
{
    check( velocityFromPosition( 0.0f, 24 ) == 24, "position top = min velocity" );
    check( velocityFromPosition( 1.0f, 24 ) == 127, "position bottom = 127" );
    check( velocityFromPosition( -3.0f, 24 ) == 24 && velocityFromPosition( 9.0f, 24 ) == 127, "position is clamped" );
    check( velocityFromPosition( 0.0f, 0 ) == 1, "velocity is never 0 (note-off)" );
    check( velocityFromPosition( 0.5f, 24 ) > 24 && velocityFromPosition( 0.5f, 24 ) < 127, "position middle in between" );

    TouchPiano p; Events e;
    p.cfg.velocityMode = VelocityMode::Fixed; p.cfg.fixedVelocity = 77;
    p.down( 0, 60, 0.0f, 0, 0, 0, e );
    check( e[0].velocity == 77, "fixed mode" );
    e.clear(); p.up( 0, e ); e.clear();

    p.cfg.velocityMode = VelocityMode::Position;
    p.down( 0, 60, 1.0f, 0, 0, 0, e );
    check( e[0].velocity == 127, "position mode: bottom of the key" );
    e.clear(); p.up( 0, e ); e.clear();

    p.cfg.velocityMode = VelocityMode::Speed; p.cfg.fixedVelocity = 90;
    p.down( 0, 60, 0.5f, 0, 0, 0, e );
    check( e[0].velocity == 90, "speed mode, a tap has no speed measurement: fixed velocity" );
    e.clear();
    // slow slide: 0.02 px/ms (below speedSlow) onto 62
    p.move( 0, 62, 0.5f, 2, 0, 100, e );
    int slow = e.back().velocity;
    e.clear();
    // fast slide: 3 px/ms (above speedFast) onto 64
    p.move( 0, 64, 0.5f, 302, 0, 200, e );
    int fast = e.back().velocity;
    check( slow == p.cfg.minVelocity, "speed mode: slow glide = min velocity" );
    check( fast == 127, "speed mode: fast glide = 127" );
    check( velocityFromSpeed( 0.8f, p.cfg ) > slow && velocityFromSpeed( 0.8f, p.cfg ) < fast, "speed mode is monotonic" );
}

static void testSpeedTracker()
{
    SpeedTracker s;
    check( s.speed() < 0, "no speed before two samples" );
    s.push( 0, 0, 0 );
    check( s.speed() < 0, "one sample: no speed" );
    s.push( 30, 40, 50 ); // 50 px in 50 ms
    check( std::fabs( s.speed() - 1.0f ) < 1e-4f, "speed = distance / time" );
    s.push( 30, 40, 50 ); // same time stamp: ignored
    check( std::fabs( s.speed() - 1.0f ) < 1e-4f, "same time stamp does not change the speed" );
}

static void testBendMapping()
{
    check( bendFromOffset( 0, 160, 8 ) == 8192, "no offset = centre" );
    check( bendFromOffset( 5, 160, 8 ) == 8192 && bendFromOffset( -8, 160, 8 ) == 8192, "dead zone" );
    check( bendFromOffset( 160, 160, 8 ) == 16383, "full right = max" );
    check( bendFromOffset( -160, 160, 8 ) == 0, "full left = 0" );
    check( bendFromOffset( 1000, 160, 8 ) == 16383 && bendFromOffset( -1000, 160, 8 ) == 0, "clamped" );
    check( bendFromOffset( 84, 160, 8 ) > 8192 && bendFromOffset( 84, 160, 8 ) < 16383, "half way in between" );
    check( bendFromOffset( 50, 160, 8 ) + bendFromOffset( -50, 160, 8 ) >= 16383 - 1 && bendFromOffset( 50, 160, 8 ) + bendFromOffset( -50, 160, 8 ) <= 16384 + 1, "roughly symmetric" );
    check( vibratoFromOffset( 0, 160, 8 ) == 0 && vibratoFromOffset( 160, 160, 8 ) == 127 && vibratoFromOffset( -160, 160, 8 ) == 127, "vibrato magnitude" );
    check( bendFromOffset( 10, 5, 8 ) == 8192, "degenerate full <= dead: centre" );

    ExpressionTracker t;
    t.down( 0, 100 );
    auto o = t.move( 0, 260, DragMode::Bend );
    check( o.changed && o.bendChanged && ! o.vibChanged && o.bend == 16383, "master finger bends" );
    t.down( 1, 500 );
    o = t.move( 1, 700, DragMode::Bend );
    check( ! o.changed && t.bend() == 16383, "second finger is not the master" );
    o = t.up( 0 );
    check( o.changed && o.bend == 8192, "master lifts: the value restarts" );
    o = t.move( 1, 500 + 160, DragMode::Bend );
    check( o.changed && o.bend == 16383, "next master measures from its own anchor" );
    o = t.up( 1 );
    check( o.changed && o.bendChanged && ! o.vibChanged && o.bend == 8192 && t.vibrato() == 0, "all up: spring back to centre" );
    o = t.move( 0, 5, DragMode::Glide );
    check( ! o.changed, "a lifted finger or a non-expression mode produces nothing" );
    t.down( 2, 0 );
    o = t.move( 2, 160, DragMode::Vibrato );
    check( o.changed && o.vibChanged && ! o.bendChanged && o.vibrato == 127 && o.bend == 8192, "vibrato mode sets CC1 only" );
    o = t.up( 2 );
    check( o.changed && o.vibChanged && ! o.bendChanged && o.vibrato == 0, "vibrato returns to 0" );
}

static void testSustain()
{
    TouchPiano p; Events e;
    p.down( 0, 60, 0.5f, 0, 0, 0, e );
    p.sustain( true, e );
    e.clear();
    p.up( 0, e );
    check( e.empty() && p.isSustained( 60 ) && p.isSounding( 60 ), "pedal down: lifting the finger keeps the note" );
    // re-strike a sustained note
    p.down( 1, 60, 0.5f, 0, 0, 10, e );
    check( e.size() == 2 && e[0].type == Event::NoteOff && e[1].type == Event::NoteOn, "re-struck sustained note: off then on (no doubled voice)" );
    check( ! p.isSustained( 60 ), "re-struck note is held by a finger again, not by the pedal" );
    e.clear();
    p.up( 1, e );
    check( e.empty() && p.isSustained( 60 ), "pedal still down: held again" );
    p.down( 2, 67, 0.5f, 0, 0, 20, e );
    e.clear();
    p.sustain( false, e );
    check( count( e, Event::NoteOff, 60 ) == 1 && count( e, Event::NoteOff, 67 ) == 0, "pedal up ends the pedal-held note, not the finger-held one" );
    check( p.noteOfSlot( 2 ) == 67 && p.fingersOn( 67 ) == 1, "finger-held note survives the pedal release" );
    e.clear();
    p.sustain( false, e );
    check( e.empty(), "no edge, no events" );
    p.up( 2, e );
    check( count( e, Event::NoteOff, 67 ) == 1, "finger lifts after the pedal: immediate note-off" );
}

static void testGlissandoWithSustain()
{
    TouchPiano p; Events e;
    p.sustain( true, e );
    p.down( 0, 60, 0.5f, 0, 0, 0, e );
    p.move( 0, 62, 0.5f, 20, 0, 10, e );
    p.move( 0, 64, 0.5f, 40, 0, 20, e );
    check( count( e, Event::NoteOff ) == 0 && count( e, Event::NoteOn ) == 3, "pedal down: a glissando leaves the whole run ringing" );
    p.up( 0, e );
    e.clear();
    p.sustain( false, e );
    check( count( e, Event::NoteOff ) == 3, "pedal up ends all three notes (60, 62, 64), none stuck" );
    for( int n = 0; n < 128; ++n ) check( ! p.isSounding( n ), "nothing sounds" );
}

static void testReleaseAll()
{
    TouchPiano p; Events e;
    p.down( 0, 60, 0.5f, 0, 0, 0, e );
    p.down( 1, 60, 0.5f, 0, 0, 0, e );
    p.down( 2, 64, 0.5f, 0, 0, 0, e );
    p.sustain( true, e );
    p.up( 2, e );
    e.clear();
    p.releaseAll( e );
    check( count( e, Event::NoteOff, 60 ) == 1 && count( e, Event::NoteOff, 64 ) == 1 && e.size() == 2, "releaseAll: one note-off per sounding note" );
    check( ! p.sustainOn(), "releaseAll clears the pedal" );
    e.clear();
    p.up( 0, e ); p.up( 1, e );
    check( e.empty(), "late touch-ups after releaseAll produce nothing" );
}

static void testBadInput()
{
    TouchPiano p; Events e;
    p.down( -1, 60, 0.5f, 0, 0, 0, e );
    p.down( 10, 60, 0.5f, 0, 0, 0, e );
    p.down( 0, -1, 0.5f, 0, 0, 0, e );
    p.down( 0, 128, 0.5f, 0, 0, 0, e );
    p.move( 99, 60, 0.5f, 0, 0, 0, e );
    p.up( 99, e );
    check( e.empty(), "out-of-range slots / notes are ignored" );
}

static void testTransposeAndMarks()
{
    check( soundingPitch( 60, 0, 0 ) == 60 && soundingPitch( 60, 1, 2 ) == 74 && soundingPitch( 60, -3, -12 ) == 12, "sounding pitch" );
    check( soundingPitch( 120, 3, 12 ) == 127 && soundingPitch( 5, -3, -12 ) == 0, "sounding pitch clamps like KeyTranspose" );
    check( keyForSoundingPitch( 74, 1, 2 ) == 60 && keyForSoundingPitch( 5, 3, 0 ) == -1, "inverse mapping" );

    const std::vector<int> major{ 2, 2, 1, 2, 2, 2, 1 };
    auto pc = scalePitchClasses( 0, major );
    int n = 0; for( bool b : pc ) n += b;
    check( n == 7 && pc[0] && pc[2] && pc[4] && pc[5] && pc[7] && pc[9] && pc[11] && ! pc[1], "C major pitch classes" );
    auto gpc = scalePitchClasses( 7, major );
    check( gpc[7] && gpc[11] && gpc[6] && ! gpc[5], "G major has F#, not F" );
    check( scalePitchClasses( 0, {} ).size() == 12, "empty formula: no scale" );

    check( markFor( 60, 0, pc, -1, {} ) == KeyMark::Root, "C is the root" );
    check( markFor( 64, 0, pc, -1, {} ) == KeyMark::InScale && markFor( 61, 0, pc, -1, {} ) == KeyMark::None, "in scale / out of scale" );
    check( markFor( 67, 0, pc, 60, { 4, 7 } ) == KeyMark::Chord && markFor( 61, 0, pc, 60, { 4, 7 } ) == KeyMark::None, "chord tones" );
    check( markFor( 62, 0, {}, -1, {} ) == KeyMark::None, "no scale: no marks" );
}

static void testWhiteKeyGeometry()
{
    // The white keys tile the octave: contiguous, no gap, no overlap (the old hit test covered only 1 unit of 1.5 / 2).
    const int pcs[7] = { 0, 2, 4, 5, 7, 9, 11 };
    float prevEnd = 0.0f;
    for( int pc : pcs )
    {
        float s = -1, e = -1;
        check( whiteKeyUnits( pc, s, e ), "white pitch class" );
        check( std::fabs( s - prevEnd ) < 1e-6f, "white keys are contiguous" );
        check( e - s >= 1.5f - 1e-6f && e - s <= 2.0f + 1e-6f, "white key is 1.5 or 2 units wide" );
        prevEnd = e;
    }
    check( std::fabs( prevEnd - 12.0f ) < 1e-6f, "the seven white keys span the 12 units" );
    float s, e;
    check( ! whiteKeyUnits( 1, s, e ) && ! whiteKeyUnits( 6, s, e ), "black keys are not white keys" );
    // Every x of the octave hits a white key (below the black keys); the old 1-unit cells left holes.
    int holes = 0;
    for( float u = 0.0f; u < 12.0f; u += 0.05f )
        if( whitePcAtUnit( u ) < 0 ) ++holes;
    check( holes == 0, "no hole in the white key row" );
    check( whitePcAtUnit( 14.6f - 12.0f ) == 2, "x = 14.6 units (the MPC second-finger touch) is D, not a miss" );
    check( whitePcAtUnit( 2.9f ) == 2 && whitePcAtUnit( 3.4f ) == 2 && whitePcAtUnit( 3.5f ) == 4, "D owns 1.5..3.5" );
    check( whitePcAtUnit( 12.0f ) == -1 && whitePcAtUnit( -0.1f ) == -1, "outside the octave" );
}

int main()
{
    testWhiteKeyGeometry();
    testTwoFingers();
    testSameNoteTwoFingers();
    testLostTouchUp();
    testGlissandoSlide();
    testGlissandoLegato();
    testGlissandoOverHeldKey();
    testDragBendKeepsKey();
    testVelocityModes();
    testSpeedTracker();
    testBendMapping();
    testSustain();
    testGlissandoWithSustain();
    testReleaseAll();
    testBadInput();
    testTransposeAndMarks();
    std::puts( "OK" );
    return 0;
}
