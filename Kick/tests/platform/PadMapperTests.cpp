// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the Pad Perform pure logic: layout (Chromatic / Notes / Start on Root / banks), chord formulas,
// diatonic Progression, the held-note table (note-off = what the press started), stored Off / Chromatic Chords
// modes, 16 LEVELS maths and the chord name.

#include "../../modules/kick_ui/modes/padperform/kick_pad_mapper.h"
#include <cstdio>
#include <cstdlib>

using namespace kick_ui::padmap;

static void check( bool ok, const char* why )
{
    if( !ok ) { std::fprintf( stderr, "FAIL: %s\n", why ); std::exit( 1 ); }
}

static const std::vector<int> kMajor = { 2, 2, 1, 2, 2, 2, 1 };
static const std::vector<int> kMinor = { 2, 1, 2, 2, 1, 2, 2 };

static Params base( int mode, std::vector<int> scale = kMajor )
{
    Params p; p.mode = mode; p.scale = scale; return p;
}

static void testChromatic()
{
    Params p = base( Chromatic );
    auto   l = layout( p );
    check( l[0].note == 0 && l[1].note == 1 && l[15].note == 15, "A01 = C-2, one semitone per pad" );
    check( l[16].note == 16, "bank B continues in 16 steps (bank offset)" );
    check( l[127].note == 127, "last slot = 127" );
    check( l[0].mark == Mark::Root && l[1].mark == Mark::Inactive && l[2].mark == Mark::Active, "C root, C# out of scale, D in scale" );
    p.octaveIdx = 8; // C6
    l = layout( p );
    check( l[0].note == 96 && l[31].note == 127 && l[32].note == -1 && l[127].note == -1, "no note above 127: the slots end, nothing wraps" );
    check( l[32].mark == Mark::NotAvailable, "slots beyond 127 are not available" );
    p.octaveIdx = 0; p.rootPc = 2; // D
    l = layout( p );
    check( l[0].note == 2 && l[0].mark == Mark::Root, "root D starts on D" );
}

static void testNotes()
{
    Params p = base( Notes );
    p.octaveIdx = 3; // C1 = 36
    auto l = layout( p );
    const int expect[8] = { 36, 38, 40, 41, 43, 45, 47, 48 };
    for( int i = 0; i < 8; ++i ) check( l[(size_t)i].note == expect[i], "Notes C major: C D E F G A B C" );
    check( l[0].mark == Mark::Root && l[1].mark == Mark::Active && l[7].mark == Mark::Root, "root every octave" );
    p.scale = kMinor; p.rootPc = 9; // A natural minor
    l = layout( p );
    check( l[0].note == 45 && l[1].note == 47 && l[2].note == 48 && l[7].note == 57, "A minor A B C ... A" );
}

static void testStartOnRoot()
{
    Params p = base( Notes ); p.startOnRoot = true;
    auto   l = layout( p );
    check( l[15].note == 26 && l[16].note == 24, "Notes: bank B starts on a root again (C+2 octaves)" );
    check( l[16].mark == Mark::Root && l[32].note == 48, "root marks at the bank start, bank C = +4 octaves" );
    p.mode = Chromatic;
    l = layout( p );
    check( l[16].note == 12 && l[32].note == 24, "Chromatic: every bank starts on the root octave" );
    p = base( Notes, { 2, 2, 3, 2, 3 } ); p.startOnRoot = true; // pentatonic: 16 pads = 3 octaves + 1
    l = layout( p );
    check( l[16].mark == Mark::Root && l[16].note % 12 == 0, "pentatonic bank start is a root too" );
}

