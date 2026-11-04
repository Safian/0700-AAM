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
        // hold / held: the Hold button (manual: "repeat the current sequence indefinitely and temporarily ignore pad
        // presses ... select other pads without selecting them to play next"). While Hold is on a pad press only
        // SELECTS ('held'); nothing is queued. Next Bar / Sudden switch to the selection (held, else the queued one).
        struct NextState
        {
            int  playing{ -1 };
            int  next{ -1 };
            bool isPlaying{ false };
            bool hold{ false };
            int  held{ -1 };
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

        // Hold switched on / off. On: the engine's queue is cancelled (the current sequence repeats) and the pad
        // selection starts empty. Off: back to normal operation; a selection made during Hold is not queued.
        inline NextState setHold( NextState S, bool On )
        {
            S.hold = On;
            S.held = -1;
            if( On )
                S.next = -1;
            return S;
        }

        // A pad / row pressed while playing: with Hold it selects, without Hold it queues.
        inline NextState pressPad( NextState S, int Index )
        {
            if( !S.isPlaying )
                return S;
            if( !S.hold )
                return queueNext( S, Index );
            S.held = ( Index == S.playing ) ? -1 : Index;
            return S;
        }

        // The sequence Next Bar / Sudden switch to: the selection while Hold is on, else the queued one (-1 none).
        inline int switchTarget( const NextState& S ) { return S.hold ? S.held : S.next; }

        // After a Sudden switch: the target plays, selection and queue are empty (Hold stays as it was).
        inline NextState afterSudden( NextState S )
        {
            const int t = switchTarget( S );
            if( t >= 0 )
                S.playing = t;
            S.next = -1;
            S.held = -1;
            return S;
        }

        // Clear = delete the selected sequence from the playlist (manual): only a sequence that is not playing.
        // While playing the selected sequence is the queued one (or the held selection); stopped it is 'playing'
        // (the selected one). The last remaining sequence is kept. Returns the index to delete, -1 none.
        inline int clearTarget( const NextState& S, int NumSequences )
        {
            if( NumSequences <= 1 )
                return -1;
            const int t = S.isPlaying ? switchTarget( S ) : S.playing;
            if( t < 0 || t >= NumSequences )
                return -1;
            if( S.isPlaying && t == S.playing )
                return -1;
            return t;
        }

        // Indices after sequence 'Deleted' was removed from a list that had 'NumBefore' entries.
        inline int indexAfterDelete( int Index, int Deleted )
        {
            if( Index < 0 || Index == Deleted )
                return -1;
            return Index > Deleted ? Index - 1 : Index;
        }

        inline NextState afterDelete( NextState S, int Deleted, int NumBefore )
        {
            S.next = indexAfterDelete( S.next, Deleted );
            S.held = indexAfterDelete( S.held, Deleted );
            const int p = indexAfterDelete( S.playing, Deleted );
            S.playing   = p >= 0 ? p : std::max( 0, std::min( Deleted, NumBefore - 2 ) ); // the neighbour takes over the selection
            return S;
        }

        // Copy to Song (manual: "copy the sequence playlist to a song"): one step per used sequence, repeat 1.
        inline int copyToSongStepCount( int NumSequences ) { return std::max( 0, NumSequences ); }

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
