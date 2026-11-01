// SPDX-License-Identifier: AGPL-3.0-or-later
// JUCE-free logic for RECALL (retrospective MIDI capture): a fixed-capacity ring of live note events and the pure
// functions that turn a snapshot of it into notes (tick start + length) for the track's note list.
#pragma once
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace mt::live
{
    // One raw live event. tick is the track-local playhead tick at capture time, or -1 when the transport was stopped.
    struct Event
    {
        int64_t timeMs{ 0 };
        int32_t tick{ -1 };
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

    constexpr size_t  kCapacity = 2048;  // events kept per track
    constexpr int64_t kMaxAgeMs = 30000; // events older than this are never used

    // Fixed-capacity overwrite-oldest ring. push() never allocates and only holds a tiny spin flag for an O(1) copy, so
    // it is safe to call from the MIDI/UI input thread; snapshot() (allocating) is for the non-realtime recall caller.
    // Per track (not global): processIncomingMidiMessage is per track and each track has its own timeline/length.
    class Ring
    {
    public:
        Ring() { buf.resize( kCapacity ); } // preallocated once, at track construction

        void push( const Event& e ) noexcept
        {
            Guard g( flag );
            buf[( head + count ) % kCapacity] = e;
            if( count < kCapacity ) ++count;
            else head = ( head + 1 ) % kCapacity;
        }

        std::vector<Event> snapshot() const
        {
            Guard              g( flag );
            std::vector<Event> out;
            out.reserve( count );
            for( size_t i = 0; i < count; ++i ) out.push_back( buf[( head + i ) % kCapacity] );
            return out;
        }

        void clear() noexcept
        {
            Guard g( flag );
            head = count = 0;
        }

    private:
        struct Guard
        {
            explicit Guard( std::atomic_flag& f ) : fl( f ) { while( fl.test_and_set( std::memory_order_acquire ) ) {} }
            ~Guard() { fl.clear( std::memory_order_release ); }
            std::atomic_flag& fl;
        };
        mutable std::atomic_flag flag = ATOMIC_FLAG_INIT;
        std::vector<Event>       buf;
        size_t                   head{ 0 };
        size_t                   count{ 0 };
    };

    // Pair note-on/off (per channel+note), drop orphans (off without on; on never released; on re-struck while held),
    // trim by age (windowMs, capped at kMaxAgeMs) and convert to ticks.
    //  useTicks=true  (playing): start = tick stored at note-on (events with tick<0 are skipped); start wraps into
    //                            [0,loopLen) when loopLen>0.
    //  useTicks=false (stopped) : start = (onMs - earliest kept onMs) * ticksPerMs, i.e. aligned to the sequence start.
    // length = (offMs-onMs)*ticksPerMs, min 1, capped at loopLen when loopLen>0. Result is sorted by start, then note.
    inline std::vector<Note> pairEvents( const std::vector<Event>& ev, int64_t nowMs, int64_t windowMs, bool useTicks,
                                         double ticksPerMs, int loopLenTicks )
    {
        struct Open
        {
            bool  open{ false };
            Event on;
        };
        std::vector<Note>    notes;
        std::vector<int64_t> onMs;
        std::vector<Open>    held( 16 * 128 );
        const int64_t        oldest = nowMs - std::min<int64_t>( windowMs, kMaxAgeMs );

        for( const auto& e : ev )
        {
            if( e.timeMs < oldest || e.note > 127 || e.channel > 15 ) continue;
            auto& h = held[size_t( e.channel ) * 128 + e.note];
            if( e.on && e.velocity > 0 )
            {
                h.open = !( useTicks && e.tick < 0 ); // a re-strike while held replaces (drops) the earlier unterminated on
                h.on   = e;
            }
            else if( h.open )
            {
                h.open = false;
                Note n;
                n.note        = e.note;
                n.velocity    = h.on.velocity;
                n.channel     = e.channel;
                n.lengthTicks = std::max( 1, int( double( e.timeMs - h.on.timeMs ) * ticksPerMs + 0.5 ) );
                if( loopLenTicks > 0 ) n.lengthTicks = std::min( n.lengthTicks, loopLenTicks );
                n.startTick = useTicks ? h.on.tick : 0;
                notes.push_back( n );
                onMs.push_back( h.on.timeMs );
            }
        }
        if( notes.empty() ) return notes;

        if( !useTicks )
        {
            const int64_t first = *std::min_element( onMs.begin(), onMs.end() );
            for( size_t i = 0; i < notes.size(); ++i )
                notes[i].startTick = int( double( onMs[i] - first ) * ticksPerMs + 0.5 );
        }
        if( loopLenTicks > 0 )
            for( auto& n : notes ) n.startTick %= loopLenTicks;
        std::sort( notes.begin(), notes.end(), []( const Note& a, const Note& b ) {
            return a.startTick != b.startTick ? a.startTick < b.startTick : a.note < b.note;
        } );
        return notes;
    }
} // namespace mt::live
