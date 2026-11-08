#pragma once

#include <cstdint>
#include <atomic>
#include <algorithm>
#include <functional>

namespace kick_hw
{
    class ScreenDimmer
    {
    public:
        enum class State
        {
            Awake,
            Dimmed
        };

        using ClockFn = std::function<int64_t()>;
        using BrightnessFn = std::function<void(int percent)>;

        ScreenDimmer(ClockFn clock = nullptr, BrightnessFn setBrightness = nullptr)
            : m_clock(clock), m_setBrightness(setBrightness)
        {
            m_lastActivityMs.store(nowMs());
        }

        void setClockFn(ClockFn fn)
        {
            m_clock = fn;
            m_lastActivityMs.store(nowMs());
        }
        void setBrightnessFn(BrightnessFn fn) { m_setBrightness = fn; }

        void setDimAfterMinutes(int minutes)
        {
            m_dimAfterMinutes.store(std::max(0, minutes));
            if (m_dimAfterMinutes.load() == 0 && m_state.load() == State::Dimmed)
            {
                wake();
            }
        }

        int getDimAfterMinutes() const { return m_dimAfterMinutes.load(); }

        void setDimLevelPercent(int percent)
        {
            m_dimLevelPercent.store(std::clamp(percent, 5, 100));
        }

        int getDimLevelPercent() const { return m_dimLevelPercent.load(); }

        void setNormalBrightnessPercent(int percent)
        {
            m_normalBrightnessPercent.store(std::clamp(percent, 0, 100));
        }

        int getNormalBrightnessPercent() const { return m_normalBrightnessPercent.load(); }

        State getState() const { return m_state.load(); }
        bool isDimmed() const { return m_state.load() == State::Dimmed; }

        // Call on ANY hardware activity (buttons, jog, pads, keys, touch)
        void noteActivity()
        {
            m_lastActivityMs.store(nowMs());
            if (m_state.load() == State::Dimmed)
            {
                wake();
            }
        }

        // Call when touch down arrives
        // Returns true if this touch woke the display from Dimmed and should be swallowed
        bool onTouchDown(int slot)
        {
            const bool wasDimmed = (m_state.load() == State::Dimmed);
            noteActivity();
            if (wasDimmed)
            {
                if (slot >= 0 && slot < 10)
                {
                    m_swallowedSlots[slot] = true;
                }
                return true;
            }
            return false;
        }

        // Returns true if this touch event should be swallowed (not dispatched to UI)
        bool shouldSwallowTouch(int slot, bool isUp)
        {
            if (slot >= 0 && slot < 10 && m_swallowedSlots[slot])
            {
                if (isUp)
                {
                    m_swallowedSlots[slot] = false;
                }
                return true;
            }
            return false;
        }

        void checkIdle(bool transportPlaying, bool isRecording, bool isModalOpen)
        {
            const int dimMins = m_dimAfterMinutes.load();
            if (dimMins <= 0)
                return; // Disabled (Off)

            // Inhibits: playing, recording, or modal dialog prevents dimming
            if (transportPlaying || isRecording || isModalOpen)
            {
                m_lastActivityMs.store(nowMs());
                return;
            }

            if (m_state.load() == State::Awake)
            {
                const int64_t idleMs = nowMs() - m_lastActivityMs.load();
                const int64_t thresholdMs = static_cast<int64_t>(dimMins) * 60 * 1000;
                if (idleMs >= thresholdMs)
                {
                    dim();
                }
            }
        }

        void wake()
        {
            if (m_state.exchange(State::Awake) == State::Dimmed)
            {
                if (m_setBrightness)
                {
                    m_setBrightness(m_normalBrightnessPercent.load());
                }
            }
        }

        void dim()
        {
            if (m_state.exchange(State::Dimmed) == State::Awake)
            {
                if (m_setBrightness)
                {
                    const int normal = m_normalBrightnessPercent.load();
                    const int level = m_dimLevelPercent.load();
                    // Dim level is percentage of normal brightness, but minimum 10%
                    const int dimVal = std::clamp((normal * level) / 100, 10, normal);
                    m_setBrightness(dimVal);
                }
            }
        }

    private:
        int64_t nowMs() const
        {
            if (m_clock)
                return m_clock();
            return 0;
        }

        ClockFn m_clock;
        BrightnessFn m_setBrightness;

        std::atomic<State> m_state{ State::Awake };
        std::atomic<int> m_dimAfterMinutes{ 0 }; // 0 = Off
        std::atomic<int> m_dimLevelPercent{ 20 }; // default 20%
        std::atomic<int> m_normalBrightnessPercent{ 80 };
        std::atomic<int64_t> m_lastActivityMs{ 0 };
        bool m_swallowedSlots[10]{ false };
    };
}
