// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace mt {
namespace recorder_position_logic {

enum class Direction { Forward, Reverse };

// Soft limiter with unity slope for nominal range (|x| <= 1.0f).
// For values beyond 1.0f, gently compresses towards 2.0f
// to provide ample headroom during multi-layer overdubs without harsh digital clipping.
inline float softLimit(float x) noexcept {
    if (x > 1.0f) {
        return 1.0f + std::tanh(x - 1.0f);
    } else if (x < -1.0f) {
        return -1.0f + std::tanh(x + 1.0f);
    }
    return x;
}

// Compute the next step in the buffer
inline int stepPosition(int currentPos, Direction dir) noexcept {
    return currentPos + (dir == Direction::Forward ? 1 : -1);
}

// Check if position reached or passed the wrap boundaries
inline bool isAtWrapBoundary(int pos, int totalFrames, Direction dir) noexcept {
    if (totalFrames <= 0) return true;
    if (dir == Direction::Forward) {
        return pos >= totalFrames;
    } else {
        return pos < 0;
    }
}

// Wrap position to the start or end depending on direction
inline int wrapPosition(int totalFrames, Direction dir) noexcept {
    if (totalFrames <= 0) return 0;
    return dir == Direction::Forward ? 0 : totalFrames - 1;
}

struct StateTransition {
    bool keepRecording;
    bool startPreview;
    int nextWritePos;
    int nextReadPos;
    int validFrames;
};

// Handle boundary wrap when recording.
// looping: is loop mode active?
// oneShot: should recording stop and switch to playback after 1 cycle of N bars?
// currentValid: currently valid frames
// totalFrames: allocated buffer frames (cycle length)
inline StateTransition handleRecordWrap(bool looping, bool oneShot, int currentValid, int totalFrames, Direction dir) noexcept {
    StateTransition result{};
    const int finalFrames = std::max(currentValid, totalFrames);
    if (!looping) {
        // Sample recording: stop at end of buffer
        result.keepRecording = false;
        result.startPreview = false;
        result.nextWritePos = wrapPosition(totalFrames, dir);
        result.nextReadPos = 0;
        result.validFrames = finalFrames;
    } else if (oneShot) {
        // One-shot N bars looper: transition from recording to playback!
        result.keepRecording = false;
        result.startPreview = true;
        result.nextWritePos = wrapPosition(totalFrames, dir);
        result.nextReadPos = wrapPosition(totalFrames, dir);
        result.validFrames = finalFrames;
    } else {
        // Continuous looping record/overdub: keep recording, wrap to start
        result.keepRecording = true;
        result.startPreview = false;
        result.nextWritePos = wrapPosition(totalFrames, dir);
        result.nextReadPos = 0;
        result.validFrames = finalFrames;
    }
    return result;
}

// Handle boundary wrap during playback (preview)
inline StateTransition handlePreviewWrap(bool looping, int validFrames, Direction dir) noexcept {
    StateTransition result{};
    result.keepRecording = false;
    if (validFrames <= 0) {
        result.startPreview = false;
        result.nextReadPos = 0;
        result.validFrames = 0;
        return result;
    }
    if (looping) {
        result.startPreview = true;
        result.nextReadPos = wrapPosition(validFrames, dir);
        result.validFrames = validFrames;
    } else {
        result.startPreview = false;
        result.nextReadPos = wrapPosition(validFrames, dir);
        result.validFrames = validFrames;
    }
    return result;
}

// Progress calculation [0.0f, 1.0f]
inline float calculateProgress(int pos, int totalFrames) noexcept {
    if (totalFrames <= 0) return 0.0f;
    const float p = static_cast<float>(pos) / static_cast<float>(totalFrames);
    if (p < 0.0f) return 0.0f;
    if (p > 1.0f) return 1.0f;
    return p;
}

// Compute aligned export length:
// If isLooping:
// If numBars > 0 and cycleFrames > 0:
// each bar has barFrames = cycleFrames / numBars.
// The export length is aligned to integer number of bars or cycleFrames.
inline int calculateExportFrames(bool isLooping, int validFrames, int cycleFrames, int numBars) noexcept {
    if (!isLooping) {
        return std::max(0, validFrames);
    }
    if (cycleFrames <= 0) return std::max(0, validFrames);
    if (numBars <= 1) {
        // 1 bar or full cycle: always export the exact cycleFrames
        return cycleFrames;
    }
    const int barFrames = cycleFrames / numBars;
    if (barFrames <= 0) return cycleFrames;
    if (validFrames >= cycleFrames) return cycleFrames;
    // Align to integer bars:
    int bars = (validFrames + barFrames / 2) / barFrames;
    if (bars < 1) bars = 1;
    if (bars > numBars) bars = numBars;
    return bars * barFrames;
}

} // namespace recorder_position_logic
} // namespace mt
