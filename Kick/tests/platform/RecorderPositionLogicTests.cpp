// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the JUCE-free Recorder position and wrap logic.
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined RecorderPositionLogicTests.cpp

#include "../../../AudIO/juce_modules/AudIO_juce/RecorderPositionLogic.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>

static int g_fail = 0;
#define CHECK(a)                                                                      \
    do {                                                                              \
        if (!(a)) {                                                                   \
            std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #a);          \
            ++g_fail;                                                                 \
        }                                                                             \
    } while (0)

using namespace mt::recorder_position_logic;

int main() {
    // 1. softLimit nominal and headroom tests
    CHECK(softLimit(0.0f) == 0.0f);
    CHECK(softLimit(0.25f) == 0.25f);
    CHECK(softLimit(0.5f) == 0.5f);
    CHECK(softLimit(0.75f) == 0.75f);
    CHECK(softLimit(1.0f) == 1.0f);
    CHECK(softLimit(-0.25f) == -0.25f);
    CHECK(softLimit(-0.75f) == -0.75f);
    CHECK(softLimit(-1.0f) == -1.0f);

    // Headroom: |x| > 1.0f gently saturates without abrupt wrap or hard clip
    const float lim2 = softLimit(2.0f);
    CHECK(lim2 > 1.0f && lim2 < 2.0f);
    const float limNeg2 = softLimit(-2.0f);
    CHECK(limNeg2 < -1.0f && limNeg2 > -2.0f);
    CHECK(softLimit(1.5f) < softLimit(2.5f));
    CHECK(softLimit(-2.5f) < softLimit(-1.5f));

    // 2. stepPosition forward and reverse
    CHECK(stepPosition(0, Direction::Forward) == 1);
    CHECK(stepPosition(100, Direction::Forward) == 101);
    CHECK(stepPosition(10, Direction::Reverse) == 9);
    CHECK(stepPosition(0, Direction::Reverse) == -1);

    // 3. isAtWrapBoundary and wrapPosition
    CHECK(isAtWrapBoundary(480, 480, Direction::Forward));
    CHECK(isAtWrapBoundary(500, 480, Direction::Forward));
    CHECK(!isAtWrapBoundary(479, 480, Direction::Forward));
    CHECK(!isAtWrapBoundary(0, 480, Direction::Forward));

    CHECK(isAtWrapBoundary(-1, 480, Direction::Reverse));
    CHECK(isAtWrapBoundary(-10, 480, Direction::Reverse));
    CHECK(!isAtWrapBoundary(0, 480, Direction::Reverse));
    CHECK(!isAtWrapBoundary(479, 480, Direction::Reverse));

    CHECK(wrapPosition(480, Direction::Forward) == 0);
    CHECK(wrapPosition(480, Direction::Reverse) == 479);
    CHECK(wrapPosition(0, Direction::Forward) == 0);

    // 4. handleRecordWrap
    // 4a: Non-looping (SampleRecorder) stops at end
    auto s1 = handleRecordWrap(false, false, 480, 480, Direction::Forward);
    CHECK(!s1.keepRecording);
    CHECK(!s1.startPreview);
    CHECK(s1.nextWritePos == 0);
    CHECK(s1.validFrames == 480);

    // 4b: Looping continuous (Looper, oneShot=false) wraps and continues
    auto s2 = handleRecordWrap(true, false, 480, 480, Direction::Forward);
    CHECK(s2.keepRecording);
    CHECK(!s2.startPreview);
    CHECK(s2.nextWritePos == 0);
    CHECK(s2.validFrames == 480);

    // 4c: Looping one-shot (LO-11, oneShot=true) transitions to preview!
    auto s3 = handleRecordWrap(true, true, 480, 480, Direction::Forward);
    CHECK(!s3.keepRecording);
    CHECK(s3.startPreview);
    CHECK(s3.nextReadPos == 0);
    CHECK(s3.validFrames == 480);

    // 4d: Reverse one-shot
    auto s4 = handleRecordWrap(true, true, 480, 480, Direction::Reverse);
    CHECK(!s4.keepRecording);
    CHECK(s4.startPreview);
    CHECK(s4.nextReadPos == 479);
    CHECK(s4.validFrames == 480);

    // 5. handlePreviewWrap
    // 5a: Looping preview wraps to start
    auto p1 = handlePreviewWrap(true, 480, Direction::Forward);
    CHECK(p1.startPreview);
    CHECK(p1.nextReadPos == 0);

    auto p2 = handlePreviewWrap(true, 480, Direction::Reverse);
    CHECK(p2.startPreview);
    CHECK(p2.nextReadPos == 479);

    // 5b: Non-looping preview stops
    auto p3 = handlePreviewWrap(false, 480, Direction::Forward);
    CHECK(!p3.startPreview);

    // 6. calculateProgress
    CHECK(calculateProgress(0, 1000) == 0.0f);
    CHECK(calculateProgress(500, 1000) == 0.5f);
    CHECK(calculateProgress(1000, 1000) == 1.0f);
    CHECK(calculateProgress(1500, 1000) == 1.0f);
    CHECK(calculateProgress(-10, 1000) == 0.0f);
    CHECK(calculateProgress(100, 0) == 0.0f);

    // 7. calculateExportFrames (LO-07 beat/bar-accurate export)
    // Non-looping (Sampler): exactly validFrames
    CHECK(calculateExportFrames(false, 350, 480, 1) == 350);
    CHECK(calculateExportFrames(false, 0, 480, 1) == 0);

    // Looping 1 bar or full cycle: always cycleFrames
    CHECK(calculateExportFrames(true, 400, 480, 1) == 480);
    CHECK(calculateExportFrames(true, 480, 480, 1) == 480);
    CHECK(calculateExportFrames(true, 500, 480, 1) == 480);

    // Looping 4 bars (cycle = 4000 frames -> 1000 frames/bar)
    // valid = 2200 -> rounds to 2 bars = 2000 frames
    CHECK(calculateExportFrames(true, 2200, 4000, 4) == 2000);
    // valid = 2800 -> rounds to 3 bars = 3000 frames
    CHECK(calculateExportFrames(true, 2800, 4000, 4) == 3000);
    // valid = 3900 -> rounds to 4 bars = 4000 frames
    CHECK(calculateExportFrames(true, 3900, 4000, 4) == 4000);
    // valid = 4000 -> 4 bars = 4000 frames
    CHECK(calculateExportFrames(true, 4000, 4000, 4) == 4000);
    // valid = 200 -> rounds to minimum 1 bar = 1000 frames
    CHECK(calculateExportFrames(true, 200, 4000, 4) == 1000);

    if (g_fail == 0) {
        std::printf("RecorderPositionLogicTests: all passed\n");
    }
    return g_fail == 0 ? 0 : 1;
}
