/*
 * Copyright (c) 2024 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
// Sustain pedal (CC64) state machine of one program. JUCE-free, header only (host-testable).
// Not thread-safe: call from the program's note-processing thread only.
#pragma once

#include <array>
#include <cstddef>

namespace mt
{
    class SustainState
    {
    public:
        // Note-on: the note sounds again, so no pending pedal release for it.
        void onNoteOn( int Note ) { if( valid( Note ) ) held[(size_t)Note] = false; }

        // Note-off of a NOTE_ON-type sample. Returns true when the voice must be released now;
        // false when the pedal defers it (released later by onCC64). ONE_SHOT samples never
        // release on note-off, so the caller must not use this for them (exempt from the pedal).
        bool onNoteOff( int Note )
        {
            if( sustainDown && valid( Note ) )
            {
                held[(size_t)Note] = true;
                return false;
            }
            return true;
        }

        // CC64. Calls Release( note ) for every held note when the pedal goes up.
        template<typename F>
        void onCC64( int Value, F&& Release )
        {
            const bool down = Value >= 64;
            if( sustainDown && !down )
                for( size_t n = 0; n < held.size(); ++n )
                    if( held[n] )
                    {
                        held[n] = false;
                        // A sequencer note currently sounds on this pitch: it ends with its own note-off, not with the pedal.
                        if( seqActive[n] == 0 )
                            Release( (int)n );
                    }
            sustainDown = down;
        }

        // CC123 / all notes off: the caller releases every voice, forget the pedal completely.
        void onAllOff()
        {
            held.fill( false );
            seqActive.fill( 0 );
            sustainDown = false;
        }

        // Sequencer (non-live) NOTE_ON-type notes, counted per pitch so the pedal can tell them from live ones.
        void onSeqNoteOn( int Note ) { if( valid( Note ) && seqActive[(size_t)Note] < 255 ) ++seqActive[(size_t)Note]; }
        void onSeqNoteOff( int Note ) { if( valid( Note ) && seqActive[(size_t)Note] > 0 ) --seqActive[(size_t)Note]; }
        // Transport stop / reset: no sequencer note is left sounding (a lost note-off must not block a later pedal release).
        void clearSeq() { seqActive.fill( 0 ); }
        int  seqCount( int Note ) const { return valid( Note ) ? seqActive[(size_t)Note] : 0; }

        bool isDown() const { return sustainDown; }
        bool isHeld( int Note ) const { return valid( Note ) && held[(size_t)Note]; }

    private:
        static bool valid( int N ) { return N >= 0 && N < 128; }
        bool                  sustainDown{ false };
        std::array<bool, 128> held{};
        std::array<unsigned char, 128> seqActive{};
    };
    // Pedal holds live notes only. A sequencer note-off releases at once, except on a pitch whose live note the pedal
    // holds (same pitch = same voice): that one keeps sounding until the pedal goes up. Returns true when to release now.
    inline bool releaseNowOnNoteOff( SustainState& s, bool live, int n )
    {
        if( live )
            return s.onNoteOff( n );
        s.onSeqNoteOff( n );
        return !( s.isDown() && s.isHeld( n ) );
    }

} // namespace mt
