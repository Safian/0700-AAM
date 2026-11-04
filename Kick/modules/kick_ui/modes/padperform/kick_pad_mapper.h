// SPDX-License-Identifier: AGPL-3.0-or-later
// Pad Perform pure logic (JUCE-free, header only; host test: Kick/tests/platform/PadMapperTests.cpp).
//
// One source for what a pad plays: the layout of the 128 pad slots (note + root / scale mark), the notes of a chord
// pad, the held-note table (a note-off always ends exactly what the matching press started), and the 16 LEVELS maths.
// The labels, the pad colours (screen + hardware LEDs) and the sounds of the Pad Perform screen all derive from it.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace kick_ui::padmap
{
    // Same numbering as mt::PerformanceMode / Controls::PadPerformType.
    enum Mode
    {
        Off = 0,
        Chromatic,
        Notes,
        Chords,
        Progression,
        ChromaticChords
    };

    enum class Mark
    {
        NotAvailable,
        Inactive,
        Root,
        Active
    };

    constexpr int kPads    = 128; // 8 banks x 16 pads
    constexpr int kMaxNote = 127;

    // Off and Chromatic Chords have no button and no behaviour of their own: a project that stored one of them plays
    // (and shows) Chromatic, so every pad maps to a note instead of all of them sounding note 0.
    inline int normaliseMode( int mode )
    {
        return mode >= Chromatic && mode <= Progression ? mode : (int)Chromatic;
    }

    // Semitone offsets above the pad note, the pad note itself implied (mt::ScaleAndNotes::chordFormulaMap).
    // Four-note chords stack THIRDS: 1-3-5-7 = major 7th (4,7,11), 1-3-5-7b = dominant 7th (4,7,10).
    inline const std::map<std::string, std::vector<int>>& chordSteps()
    {
        static const std::map<std::string, std::vector<int>> m = {
            { "1-3-5", { 4, 7 } },
            { "1-4-5", { 5, 7 } },
            { "1-2-5", { 2, 7 } },
            { "1-3-5-7", { 4, 7, 11 } },
            { "1-3-5-7b", { 4, 7, 10 } },
        };
        return m;
    }

    // Scale degrees (0 = the pad's own degree) stacked for the diatonic Progression chords. 1-3-5 = degrees 0,2,4 etc.
    inline std::vector<int> chordDegrees( const std::string& name )
    {
        if( name == "1-3-5" ) return { 0, 2, 4 };
        if( name == "1-4-5" ) return { 0, 3, 4 };
        if( name == "1-2-5" ) return { 0, 1, 4 };
        if( name == "1-3-5-7" || name == "1-3-5-7b" ) return { 0, 2, 4, 6 };
        return { 0, 2, 4 };
    }

    struct Params
    {
        int              mode        = Chromatic;
        int              rootPc      = 0; // 0..11, C = 0
        int              octaveIdx   = 0; // index of the "-2".."8" list: the lowest pad starts at rootPc + 12 * octaveIdx
        std::vector<int> scale       = { 2, 2, 1, 2, 2, 2, 1 }; // semitone steps, repeating (mt::ScaleAndNotes::scaleFormulaMap)
        bool             startOnRoot = false; // "Banks: Start on Root"
        std::string      chord       = "1-3-5";
    };

    struct Pad
    {
        int  note = -1; // MIDI note of the slot, -1 = no note (beyond 127)
        Mark mark = Mark::NotAvailable;
    };
    using Layout = std::array<Pad, kPads>;

    // The 128 pad slots in order (slot = bank * 16 + pad). Chromatic walks semitones; Notes / Chords / Progression walk
    // the scale only. The root / scale marks follow the scale in every mode (Chromatic shows the scale as colour).
    inline Layout layout( const Params& p )
    {
        Layout out;
        if( p.scale.empty() ) return out;
        const int  mode    = normaliseMode( p.mode );
        const bool onlyScale = mode == Notes || mode == Chords || mode == Progression;
        const int  base    = std::max( 0, p.rootPc % 12 ) + 12 * std::max( 0, p.octaveIdx );

        int noteIndex = base, scaleIdx = base, scaleIt = 0;
        for( int i = 0; i < kPads; ++i )
        {
            if( p.startOnRoot && i % 16 == 0 && i != 0 )
            {
                // Every bank restarts on a root: snap down to the nearest root octave (12 * k above the start).
                const int k = std::max( 0, ( noteIndex - base ) / 12 );
                noteIndex   = base + 12 * k;
                scaleIdx    = noteIndex;
                scaleIt     = 0;
            }
            if( noteIndex < 0 || noteIndex > kMaxNote ) break;

            Pad& pad = out[(size_t)i];
            pad.note = noteIndex;
            if( noteIndex == scaleIdx )
            {
                pad.mark       = scaleIt == 0 ? Mark::Root : Mark::Active;
                const int step = std::max( 1, p.scale[(size_t)scaleIt] );
                scaleIdx += step;
                if( onlyScale ) noteIndex += step - 1; // incremented once more below
                if( ++scaleIt >= (int)p.scale.size() ) scaleIt = 0;
            }
            else
                pad.mark = Mark::Inactive;

            ++noteIndex;
        }
        return out;
    }

    // The notes of the scale from the lowest pad note upward (the walk the Notes layout follows).
    inline std::vector<int> scaleWalk( const Params& p )
    {
        std::vector<int> notes;
        if( p.scale.empty() ) return notes;
        int n = std::max( 0, p.rootPc % 12 ) + 12 * std::max( 0, p.octaveIdx ), it = 0;
        while( n <= kMaxNote )
        {
            notes.push_back( n );
            n += std::max( 1, p.scale[(size_t)it] );
            if( ++it >= (int)p.scale.size() ) it = 0;
        }
        return notes;
    }

    // All the notes pad slot `slot` plays: the pad note, plus the chord above it in Chords (fixed intervals) and
    // Progression (diatonic: scale degrees stacked in thirds, so the quality follows the scale). Notes above 127 are
    // dropped, never wrapped. Empty = the slot has no note.
    inline std::vector<int> notesForPad( const Layout& l, const Params& p, int slot )
    {
        std::vector<int> out;
        if( slot < 0 || slot >= kPads || l[(size_t)slot].note < 0 ) return out;
        const int note = l[(size_t)slot].note;
        out.push_back( note );

        const int mode = normaliseMode( p.mode );
        if( mode == Chords )
        {
            auto it = chordSteps().find( p.chord );
            if( it != chordSteps().end() )
                for( int s : it->second )
                    if( note + s <= kMaxNote ) out.push_back( note + s );
        }
        else if( mode == Progression )
        {
            const auto walk = scaleWalk( p );
            const auto at   = std::find( walk.begin(), walk.end(), note );
            if( at != walk.end() )
            {
                const int degree = (int)( at - walk.begin() );
                for( int d : chordDegrees( p.chord ) )
                {
                    if( d == 0 ) continue;
                    if( degree + d >= (int)walk.size() ) break;
                    int n = walk[(size_t)( degree + d )];
                    if( p.chord == "1-3-5-7b" && d == 6 && n - note == 11 ) n -= 1; // dominant 7th
                    out.push_back( n );
                }
            }
            else
            {
                // A pad that is not on the scale walk (cannot happen in the Notes layout): fixed fallback, major triad.
                for( int s : { 4, 7 } )
                    if( note + s <= kMaxNote ) out.push_back( note + s );
            }
        }
        return out;
    }

    // ---- held notes ---------------------------------------------------------------------------------------------

    // Which notes each finger / pad started, kept until it lets go: the note-off is computed from the press, not from
    // the mapping of the moment (bank / scale / root changes while a pad is held no longer leave a note stuck).
    // Notes are reference counted: two pads that map to the same pitch keep it on until both are released.
    class HeldNotes
    {
    public:
        // Registers the notes a key started. A key that is still held (a lost release) is released first: the returned
        // notes are those that must be switched off before the new press starts.
        std::vector<int> press( int key, const std::vector<int>& notes )
        {
            std::vector<int> off = release( key );
            held_[key]           = notes;
            for( int n : notes )
                if( n >= 0 && n <= kMaxNote ) ++count_[(size_t)n];
            return off;
        }

        // Notes to switch off for this key (only those no other key still holds).
        std::vector<int> release( int key )
        {
            std::vector<int> off;
            auto             it = held_.find( key );
            if( it == held_.end() ) return off;
            for( int n : it->second )
                if( n >= 0 && n <= kMaxNote && count_[(size_t)n] > 0 && --count_[(size_t)n] == 0 ) off.push_back( n );
            held_.erase( it );
            return off;
        }

        // Panic / screen hidden / transport stop: every note that is still on.
        std::vector<int> releaseAll()
        {
            std::vector<int> off;
            for( int n = 0; n <= kMaxNote; ++n )
                if( count_[(size_t)n] > 0 ) off.push_back( n );
            held_.clear();
            count_.fill( 0 );
            return off;
        }

        bool isHeld( int key ) const { return held_.find( key ) != held_.end(); }
        int  heldKeys() const { return (int)held_.size(); }
        int  count( int note ) const { return note >= 0 && note <= kMaxNote ? count_[(size_t)note] : 0; }
        const std::vector<int>* notesFor( int key ) const
        {
            auto it = held_.find( key );
            return it != held_.end() ? &it->second : nullptr;
        }

        // Pitch classes that sound (0..11), for the chord-name display.
        std::vector<int> soundingNotes() const
        {
            std::vector<int> n;
            for( int i = 0; i <= kMaxNote; ++i )
                if( count_[(size_t)i] > 0 ) n.push_back( i );
            return n;
        }

    private:
        std::map<int, std::vector<int>> held_;
        std::array<int, kPads>          count_{};
    };

    // Held pads and touch fingers share one table: hardware pads use their slot, fingers use a key above the pad range.
    constexpr int fingerKey( int finger ) { return 1000 + finger; }

    // ---- chord name ---------------------------------------------------------------------------------------------

    // "Cmaj7", "Am", "G7" ... of the sounding notes; empty when it is not a recognisable chord (or < 3 notes).
    inline std::string chordName( std::vector<int> notes, bool flats )
    {
        static const char* const sharp[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        static const char* const flat[]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
        if( notes.size() < 3 ) return {};
        std::sort( notes.begin(), notes.end() );
        struct Q { const char* suffix; std::vector<int> iv; };
        static const Q qualities[] = {
            { "", { 0, 4, 7 } },        { "m", { 0, 3, 7 } },      { "dim", { 0, 3, 6 } },   { "aug", { 0, 4, 8 } },
            { "sus2", { 0, 2, 7 } },    { "sus4", { 0, 5, 7 } },   { "maj7", { 0, 4, 7, 11 } }, { "7", { 0, 4, 7, 10 } },
            { "m7", { 0, 3, 7, 10 } },  { "m7b5", { 0, 3, 6, 10 } }, { "dim7", { 0, 3, 6, 9 } },
        };
        // Try every sounding pitch class as the root (the lowest one first, so inversions name after the bass note).
        std::vector<int> pcs;
        for( int n : notes )
        {
            const int pc = n % 12;
            if( std::find( pcs.begin(), pcs.end(), pc ) == pcs.end() ) pcs.push_back( pc );
        }
        for( int root : pcs )
        {
            std::vector<int> iv;
            for( int pc : pcs ) iv.push_back( ( pc - root + 12 ) % 12 );
            std::sort( iv.begin(), iv.end() );
            for( const Q& q : qualities )
                if( iv == q.iv ) return std::string( flats ? flat[root] : sharp[root] ) + q.suffix;
        }
        return {};
    }

    // ---- 16 LEVELS ----------------------------------------------------------------------------------------------

    // The stepped parameter. Layer / Filter / Attack / Decay need a voice-side hook that does not exist for live notes.
    enum class LevelParam
    {
        Velocity,
        Tune
    };

    inline int levelVelocity( int pad )
    {
        const int k = std::max( 0, std::min( 15, pad ) );
        return std::max( 1, std::min( 127, (int)std::lround( ( k + 1 ) * 127.0 / 16.0 ) ) );
    }

    // Tune: pad 9 plays the base note, the others a semitone per pad below / above it (pad 1 = -8, pad 16 = +7).
    inline int levelNote( int baseNote, int pad ) { return std::max( 0, std::min( kMaxNote, baseNote + ( std::max( 0, std::min( 15, pad ) ) - 8 ) ) ); }

    struct Level
    {
        int note;
        int velocity; // < 0 = use the velocity of the hit
    };

    inline Level levelFor( LevelParam param, int baseNote, int pad )
    {
        if( param == LevelParam::Velocity ) return { std::max( 0, std::min( kMaxNote, baseNote ) ), levelVelocity( pad ) };
        return { levelNote( baseNote, pad ), -1 };
    }

    // PP-40: Scale degree of a note in the active scale (1-based, 1..N, or 0 if non-scale / out of range).
    inline int scaleDegree( int note, int rootPc, const std::vector<int>& scale )
    {
        if( note < 0 || scale.empty() ) return 0;
        const int targetPc = ( ( note - rootPc ) % 12 + 12 ) % 12;
        int acc = 0;
        for( size_t i = 0; i < scale.size(); ++i )
        {
            if( acc == targetPc )
                return (int)i + 1;
            acc += scale[i];
        }
        return 0;
    }

    // PP-24: Touch strip position (0..127) mapped to the Pad Perform layout's sounding note for the bank.
    inline int noteForStrip( int val127, const Layout& layout, int bankOffset )
    {
        if( val127 < 0 ) val127 = 0;
        if( val127 > 127 ) val127 = 127;
        const int slot = ( val127 * 16 ) / 128;
        const int padIndex = bankOffset + ( slot >= 16 ? 15 : slot );
        if( padIndex >= 0 && padIndex < kPads )
        {
            const int note = layout[(size_t)padIndex].note;
            if( note >= 0 ) return note;
        }
        return std::max( 0, std::min( kMaxNote, bankOffset + ( slot >= 16 ? 15 : slot ) ) );
    }
}
