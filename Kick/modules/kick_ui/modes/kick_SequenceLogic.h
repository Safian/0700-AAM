/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Pure logic of the Next Sequence and Song screens: the Q-Link (sequence) mapping and the
 * Next Sequence play / next / hold state transitions. No JUCE, no engine: header only, covered by
 * Kick/tests/platform/SequenceLogicTests.cpp.
 */
#pragma once

#include <algorithm>

namespace kick_ui
{
    namespace sequence_logic
    {
        // ---- Q-Link 1..4 on the selected sequence (the Key 61 only has four encoders) -------------------
        // 1 = BPM, 2 = Length (bars), 3 = Loop start, 4 = Loop end. A missing sequence gives no edit.
        struct SeqValues
        {
            bool valid{ false };
            int  bpm{ 120 };
            int  length{ 4 };
            int  loopStart{ 0 };
            int  loopEnd{ 0 };
        };

        struct QLinkEdit
        {
            enum class Kind
            {
                None,
                Bpm,
                Length,
                LoopPoints
            };
            Kind kind{ Kind::None };
            int  a{ 0 }; // BPM / bars / loop start
            int  b{ 0 }; // loop end
        };

        constexpr int kBpmMin    = 20;
        constexpr int kBpmMax    = 240;
        constexpr int kLengthMin = 1;
        constexpr int kLengthMax = 100;

        inline int clampTo( int V, int Lo, int Hi ) { return std::min( Hi, std::max( Lo, V ) ); }

        // Knob is 1-based (EN_KNOB_1 = 1). Knobs above 4 do not exist on the Key 61 and are not handled here.
        inline QLinkEdit sequenceQLink( int Knob, const SeqValues& Seq, int Delta )
        {
            QLinkEdit e;
            if( !Seq.valid || Delta == 0 )
                return e;
            switch( Knob )
            {
                case 1: e = { QLinkEdit::Kind::Bpm, clampTo( Seq.bpm + Delta, kBpmMin, kBpmMax ), 0 }; break;
                case 2: e = { QLinkEdit::Kind::Length, clampTo( Seq.length + Delta, kLengthMin, kLengthMax ), 0 }; break;
                case 3: e = { QLinkEdit::Kind::LoopPoints, std::max( 0, Seq.loopStart + Delta ), Seq.loopEnd }; break;
                case 4: e = { QLinkEdit::Kind::LoopPoints, Seq.loopStart, std::max( 0, Seq.loopEnd + Delta ) }; break;
                default: break;
            }
            return e;
        }

        // ---- Next Sequence state ---------------------------------------------------------------------------
        // playing  = the sequence shown as playing / selected, next = the queued one (-1 none),
        // isPlaying = the engine transport state this screen believes in.
        struct NextState
        {
            int  playing{ -1 };
            int  next{ -1 };
            bool isPlaying{ false };
        };

        // Pad / row pressed while playing: queue 'Index' as the next sequence. Pressing the playing
        // sequence only drops a queued one; pressing the queued one again keeps it.
        inline NextState queueNext( NextState S, int Index )
        {
            if( S.next >= 0 && S.next != Index )
                S.next = -1;
            if( Index != S.playing && S.next != Index )
                S.next = Index;
            return S;
        }

        // The screen (re)appears or a bank changed: bring the screen state in line with the engine.
        // A queued sequence only exists while playing; if the engine stopped meanwhile (STOP pressed on
        // another screen) the queue is dropped (the engine's own queue too) instead of crashing.
        struct RestoreResult
        {
            NextState state;
            bool      clearEngineNext{ false };
        };

        inline RestoreResult restore( NextState S, bool EnginePlaying, int EngineSelected )
        {
            RestoreResult r;
            if( S.playing < 0 )
                S.playing = EngineSelected;
            S.isPlaying = EnginePlaying;
            if( !S.isPlaying && S.next >= 0 )
            {
                S.next                = -1;
                r.clearEngineNext     = true;
            }
            r.state = S;
            return r;
        }

        // Clear button: drop the queued sequence; with no playing sequence known the engine's selection stands in.
        inline NextState clearNext( NextState S, int EngineSelected )
        {
            if( S.playing < 0 )
                S.playing = EngineSelected;
            S.next = -1;
            return S;
        }

        // Hold pressed on: a queued sequence is dropped (Akai semantics NOT verified, behaviour kept).
        inline NextState holdOn( NextState S )
        {
            if( S.isPlaying )
                S.next = -1;
            return S;
        }

        // Screen hidden while stopped: the queued sequence becomes the selected one.
        inline NextState hiddenWhileStopped( NextState S )
        {
            if( !S.isPlaying && S.next >= 0 )
            {
                S.playing = S.next;
                S.next    = -1;
            }
            return S;
        }
    }
}
