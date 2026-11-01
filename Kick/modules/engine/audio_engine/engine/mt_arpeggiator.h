// SPDX-License-Identifier: AGPL-3.0-or-later
// JUCE-free arpeggiator logic (held-note set, ordering per mode, octave expansion, latch, tick-to-step timing).
// Not thread-safe by itself: the owner (MTTrack) serialises noteOn/noteOff/config (input thread) and tick (tick thread)
// with a short spin guard. Fixed-size storage only: no allocation, no blocking, O(held*octaves) work per change and
// O(1) per tick.
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>

namespace mt::arp
{
    enum Mode : int { Up = 0, Down = 1, UpDown = 2, Random = 3, NumModes };

    // Note values, tied to the transport's PPQN (ticks per quarter note). Triplets: 3 notes in the time of 2.
    enum Rate : int { R1_4 = 0, R1_8, R1_16, R1_32, R1_4T, R1_8T, R1_16T, R1_32T, NumRates };

    constexpr int kMaxHeld = 64;
    constexpr int kMaxOct  = 4;
    constexpr int kMaxSeq  = kMaxHeld * kMaxOct;

    inline int stepTicks( int rate, int ppqn )
    {
        int t = ppqn;
        switch( rate )
        {
            case R1_4: t = ppqn; break;
            case R1_8: t = ppqn / 2; break;
            case R1_16: t = ppqn / 4; break;
            case R1_32: t = ppqn / 8; break;
            case R1_4T: t = ppqn * 2 / 3; break;
            case R1_8T: t = ppqn / 3; break;
            case R1_16T: t = ppqn / 6; break;
            case R1_32T: t = ppqn / 12; break;
            default: t = ppqn / 4; break;
        }
        return std::max( 1, t );
    }

    // Tiny deterministic generator (xorshift32); the seed is supplied by the owner.
    struct Rng
    {
        uint32_t s{ 0x9E3779B9u };
        explicit Rng( uint32_t seed = 0x9E3779B9u ) : s( seed ? seed : 0x9E3779B9u ) {}
        uint32_t next()
        {
            s ^= s << 13;
            s ^= s >> 17;
            s ^= s << 5;
            return s;
        }
    };

    struct Out
    {
        uint8_t note{ 0 };
        uint8_t velocity{ 0 };
        bool    on{ false };
    };

    // Result buffer of one call. At most one note-off (the previous step) and one note-on per tick.
    struct Result
    {
        std::array<Out, 4> ev{};
        int                n{ 0 };
        void               add( uint8_t note, uint8_t vel, bool on )
        {
            if( n < (int)ev.size() ) ev[(size_t)n++] = Out{ note, vel, on };
        }
    };

    // Short spin guard (same pattern as mt::live::Ring): protects O(1)/O(held*octaves) critical sections shared by the
    // input thread (noteOn/noteOff/config) and the tick thread (tick). Never held across a call into the audio program.
    class SpinGuard
    {
    public:
        explicit SpinGuard( std::atomic_flag& f ) : fl( f ) { while( fl.test_and_set( std::memory_order_acquire ) ) {} }
        ~SpinGuard() { fl.clear( std::memory_order_release ); }
        SpinGuard( const SpinGuard& ) = delete;
        SpinGuard& operator=( const SpinGuard& ) = delete;

    private:
        std::atomic_flag& fl;
    };

    class Arpeggiator
    {
    public:
        explicit Arpeggiator( uint32_t seed = 0x9E3779B9u, int ppqn = 96 ) : rng( seed ), ppqnV( std::max( 1, ppqn ) ) {}

        // ---- configuration (values are clamped) ----
        void setMode( int m ) { mode = std::clamp( m, 0, (int)NumModes - 1 ); }
        void setRate( int r ) { rate = std::clamp( r, 0, (int)NumRates - 1 ); }
        void setOctaves( int o ) { octaves = std::clamp( o, 1, kMaxOct ); rebuild(); }
        void setGatePercent( int g ) { gate = std::clamp( g, 5, 100 ); }
        int  getMode() const { return mode; }
        int  getRate() const { return rate; }
        int  getOctaves() const { return octaves; }
        int  getGatePercent() const { return gate; }
        bool isLatch() const { return latch; }
        bool isEnabled() const { return enabled; }
        int  heldCount() const { return nHeld; }
        int  seqSize() const { return nSeq; }

        // Latch off: latched-but-not-physically-held notes are dropped.
        void setLatch( bool on )
        {
            latch = on;
            if( !on )
            {
                int w = 0;
                for( int i = 0; i < nHeld; ++i )
                    if( down[held[(size_t)i].note] ) held[(size_t)w++] = held[(size_t)i];
                nHeld = w;
                rebuild();
            }
        }

        // Enable/disable. Disabling releases the sounding note (reported in r) and clears all state.
        void setEnabled( bool on, Result& r )
        {
            if( on == enabled ) return;
            enabled = on;
            if( !on ) reset( r );
        }

