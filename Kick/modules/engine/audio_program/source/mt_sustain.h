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
                        Release( (int)n );
                    }
            sustainDown = down;
        }

        // CC123 / all notes off: the caller releases every voice, forget the pedal completely.
        void onAllOff()
        {
            held.fill( false );
            sustainDown = false;
        }

        bool isDown() const { return sustainDown; }
        bool isHeld( int Note ) const { return valid( Note ) && held[(size_t)Note]; }

    private:
        static bool valid( int N ) { return N >= 0 && N < 128; }
        bool                  sustainDown{ false };
        std::array<bool, 128> held{};
    };
} // namespace mt
