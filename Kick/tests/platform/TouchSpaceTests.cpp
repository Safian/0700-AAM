// Host test: touch coordinate chain raw ILI2117 -> framebuffer 1280x800 -> UI 1024x600. JUCE/ALSA free.
#include "../../apps/kick/hardware/touch_space.h"
#include <cstdio>
#include <cstdlib>

static int g_fail = 0;
static void check(bool ok, const char* what)
{
    if (!ok) { std::printf("FAIL: %s\n", what); ++g_fail; }
}

int main()
{
    using namespace kick_hw::probe;
    int x = 0, y = 0;

    // fb -> UI: corners, centre, exact scale (1280 -> 1024 is x0.8, 800 -> 600 is x0.75).
    fbToUi(0, 0, 1280, 800, 1024, 600, x, y);
    check(x == 0 && y == 0, "fb origin");
    fbToUi(1279, 799, 1280, 800, 1024, 600, x, y);
    check(x == 1023 && y == 599, "fb far corner stays inside the UI");
    fbToUi(640, 400, 1280, 800, 1024, 600, x, y);
    check(x == 512 && y == 300, "fb centre");
    fbToUi(1280, 800, 1280, 800, 1024, 600, x, y);
    check(x == 1023 && y == 599, "fb at the limit is clamped to the last pixel");
    fbToUi(-5, 9999, 1280, 800, 1024, 600, x, y);
    check(x == 0 && y == 599, "out of range is clamped");
    fbToUi(550, 181, 1280, 800, 1024, 600, x, y);
    check(x == 440 && y == 135, "a framebuffer point (550,181) is UI (440,135)");
    fbToUi(100, 50, 1280, 800, 1280, 800, x, y);
    check(x == 100 && y == 50, "identity when sizes are equal");
    fbToUi(100, 50, 0, 0, 1024, 600, x, y);
    check(x == 100 && y == 50, "degenerate size leaves the point alone");

    // Route: touch source available -> touch; else finger 0 is mouse source 0; other fingers dropped.
    check(chooseTouchRoute(true, 0) == TouchRoute::Touch, "XI: finger 0 touch");
    check(chooseTouchRoute(true, 3) == TouchRoute::Touch, "XI: finger 3 touch");
    check(chooseTouchRoute(false, 0) == TouchRoute::Mouse, "no XI: finger 0 mouse");
    check(chooseTouchRoute(false, 1) == TouchRoute::Drop, "no XI: finger 1 dropped");

    // Whole chain from the raw 2048x2048 range (rotated mount: UI x from inverted raw Y, UI y from raw X).
    rawToUi(0, 0, 2048, 2048, 1280, 800, 1024, 600, x, y);
    check(x == 1023 && y == 0, "raw (0,0) -> top right");
    rawToUi(2048, 2048, 2048, 2048, 1280, 800, 1024, 600, x, y);
    check(x == 0 && y == 599, "raw (2048,2048) -> bottom left");
    rawToUi(2048, 0, 2048, 2048, 1280, 800, 1024, 600, x, y);
    check(x == 1023 && y == 599, "raw (2048,0) -> bottom right");
    rawToUi(0, 2048, 2048, 2048, 1280, 800, 1024, 600, x, y);
    check(x == 0 && y == 0, "raw (0,2048) -> top left");
    rawToUi(1024, 1024, 2048, 2048, 1280, 800, 1024, 600, x, y);
    check(x >= 510 && x <= 512 && y >= 298 && y <= 300, "raw centre -> UI centre");
    rawToUi(5000, -5, 2048, 2048, 1280, 800, 1024, 600, x, y);
    check(x == 1023 && y == 599, "raw out of range is clamped");

    if (g_fail) return 1;
    std::puts("touch-space-tests OK");
    return 0;
}
