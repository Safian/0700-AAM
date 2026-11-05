// SPDX-License-Identifier: AGPL-3.0-or-later
// JUCE-free logic for SEQUENCE RECORDING (hardware REC / OVERDUB + PLAY): the arm/record state machine, the live
// event capture buffer and the note pairing (event tick -> note start/length). The engine glue (arming tracks,
// clearing, committing into the note lists, undo) is in mt_audio_manager.cpp / mt_audio_track.cpp.
//
// Source of the behaviour (MPC v3.10 manual, Key 61 controls 9-10, "Recording a Drum Sequence", "Rec Arm Behavior"):
// REC arms the recording, PLAY / PLAY START starts it, a plain REC take erases the existing events of the armed
// tracks, OVERDUB keeps them. Everything the manual does not say is a documented dummy, see
// docs/hardware/key61/morning-decisions.md ("Szekvenciafelvetel").
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace mt::seqrec
{
    // Mirrors mt::RecordState (NotArmed, Armed, Recording) without the JUCE header.
    enum class Phase : uint8_t
    {
        Off       = 0,
        Armed     = 1,
        Recording = 2
    };

    struct Control
    {
        Phase phase{ Phase::Off };
        bool  overdub{ false }; // OVERDUB mode: keep the existing events (a plain REC take erases them)
    };

    // Hardware REC. Off -> Armed; Armed -> Off (disarm); Recording -> Off (punch out, playback continues).
    // A take is a one-shot: leaving Recording resets the overdub mode (so the LEDs always tell the truth).
    inline Control onRecPressed( Control c )
    {
        if( Phase::Off == c.phase )
            c.phase = Phase::Armed;
        else
            c = Control{};
        return c;
    }

    // Hardware OVERDUB. Toggles the overdub mode; turning it on while Off also arms (dummy: the manual does not say
    // whether OVERDUB alone arms). Turning it off while Armed/Recording keeps the phase (a plain REC take).
    inline Control onOverdubPressed( Control c )
    {
        c.overdub = !c.overdub;
        if( c.overdub && Phase::Off == c.phase )
            c.phase = Phase::Armed;
        return c;
    }

    // PLAY / PLAY START while Armed starts the take. clearExisting: a plain REC take erases the armed tracks' events.
    inline Control onPlayStarted( Control c, bool& clearExisting )
    {
        clearExisting = false;
        if( Phase::Armed == c.phase )
        {
            clearExisting = !c.overdub;
            c.phase       = Phase::Recording;
        }
        return c;
    }

    // The sequence play of the Looper (LoopRecorder::record in sync mode) also goes through setSequencePlay(Now).
    // While the Looper records/overdubs that play is NOT a sequence-recording PLAY: an Armed sequence must stay
    // Armed and must not erase the armed tracks' notes. looperBusy = looper isRecording() || isOverdubbing().
    inline Control onPlayStartedUnlessLooper( Control c, bool looperBusy, bool& clearExisting )
    {
        clearExisting = false;
        if( looperBusy )
            return c;
        return onPlayStarted( c, clearExisting );
    }

    // STOP / pause / end of the sequence while recording: the take is finished.
    inline Control onTransportStopped( Control c )
    {
        if( Phase::Recording == c.phase )
            c = Control{};
        return c;
    }

    // One captured live note event. tick = the track-local playhead tick (0 .. track length - 1) when it arrived.
    struct Event
    {
        int32_t tick{ 0 };
        uint8_t note{ 0 };
        uint8_t velocity{ 0 };
        uint8_t channel{ 0 };
        bool    on{ false };
    };

    struct Note
    {
        int note{ 0 };
        int velocity{ 0 };
        int channel{ 0 };
        int startTick{ 0 };
        int lengthTicks{ 1 };
    };

    // Length of a note from onTick to offTick on a looping track of loopLen ticks (0 = unknown, no wrap): the note may
    // be released after the loop wrapped (offTick < onTick). Same tick = 1 tick; never longer than one loop.
    inline int spanTicks( int onTick, int offTick, int loopLen )
    {
        int d = offTick - onTick;
        if( d < 0 && loopLen > 0 )
            d += loopLen;
        if( d < 1 )
            d = 1;
        if( loopLen > 0 && d > loopLen )
            d = loopLen;
        return d;
    }

    constexpr size_t kCapacity = 1024; // events held per track between two drains (25 Hz drain: far more than needed)

    // Fixed-capacity capture buffer. push() runs on the MIDI/UI input thread: it never allocates and only holds a tiny
    // spin flag for an O(1) store. drain() runs on the message thread (25 Hz poll / STOP): it pairs note-on with note-off
    // (per channel + note) and removes the consumed events; a note still held stays in the buffer for the next drain
    // (closeOpen = false) or is closed at closeTick (closeOpen = true, end of the take). The pairing runs under the same
    // spin flag so a note-off can never arrive between "taken out" and "put back" (the SustainState data race lesson);
    // it is O(kCapacity) on the stack with no allocation (out.reserve() is done before the flag is taken).
    class Capture
    {
    public:
        // false = buffer full (event dropped). Note-on with velocity 0 counts as note-off.
        bool push( const Event& e ) noexcept
        {
            Guard g( flag );
            if( count >= kCapacity )
                return false;
            buf[count++] = e;
            return true;
        }

        void clear() noexcept
        {
            Guard g( flag );
            count = 0;
        }

        size_t size() const noexcept
        {
            Guard g( flag );
            return count;
        }

        // Appends the completed notes to out (sorted by start tick, then note). Returns how many.
        int drain( std::vector<Note>& out, int loopLen, int closeTick, bool closeOpen )
        {
            out.reserve( out.size() + kCapacity / 2 + 1 );
            const size_t first = out.size();
            Guard        g( flag );
            int16_t      held[16 * 128];
            for( auto& h : held )
                h = -1;
            bool consumed[kCapacity];
            for( size_t i = 0; i < count; ++i )
                consumed[i] = false;

            auto emit = [&]( size_t onIdx, int endTick ) {
                const Event& on = buf[onIdx];
                Note         n;
                n.note        = on.note;
                n.velocity    = on.velocity;
                n.channel     = on.channel;
                n.startTick   = loopLen > 0 ? ( on.tick < 0 ? 0 : ( on.tick >= loopLen ? on.tick % loopLen : on.tick ) ) : ( on.tick < 0 ? 0 : on.tick );
                n.lengthTicks = spanTicks( on.tick, endTick, loopLen );
                out.push_back( n ); // capacity reserved above: no allocation under the flag
            };

            for( size_t i = 0; i < count; ++i )
            {
                const Event& e = buf[i];
                if( e.note > 127 || e.channel > 15 )
                {
                    consumed[i] = true;
                    continue;
                }
                const size_t slot = size_t( e.channel ) * 128 + e.note;
                if( e.on && e.velocity > 0 )
                {
                    if( held[slot] >= 0 ) // re-strike while held: close the earlier note at this tick
                    {
                        emit( (size_t)held[slot], e.tick );
                        consumed[(size_t)held[slot]] = true;
                    }
                    held[slot] = (int16_t)i;
                }
                else
                {
                    if( held[slot] >= 0 )
                    {
                        emit( (size_t)held[slot], e.tick );
                        consumed[(size_t)held[slot]] = true;
                        held[slot]                    = -1;
                    }
                    consumed[i] = true; // the off itself (an off without an on is an orphan: dropped)
                }
            }
            if( closeOpen )
            {
                for( auto h : held )
                    if( h >= 0 )
                    {
                        emit( (size_t)h, closeTick );
                        consumed[(size_t)h] = true;
                    }
            }
            size_t kept = 0; // keep what is not consumed (held notes), in order
            for( size_t i = 0; i < count; ++i )
                if( !consumed[i] )
                    buf[kept++] = buf[i];
            count = kept;

            for( size_t a = first + 1; a < out.size(); ++a ) // insertion sort: tiny n, no allocation
            {
                Note   key = out[a];
                size_t b   = a;
                while( b > first && ( out[b - 1].startTick > key.startTick || ( out[b - 1].startTick == key.startTick && out[b - 1].note > key.note ) ) )
                {
                    out[b] = out[b - 1];
                    --b;
                }
                out[b] = key;
            }
            return int( out.size() - first );
        }

    private:
        struct Guard
        {
            explicit Guard( const std::atomic_flag& f ) : fl( const_cast<std::atomic_flag&>( f ) ) { while( fl.test_and_set( std::memory_order_acquire ) ) {} }
            ~Guard() { fl.clear( std::memory_order_release ); }
            std::atomic_flag& fl;
        };
        mutable std::atomic_flag flag = ATOMIC_FLAG_INIT;
        Event                    buf[kCapacity];
        size_t                   count{ 0 };
    };
} // namespace mt::seqrec