static void testChordFormulas()
{
    check( chordSteps().at( "1-3-5" ) == std::vector<int>( { 4, 7 } ), "1-3-5 = major triad" );
    check( chordSteps().at( "1-3-5-7" ) == std::vector<int>( { 4, 7, 11 } ), "1-3-5-7 uses a major THIRD (4), not a fourth" );
    check( chordSteps().at( "1-3-5-7b" ) == std::vector<int>( { 4, 7, 10 } ), "1-3-5-7b = dominant 7th" );
    Params p = base( Chords ); p.chord = "1-3-5-7"; p.octaveIdx = 3;
    auto l = layout( p );
    auto n = notesForPad( l, p, 0 );
    check( n == std::vector<int>( { 36, 40, 43, 47 } ), "Chords 1-3-5-7 on C1: C E G B (reproduction of B1)" );
    p.octaveIdx = 10; p.rootPc = 7; // G8 = 127
    l = layout( p );
    n = notesForPad( l, p, 0 );
    check( n == std::vector<int>( { 127 } ), "chord tones above 127 are dropped, the root alone sounds" );
    p.octaveIdx = 9; p.rootPc = 11; // B7 = 119
    l = layout( p );
    p.chord = "1-3-5-7";
    n = notesForPad( l, p, 0 );
    check( n.size() == 3 && n.back() == 126, "119: 123 and 126 fit, 130 is dropped" );
    check( notesForPad( l, p, 127 ).empty() || l[127].note < 0, "an unmapped slot plays nothing" );
}

static void testProgression()
{
    Params p = base( Progression ); p.octaveIdx = 3; p.chord = "1-3-5";
    auto l = layout( p );
    check( notesForPad( l, p, 0 ) == std::vector<int>( { 36, 40, 43 } ), "I = C E G" );
    check( notesForPad( l, p, 1 ) == std::vector<int>( { 38, 41, 45 } ), "ii = D F A (minor, from the scale)" );
    check( notesForPad( l, p, 6 ) == std::vector<int>( { 47, 50, 53 } ), "vii = B D F (diminished)" );
    p.chord = "1-3-5-7";
    check( notesForPad( l, p, 0 ) == std::vector<int>( { 36, 40, 43, 47 } ), "Imaj7" );
    check( notesForPad( l, p, 4 ) == std::vector<int>( { 43, 47, 50, 53 } ), "V7 = G B D F in C major" );
    p.chord = "1-3-5-7b";
    check( notesForPad( l, p, 0 ) == std::vector<int>( { 36, 40, 43, 46 } ), "7b lowers a major 7th to the dominant 7th" );
    p.chord = "1-3-5"; p.scale = kMinor; p.rootPc = 9; l = layout( p );
    check( notesForPad( l, p, 0 ) == std::vector<int>( { 45, 48, 52 } ), "A minor: i = A C E" );
    Params q = base( Progression ); q.octaveIdx = 10; q.rootPc = 7; // 127
    l = layout( q );
    check( notesForPad( l, q, 0 ) == std::vector<int>( { 127 } ), "progression near the top drops what does not fit" );
}

static void testStoredModes()
{
    check( normaliseMode( Off ) == Chromatic && normaliseMode( ChromaticChords ) == Chromatic, "stored Off / Chromatic Chords play Chromatic" );
    check( normaliseMode( 77 ) == Chromatic && normaliseMode( -3 ) == Chromatic, "garbage modes too" );
    Params p = base( Off ); auto l = layout( p );
    bool anyZero = false; int distinct = 0;
    for( int i = 0; i < 16; ++i ) { if( l[(size_t)i].note == 0 && i > 0 ) anyZero = true; if( i == 0 || l[(size_t)i].note != l[(size_t)i - 1].note ) ++distinct; }
    check( ! anyZero && distinct == 16, "stored Off mode: pads are not all note 0 (B2)" );
    Params c = base( ChromaticChords ); c.chord = "1-3-5";
    check( notesForPad( layout( c ), c, 3 ).size() == 1, "Chromatic Chords has no chord of its own" );
}

