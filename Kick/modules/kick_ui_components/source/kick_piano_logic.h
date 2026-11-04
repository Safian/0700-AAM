// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// Pure (JUCE-free, header only) logic of the touch piano: multi-finger note state machine, glissando (slide / legato),
// velocity modes (fixed / key position / finger speed), pitch bend + vibrato mapping, sustain held-note logic, the
// shared transposition maths and the scale/chord marking helper. The widget (kick_KeyboardComponent) and the screen
// (kick_PianoView) only translate touches and engine calls to / from this; host tests: Kick/tests/platform/PianoLogicTests.cpp.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace ui::piano
{
    constexpr int kSlots = 10; // MT slots of the ILI2117 touch panel

    // ---- velocity -----------------------------------------------------------------------------------------------

    enum class VelocityMode
    {
        Fixed,    // always cfg.fixedVelocity
        Position, // where on the key the finger lands: top = soft (minVelocity), bottom = hard (127)
        Speed     // finger speed (from the touch event time stamps): slow = soft, fast = hard
    };

    // What a horizontal slide does after the finger went down on a key.
    enum class DragMode
    {
        Glide,   // glissando, the note moves with the finger (note-off of the old key, then note-on of the new one)
        Legato,  // glissando, the new note starts before the old one ends (overlap, for mono / legato voices)
        Bend,    // the finger stays on its key, the horizontal offset bends the pitch
        Vibrato  // the finger stays on its key, the horizontal offset sets the mod wheel (CC1)
    };

    struct Config
    {
        VelocityMode velocityMode  = VelocityMode::Position;
        DragMode     dragMode      = DragMode::Glide;
        int          fixedVelocity = 100; // 1..127
        int          minVelocity   = 24;  // floor of the position / speed modes (never 0: velocity 0 would be a note-off)
        float        speedSlow     = 0.05f; // px/ms that still counts as the softest speed
        float        speedFast     = 1.50f; // px/ms that counts as the hardest speed
    };

    inline int clampVelocity( int v ) { return std::max( 1, std::min( 127, v ) ); }

    // yFrac: 0 = top edge of the key, 1 = bottom edge.
    inline int velocityFromPosition( float yFrac, int minVelocity )
    {
        const float f   = std::max( 0.0f, std::min( 1.0f, yFrac ) );
        const int   mn  = clampVelocity( minVelocity );
        return clampVelocity( mn + (int)std::lround( f * (float)( 127 - mn ) ) );
    }

    // pxPerMs < 0 = no speed measured. Linear between speedSlow (minVelocity) and speedFast (127).
    inline int velocityFromSpeed( float pxPerMs, const Config& c )
    {
        if( pxPerMs < 0.0f ) return clampVelocity( c.fixedVelocity );
        const float span = std::max( 1e-6f, c.speedFast - c.speedSlow );
        const float f    = std::max( 0.0f, std::min( 1.0f, ( pxPerMs - c.speedSlow ) / span ) );
        const int   mn   = clampVelocity( c.minVelocity );
        return clampVelocity( mn + (int)std::lround( f * (float)( 127 - mn ) ) );
    }

    // Finger speed from the time stamps of consecutive touch events (px per ms, smoothed). speed() < 0 until two
    // samples with a time difference exist.
    class SpeedTracker
    {
    public:
        void reset() { m_n = 0; m_speed = -1.0f; }
        void push( float x, float y, int64_t tMs )
        {
            if( m_n > 0 && tMs > m_t )
            {
                const float d = std::hypot( x - m_x, y - m_y ) / (float)( tMs - m_t );
                m_speed       = m_speed < 0.0f ? d : 0.5f * m_speed + 0.5f * d;
            }
            if( m_n == 0 || tMs > m_t ) { m_x = x; m_y = y; m_t = tMs; }
            ++m_n;
        }
        float speed() const { return m_speed; }

    private:
        int     m_n     = 0;
        float   m_x = 0, m_y = 0;
        int64_t m_t     = 0;
        float   m_speed = -1.0f;
    };

    // ---- bend / vibrato -----------------------------------------------------------------------------------------

    constexpr int kBendCentre = 8192;
    constexpr int kBendMax    = 16383;

    // dx = horizontal offset from where the finger went down (px). Within deadPx nothing happens; fullPx = full deflection.
    inline float offsetNorm( float dx, float fullPx, float deadPx )
    {
        const float a = std::fabs( dx );
        if( a <= deadPx || fullPx <= deadPx ) return 0.0f;
        const float n = std::min( 1.0f, ( a - deadPx ) / ( fullPx - deadPx ) );
        return dx < 0.0f ? -n : n;
    }
    inline int bendFromOffset( float dx, float fullPx, float deadPx )
    {
        const float n = offsetNorm( dx, fullPx, deadPx );
        const int   v = kBendCentre + (int)std::lround( n * ( n > 0.0f ? (float)( kBendMax - kBendCentre ) : (float)kBendCentre ) );
        return std::max( 0, std::min( kBendMax, v ) );
    }
    inline int vibratoFromOffset( float dx, float fullPx, float deadPx )
    {
        return std::max( 0, std::min( 127, (int)std::lround( std::fabs( offsetNorm( dx, fullPx, deadPx ) ) * 127.0f ) ) );
    }

    // Bend / vibrato state over all fingers: the master is the oldest finger still down (a pitch wheel is one value per
    // channel). When no finger is left the bend springs back to the centre and the vibrato to 0.
    class ExpressionTracker
    {
    public:
        struct Out
        {
            bool changed     = false; // bendChanged || vibChanged
            bool bendChanged = false;
            bool vibChanged  = false;
            int  bend        = kBendCentre;
            int  vibrato = 0;
        };

        float fullPx = 160.0f;
        float deadPx = 8.0f;

        void down( int slot, float x )
        {
            if( slot < 0 || slot >= kSlots ) return;
            m_down[slot] = true;
            m_ax[slot]   = x;
            m_order[slot] = ++m_seq;
        }
        // mode must be Bend or Vibrato, anything else produces nothing.
        Out move( int slot, float x, DragMode mode )
        {
            Out o;
            if( slot < 0 || slot >= kSlots || ! m_down[slot] || slot != master() ) return o;
            if( mode == DragMode::Bend )
            {
                const int b = bendFromOffset( x - m_ax[slot], fullPx, deadPx );
                o.bendChanged = b != m_bend;
                o.changed     = o.bendChanged;
                m_bend        = b;
            }
            else if( mode == DragMode::Vibrato )
            {
                const int v = vibratoFromOffset( x - m_ax[slot], fullPx, deadPx );
                o.vibChanged = v != m_vib;
                o.changed    = o.vibChanged;
                m_vib        = v;
            }
            o.bend = m_bend; o.vibrato = m_vib;
            return o;
        }
        Out up( int slot )
        {
            Out o;
            if( slot < 0 || slot >= kSlots || ! m_down[slot] ) return o;
            const bool wasMaster = slot == master();
            m_down[slot]         = false;
            // No finger left, or the master left: the value restarts (centre / 0); the next master measures from its own anchor.
            if( ( master() < 0 || wasMaster ) && ( m_bend != kBendCentre || m_vib != 0 ) )
            {
                o.bendChanged = m_bend != kBendCentre;
                o.vibChanged  = m_vib != 0;
                m_bend = kBendCentre; m_vib = 0;
                o.changed = true;
            }
            o.bend = m_bend; o.vibrato = m_vib;
            return o;
        }
        int bend() const { return m_bend; }
        int vibrato() const { return m_vib; }

    private:
        int master() const
        {
            int best = -1;
            for( int i = 0; i < kSlots; ++i )
                if( m_down[i] && ( best < 0 || m_order[i] < m_order[best] ) ) best = i;
            return best;
        }
        bool m_down[kSlots]  = {};
        float m_ax[kSlots]   = {};
        int  m_order[kSlots] = {};
        int  m_seq           = 0;
        int  m_bend          = kBendCentre;
        int  m_vib           = 0;
    };

    // ---- shared transposition (mirror of KeyTranspose::soundingPitch: octave * 12 + semitone, clamped to 0..127) ---

    inline int soundingPitch( int key, int octave, int semitone )
    {
        return std::max( 0, std::min( 127, key + octave * 12 + semitone ) );
    }

    // ---- multi-finger note state machine ------------------------------------------------------------------------

    struct Event
    {
        enum Type { NoteOn, NoteOff } type = NoteOn;
        int note     = 0; // key number (before transposition)
        int velocity = 0; // note-on only
        int slot     = -1;
    };
    using Events = std::vector<Event>;

    class TouchPiano
    {
    public:
        Config cfg;

        // Finger `slot` touched down on `note` (-1 = no key under the finger). yFrac: 0 top .. 1 bottom of the key.
        void down( int slot, int note, float yFrac, float x, float y, int64_t tMs, Events& out )
        {
            if( slot < 0 || slot >= kSlots ) return;
            if( m_note[slot] >= 0 ) up( slot, out ); // a lost touch-up must not leave a note hanging
            m_speed[slot].reset();
            m_speed[slot].push( x, y, tMs );
            if( note < 0 || note > 127 ) return;
            attach( slot, note, velocityFor( slot, yFrac ), out );
        }

        // Finger moved (event after the touch slop is handled by the caller).
        void move( int slot, int note, float yFrac, float x, float y, int64_t tMs, Events& out )
        {
            if( slot < 0 || slot >= kSlots ) return;
            m_speed[slot].push( x, y, tMs );
            if( cfg.dragMode == DragMode::Bend || cfg.dragMode == DragMode::Vibrato ) return; // the finger stays on its key
            if( note < 0 || note > 127 || note == m_note[slot] ) return; // between keys / off the keys: keep holding
            const int old = m_note[slot];
            const int vel = velocityFor( slot, yFrac );
            if( cfg.dragMode == DragMode::Legato )
            {
                attach( slot, note, vel, out ); // new note first ...
                if( old >= 0 ) detach( old, slot, out ); // ... then the old one ends
            }
            else
            {
                if( old >= 0 ) { m_note[slot] = -1; detach( old, slot, out ); }
                attach( slot, note, vel, out );
            }
        }

        void up( int slot, Events& out )
        {
            if( slot < 0 || slot >= kSlots ) return;
            const int n = m_note[slot];
            m_note[slot] = -1;
            m_speed[slot].reset();
            if( n >= 0 ) detach( n, slot, out );
        }

        // Sustain pedal / button. Releasing it ends every note that is only kept alive by the pedal.
        void sustain( bool on, Events& out )
        {
            if( m_sustain == on ) return;
            m_sustain = on;
            if( on ) return;
            for( int n = 0; n < 128; ++n )
                if( m_sustained[n] )
                {
                    m_sustained[n] = false;
                    out.push_back( { Event::NoteOff, n, 0, -1 } );
                }
        }

        // All fingers up, pedal off: every sounding note ends (screen closed, track change, ...).
        void releaseAll( Events& out )
        {
            for( int s = 0; s < kSlots; ++s )
            {
                const int n = m_note[s];
                m_note[s]   = -1;
                m_speed[s].reset();
                if( n >= 0 && m_count[n] > 0 )
                {
                    m_count[n] = 0;
                    out.push_back( { Event::NoteOff, n, 0, s } );
                }
            }
            m_sustain = false;
            for( int n = 0; n < 128; ++n )
            {
                if( m_sustained[n] ) out.push_back( { Event::NoteOff, n, 0, -1 } );
                m_sustained[n] = false;
                m_count[n]     = 0;
            }
        }

        int  noteOfSlot( int slot ) const { return slot >= 0 && slot < kSlots ? m_note[slot] : -1; }
        int  fingersOn( int note ) const { return note >= 0 && note < 128 ? m_count[note] : 0; }
        bool isSustained( int note ) const { return note >= 0 && note < 128 && m_sustained[note]; }
        bool isSounding( int note ) const { return fingersOn( note ) > 0 || isSustained( note ); }
        bool sustainOn() const { return m_sustain; }

    private:
        int velocityFor( int slot, float yFrac ) const
        {
            switch( cfg.velocityMode )
            {
                case VelocityMode::Fixed: return clampVelocity( cfg.fixedVelocity );
                case VelocityMode::Position: return velocityFromPosition( yFrac, cfg.minVelocity );
                case VelocityMode::Speed: return velocityFromSpeed( m_speed[slot].speed(), cfg );
            }
            return clampVelocity( cfg.fixedVelocity );
        }
        void attach( int slot, int note, int vel, Events& out )
        {
            m_note[slot] = note;
            if( ++m_count[note] == 1 )
            {
                if( m_sustained[note] ) // re-struck while the pedal holds it: end the old voice, start a new one
                {
                    m_sustained[note] = false;
                    out.push_back( { Event::NoteOff, note, 0, slot } );
                }
                out.push_back( { Event::NoteOn, note, vel, slot } );
            }
        }
        void detach( int note, int slot, Events& out )
        {
            if( m_count[note] > 0 && --m_count[note] == 0 )
            {
                if( m_sustain ) m_sustained[note] = true;
                else out.push_back( { Event::NoteOff, note, 0, slot } );
            }
        }

        int          m_note[kSlots]  = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
        SpeedTracker m_speed[kSlots];
        int          m_count[128]     = {};
        bool         m_sustained[128] = {};
        bool         m_sustain        = false;
    };

    // ---- scale / chord marking (shared source: the Pad Perform root / scale / chord) ----------------------------

    enum class KeyMark { None, InScale, Root, Chord };

    // formula = the step list of mt::ScaleAndNotes::scaleFormulaMap (semitone steps from the root, repeating).
    // Returns the 12 pitch classes (0 = C) that belong to the scale; rootPc 0..11.
    inline std::vector<bool> scalePitchClasses( int rootPc, const std::vector<int>& formula )
    {
        std::vector<bool> pc( 12, false );
        if( formula.empty() ) return pc;
        int n = ( ( rootPc % 12 ) + 12 ) % 12, i = 0, guard = 0;
        do
        {
            pc[(size_t)n] = true;
            n = ( n + std::max( 1, formula[(size_t)i] ) ) % 12;
            i = ( i + 1 ) % (int)formula.size();
        } while( n != ( ( rootPc % 12 ) + 12 ) % 12 && ++guard < 64 );
        return pc;
    }

    // Mark of a key. chordRootKey < 0 = no chord shown; chordSteps = semitone offsets above chordRootKey (the formula
    // of mt::ScaleAndNotes::chordFormulaMap, the root itself implied).
    inline KeyMark markFor( int key, int rootPc, const std::vector<bool>& inScale, int chordRootKey, const std::vector<int>& chordSteps )
    {
        if( chordRootKey >= 0 )
        {
            if( key == chordRootKey ) return KeyMark::Chord;
            for( int s : chordSteps )
                if( key == chordRootKey + s ) return KeyMark::Chord;
        }
        if( key % 12 == ( ( rootPc % 12 ) + 12 ) % 12 && ! inScale.empty() ) return KeyMark::Root;
        if( ! inScale.empty() && inScale[(size_t)( key % 12 )] ) return KeyMark::InScale;
        return KeyMark::None;
    }

    // ---- display of notes that sound from elsewhere (physical keys, sequence) -------------------------------------

    // The engine reports SOUNDING pitches; the screen keys are labelled like the physical keys (before the shared
    // transposition), so a sounding pitch is drawn on the key you would press to get it. -1 = out of the 0..127 range.
    inline int keyForSoundingPitch( int sounding, int octave, int semitone )
    {
        const int k = sounding - octave * 12 - semitone;
        return k < 0 || k > 127 ? -1 : k;
    }
} // namespace ui::piano
