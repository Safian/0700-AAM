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
// Thread-safe: noteProcess() of one program is reached from several threads (tick thread for sequencer/arp output,
// message thread for USB MIDI, ALSA reader/message thread for the hardware bridge), with no common lock. Every method
// takes a short internal spin lock (no heap, no blocking call inside). The Release callback of onCC64 is called AFTER
// the lock is dropped (pitches are collected in a fixed bitmask under the lock), so noteOff never runs under it.
#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace mt
{
    class SustainState
    {
    public:
        // Note-on: the note sounds again, so no pending pedal release for it.
        void onNoteOn( int Note ) { Lock l( *this ); if( valid( Note ) ) held[(size_t)Note] = false; }

        // Note-off of a NOTE_ON-type sample. Returns true when the voice must be released now;
        // false when the pedal defers it (released later by onCC64). ONE_SHOT samples never
        // release on note-off, so the caller must not use this for them (exempt from the pedal).
        bool onNoteOff( int Note )
        {
            Lock l( *this );
            if( sustainDown && valid( Note ) )
            {
                held[(size_t)Note] = true;
                return false;
            }
            return true;
        }

        // CC64. Calls Release( note ) for every held note when the pedal goes up (after the lock is dropped).
        template<typename F>
        void onCC64( int Value, F&& Release )
        {
            std::array<bool, 128> rel{};
            bool any = false;
            {
                Lock l( *this );
                const bool down = Value >= 64;
                if( sustainDown && !down )
                    for( size_t n = 0; n < held.size(); ++n )
                        if( held[n] )
                        {
                            held[n] = false;
                            // A sequencer note currently sounds on this pitch: it ends with its own note-off, not with the pedal.
                            if( seqActive[n] == 0 )
                            {
                                rel[n] = true;
                                any    = true;
                            }
                        }
                sustainDown = down;
            }
            if( any )
                for( size_t n = 0; n < rel.size(); ++n )
                    if( rel[n] )
                        Release( (int)n );
        }

        // CC123 / all notes off: the caller releases every voice, forget the pedal completely.
        void onAllOff()
        {
            Lock l( *this );
            held.fill( false );
            seqActive.fill( 0 );
            sustainDown = false;
        }

        // Sequencer (non-live) NOTE_ON-type notes, counted per pitch so the pedal can tell them from live ones.
        void onSeqNoteOn( int Note ) { Lock l( *this ); if( valid( Note ) && seqActive[(size_t)Note] < 255 ) ++seqActive[(size_t)Note]; }
        void onSeqNoteOff( int Note ) { Lock l( *this ); if( valid( Note ) && seqActive[(size_t)Note] > 0 ) --seqActive[(size_t)Note]; }
        // Sequencer note-off and the "release now?" decision in ONE critical section (see releaseNowOnNoteOff).
        bool onSeqNoteOffReleaseNow( int Note )
        {
            Lock l( *this );
            if( valid( Note ) && seqActive[(size_t)Note] > 0 )
                --seqActive[(size_t)Note];
            return !( sustainDown && valid( Note ) && held[(size_t)Note] );
        }
        // Transport stop / reset: no sequencer note is left sounding (a lost note-off must not block a later pedal release).
        void clearSeq() { Lock l( *this ); seqActive.fill( 0 ); }
        int  seqCount( int Note ) const { Lock l( *this ); return valid( Note ) ? seqActive[(size_t)Note] : 0; }

        bool isDown() const { Lock l( *this ); return sustainDown; }
        bool isHeld( int Note ) const { Lock l( *this ); return valid( Note ) && held[(size_t)Note]; }

    private:
        struct Lock
        {
            explicit Lock( const SustainState& s ) : f( s.flag ) { while( f.test_and_set( std::memory_order_acquire ) ) {} }
            ~Lock() { f.clear( std::memory_order_release ); }
            Lock( const Lock& ) = delete;
            Lock& operator=( const Lock& ) = delete;
            std::atomic_flag& f;
        };
        static bool valid( int N ) { return N >= 0 && N < 128; }
        mutable std::atomic_flag flag = ATOMIC_FLAG_INIT;
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
        return s.onSeqNoteOffReleaseNow( n ); // atomic: count-down and pedal check in one critical section
    }

} // namespace mt
