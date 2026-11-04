// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

namespace ui
{
    // Which finger (MT slot) a mouse event delivered by the hardware bridge belongs to, and when the finger event was
    // read from the touch device. The bridge's manual delivery path (the one that runs on the MPC: no X display,
    // hidden window) calls mouseDown / mouseDrag / mouseUp directly and every such event carries the main mouse
    // source, index 0, whatever finger it came from; the bridge therefore opens a TouchSlot::Scope around each call.
    // A widget that needs per-finger state reads TouchSlot::of( event ): the slot while a Scope is open (hardware
    // bridge), else the JUCE source index (real touch input on desktop / iOS, or the mouse = 0).
    struct TouchSlot
    {
        struct State
        {
            int     slot    = -1;
            int64_t eventUs = 0; // steady_clock microseconds when the touch device event was read; 0 = unknown
        };
        static State& state()
        {
            static thread_local State s;
            return s;
        }
        struct Scope
        {
            Scope( int slot, int64_t eventUs ) : prev( state() ) { state() = { slot, eventUs }; }
            ~Scope() { state() = prev; }
            Scope( const Scope& ) = delete;
            Scope& operator=( const Scope& ) = delete;
            State  prev;
        };
        static bool    inBridge() { return state().slot >= 0; }
        static int64_t eventUs() { return state().eventUs; }

        static int64_t nowUs()
        {
            using namespace std::chrono;
            return duration_cast<microseconds>( steady_clock::now().time_since_epoch() ).count();
        }
    };

    // Touch -> sound latency trace: [LAT] lines through kick_log( "bridge", ... ), all behind one runtime switch
    // (off by default: a trace line per touch event is too much for the 30 Hz paint thread's neighbours).
    // On: kick_cmd "latency 1" / "latency 0", or the environment variable KICK_LATENCY=1 at start.
    struct LatencyTrace
    {
        static std::atomic<bool>& flag()
        {
            static std::atomic<bool> f{ false };
            return f;
        }
        static bool enabled() { return flag().load( std::memory_order_relaxed ); }
        static void set( bool on ) { flag().store( on, std::memory_order_relaxed ); }
    };
}