static void testHeld()
{
    HeldNotes h;
    Params    p = base( Chords ); p.octaveIdx = 3; p.chord = "1-3-5";
    auto      l = layout( p );
    auto      on = notesForPad( l, p, 0 );
    check( h.press( 0, on ).empty(), "first press switches nothing off" );
    // The mapping changes while the pad is held (bank / scale / root / chord).
    p.rootPc = 5; p.chord = "1-4-5"; p.mode = Notes;
    auto l2 = layout( p );
    check( notesForPad( l2, p, 0 ) != on, "mapping really changed" );
    auto off = h.release( 0 );
    check( off.size() == 3 && off[0] == 36 && off[1] == 40 && off[2] == 43, "release ends exactly the notes of the press (B7)" );
    check( h.release( 0 ).empty() && h.heldKeys() == 0, "double release is harmless" );

    // Same pitch from two pads: the note stays until both are up.
    h.press( 1, { 60, 64 } ); h.press( 2, { 64, 67 } );
    check( h.release( 1 ) == std::vector<int>( { 60 } ), "64 is still held by the second pad" );
    check( h.release( 2 ) == std::vector<int>( { 64, 67 } ), "second pad ends 64 and 67" );

    // A lost release: the next press of the same key ends the old notes first.
    h.press( 3, { 50 } );
    check( h.press( 3, { 52 } ) == std::vector<int>( { 50 } ), "re-press without release ends the old note" );
    h.press( fingerKey( 0 ), { 70 } );
    auto all = h.releaseAll();
    check( all == std::vector<int>( { 52, 70 } ) && h.heldKeys() == 0 && h.soundingNotes().empty(), "panic ends every note that is on" );
    check( h.releaseAll().empty(), "nothing left" );
}

static void testLevels()
{
    check( levelVelocity( 0 ) == 8 && levelVelocity( 15 ) == 127 && levelVelocity( 7 ) == 64, "16 LEVELS velocity ramp 8..127" );
    for( int i = 1; i < 16; ++i ) check( levelVelocity( i ) > levelVelocity( i - 1 ), "velocity strictly rising" );
    check( levelVelocity( -4 ) == 8 && levelVelocity( 99 ) == 127, "clamped" );
    auto v = levelFor( LevelParam::Velocity, 36, 5 );
    check( v.note == 36 && v.velocity == levelVelocity( 5 ), "Velocity: same note on every pad" );
    auto t = levelFor( LevelParam::Tune, 36, 0 );
    check( t.note == 28 && t.velocity < 0, "Tune: pad 1 = -8 semitones, hit velocity kept" );
    check( levelFor( LevelParam::Tune, 36, 8 ).note == 36 && levelFor( LevelParam::Tune, 36, 15 ).note == 43, "pad 9 = base, pad 16 = +7" );
    check( levelFor( LevelParam::Tune, 3, 0 ).note == 0 && levelFor( LevelParam::Tune, 125, 15 ).note == 127, "tune clamps to 0..127" );
}

static void testChordName()
{
    check( chordName( { 60, 64, 67 }, false ) == "C", "C major" );
    check( chordName( { 60, 64, 67, 71 }, false ) == "Cmaj7", "Cmaj7" );
    check( chordName( { 55, 59, 62, 65 }, false ) == "G7", "G7" );
    check( chordName( { 57, 60, 64 }, false ) == "Am", "A minor" );
    check( chordName( { 64, 67, 72 }, false ) == "C", "first inversion is still named C" );
    check( chordName( { 61, 65, 68 }, true ) == "Db" && chordName( { 61, 65, 68 }, false ) == "C#", "flat / sharp names" );
    check( chordName( { 60, 64 }, false ).empty() && chordName( { 60, 61, 62 }, false ).empty(), "no name for two notes / clusters" );
}

int main()
{
    testChromatic();
    testNotes();
    testStartOnRoot();
    testChordFormulas();
    testProgression();
    testStoredModes();
    testHeld();
    testLevels();
    testChordName();
    std::puts( "PadMapperTests: OK" );
    return 0;
}