        // Releases the sounding note and forgets held notes / key state.
        void reset( Result& r )
        {
            if( sounding >= 0 ) r.add( (uint8_t)sounding, soundingVel, false );
            sounding = -1;
            nHeld = nSeq = 0;
            downCount = 0;
            down.fill( false );
            step = 0;
            haveLast = false;
        }

        // Transport stopped/paused/restarted: releases the sounding note and resyncs the clock; held (latched) notes stay.
        void panic( Result& r )
        {
            if( sounding >= 0 ) r.add( (uint8_t)sounding, soundingVel, false );
            sounding = -1;
            haveLast = false;
        }

        // ---- live input ----
        void noteOn( int note, int vel )
        {
            if( note < 0 || note > 127 ) return;
            if( latch && downCount == 0 && nHeld > 0 ) nHeld = 0; // new chord after full release replaces the latched one
            if( !down[(size_t)note] ) { down[(size_t)note] = true; ++downCount; }
            const uint8_t v = (uint8_t)std::clamp( vel, 1, 127 );
            for( int i = 0; i < nHeld; ++i )
                if( held[(size_t)i].note == note ) { held[(size_t)i].vel = v; rebuild(); return; }
            if( nHeld >= kMaxHeld ) return;
            int i = nHeld++;
            while( i > 0 && held[(size_t)i - 1].note > note ) { held[(size_t)i] = held[(size_t)i - 1]; --i; } // keep sorted by pitch
            held[(size_t)i] = { (uint8_t)note, v };
            rebuild();
        }

        // Returns true if the note belonged to the arpeggiator (was physically down), i.e. the caller must not forward it.
        bool noteOff( int note )
        {
            if( note < 0 || note > 127 || !down[(size_t)note] ) return false;
            down[(size_t)note] = false;
            if( downCount > 0 ) --downCount;
            if( !latch )
            {
                int w = 0;
                for( int i = 0; i < nHeld; ++i )
                    if( held[(size_t)i].note != note ) held[(size_t)w++] = held[(size_t)i];
                nHeld = w;
                rebuild();
            }
            return true;
        }

        // ---- clock ----
        // nowTick: monotonically increasing transport tick (same domain every call). Steps start on multiples of the
        // step length, so the pattern is aligned to the grid. A backwards or large forward jump (seek/loop/stop) only
        // resynchronises: the tick is processed alone.
        void tick( int64_t nowTick, Result& r )
        {
            if( !enabled ) return;
            int64_t first = nowTick;
            if( haveLast && nowTick > lastTick && nowTick - lastTick <= 2L * ppqnV ) first = lastTick + 1;
            for( int64_t t = first; t <= nowTick; ++t ) processTick( t, r );
            lastTick = nowTick;
            haveLast = true;
        }

    private:
        struct Held { uint8_t note{ 0 }; uint8_t vel{ 0 }; };

        void rebuild()
        {
            nSeq = 0;
            for( int o = 0; o < octaves; ++o )
                for( int i = 0; i < nHeld; ++i )
                {
                    const int n = held[(size_t)i].note + 12 * o;
                    if( n > 127 ) continue;
                    seq[(size_t)nSeq++] = { (uint8_t)n, held[(size_t)i].vel };
                }
        }

        int pickIndex()
        {
            const int n = nSeq;
            if( n == 1 ) return 0;
            switch( mode )
            {
                case Down: return n - 1 - (int)( step % (uint32_t)n );
                case UpDown:
                {
                    const uint32_t period = (uint32_t)( 2 * n - 2 );
                    const int      p      = (int)( step % period );
                    return p < n ? p : (int)period - p;
                }
                case Random: return (int)( rng.next() % (uint32_t)n );
                case Up:
                default: return (int)( step % (uint32_t)n );
            }
        }

        void processTick( int64_t t, Result& r )
        {
            if( sounding >= 0 && t >= offTick )
            {
                r.add( (uint8_t)sounding, soundingVel, false );
                sounding = -1;
            }
            const int st = stepTicks( rate, ppqnV );
            if( t % st != 0 || nSeq == 0 ) return;
            const Held h = seq[(size_t)pickIndex()];
            ++step;
            if( sounding >= 0 ) { r.add( (uint8_t)sounding, soundingVel, false ); sounding = -1; }
            r.add( h.note, h.vel, true );
            sounding    = h.note;
            soundingVel = h.vel;
            offTick     = t + std::max( 1, st * gate / 100 );
        }

        Rng                             rng;
        int                             ppqnV;
        int                             mode{ Up };
        int                             rate{ R1_16 };
        int                             octaves{ 1 };
        int                             gate{ 50 };
        bool                            latch{ false };
        bool                            enabled{ false };
        std::array<Held, kMaxHeld>      held{};
        std::array<Held, kMaxSeq>       seq{};
        std::array<bool, 128>           down{};
        int                             nHeld{ 0 };
        int                             nSeq{ 0 };
        int                             downCount{ 0 };
        uint32_t                        step{ 0 };
        int                             sounding{ -1 };
        uint8_t                         soundingVel{ 0 };
        int64_t                         offTick{ 0 };
        int64_t                         lastTick{ 0 };
        bool                            haveLast{ false };
    };
} // namespace mt::arp
