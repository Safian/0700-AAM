// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <atomic>
#include <algorithm>
#include <functional>

namespace ui
{
    // Meeting point of the on-screen piano (kick_ui PianoView) and the hardware bridge (apps/kick), which live in
    // different layers. It makes the two keyboards one instrument:
    //  - the transposition (octave / semitone) of the Key 61 keybed (the bridge's KeyTranspose) is mirrored here, and the
    //    screen's octave buttons step THAT transposition (stepTranspose), so OCT-/OCT+ and the screen are the same switch;
    //  - the on-screen Sustain button goes through the bridge's sustain state machine as its own source (setSustain),
    //    next to the pedal and the touch strip.
    // Without a bridge (desktop build, iOS) the handlers are empty and the hub keeps the transposition itself and the
    // screen sends CC64 directly.
    class PianoHub
    {
    public:
        static constexpr int kMaxOctave   = 3;  // same limits as KeyTranspose
        static constexpr int kMaxSemitone = 12;

        static PianoHub& instance()
        {
            static PianoHub h;
            return h;
        }

        // ---- set by the bridge (hardware builds); null = no bridge ----
        std::function<void( bool up, bool shift )> stepTranspose;
        std::function<void()>                      resetTranspose;
        std::function<void( bool down )>           setSustain;

        // ---- transposition mirror (bridge -> screen) ----
        int  octave() const { return m_octave.load( std::memory_order_relaxed ); }
        int  semitone() const { return m_semitone.load( std::memory_order_relaxed ); }
        void publishTranspose( int octave, int semitone )
        {
            m_octave.store( octave, std::memory_order_relaxed );
            m_semitone.store( semitone, std::memory_order_relaxed );
            m_version.fetch_add( 1, std::memory_order_relaxed );
        }
        int version() const { return m_version.load( std::memory_order_relaxed ); }

        // ---- screen -> shared transposition ----
        void requestStep( bool up, bool shift )
        {
            if( stepTranspose ) { stepTranspose( up, shift ); return; }
            // desktop: keep it here, same clamps as KeyTranspose::step
            const int d = up ? 1 : -1;
            if( shift )
            {
                const int v = semitone() + d;
                if( v >= -kMaxSemitone && v <= kMaxSemitone ) publishTranspose( octave(), v );
            }
            else
            {
                const int v = octave() + d;
                if( v >= -kMaxOctave && v <= kMaxOctave ) publishTranspose( v, semitone() );
            }
        }
        void requestReset()
        {
            if( resetTranspose ) { resetTranspose(); return; }
            publishTranspose( 0, 0 );
        }
        bool sustainViaBridge() const { return static_cast<bool>( setSustain ); }

    private:
        std::atomic<int> m_octave{ 0 };
        std::atomic<int> m_semitone{ 0 };
        std::atomic<int> m_version{ 0 };
    };
}
