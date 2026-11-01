// Host test for apps/kick/hardware/hw_targets.h. g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined
#include "../../apps/kick/hardware/hw_targets.h"
#include <cstdio>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++g_fail; } } while (0)

struct Sel { int track = 0; };

int main()
{
    { // two Shift buttons
        kick_hw::ShiftCounter s;
        CHECK(!s.down());
        CHECK(s.press());    // left down -> EN_SHIFT pressed
        CHECK(!s.press());   // right down -> nothing
        CHECK(s.down());
        CHECK(!s.release()); // right up while left held -> still shifted
        CHECK(s.down());
        CHECK(s.release());  // left up -> released
        CHECK(!s.down());
        CHECK(!s.release()); // stray release ignored
        CHECK(!s.down());
        CHECK(s.press());    // still works after a stray release
    }
    { // note targets
        kick_hw::NoteTargets<Sel> n;
        n.set(60, Sel{ 3 });
        CHECK(n.take(60, Sel{ 7 }).track == 3);
        CHECK(n.take(60, Sel{ 7 }).track == 7); // forgotten
        CHECK(n.take(-1, Sel{ 9 }).track == 9);
        CHECK(n.take(128, Sel{ 9 }).track == 9);
        n.set(200, Sel{ 1 }); // ignored
        n.set(5, Sel{ 2 });
        n.clear();
        CHECK(n.take(5, Sel{ 8 }).track == 8);
    }
    { // sustain / bend target
        kick_hw::HeldTarget<Sel> h;
        CHECK(h.end(Sel{ 4 }).track == 4);   // release without press -> current
        CHECK(h.begin(Sel{ 1 }).track == 1); // pedal down on track 1
        CHECK(h.begin(Sel{ 2 }).track == 1); // further messages stay on track 1
        CHECK(h.peek(Sel{ 5 }).track == 1);
        CHECK(h.end(Sel{ 2 }).track == 1);   // release goes to track 1
        CHECK(!h.active());
        CHECK(h.peek(Sel{ 5 }).track == 5);
    }
    { // G16: bend centre tolerance, separate strip / wheel targets, CC targets
        CHECK(kick_hw::isBendCentre(8192) && kick_hw::isBendCentre(8190) && kick_hw::isBendCentre(8194));
        CHECK(!kick_hw::isBendCentre(8189) && !kick_hw::isBendCentre(8195) && !kick_hw::isBendCentre(0));
        kick_hw::HeldTarget<Sel> wheel, strip;
        CHECK(kick_hw::routeBend(wheel, 9000, Sel{ 1 }).track == 1);  // wheel bends on track 1
        CHECK(kick_hw::routeBend(strip, 7000, Sel{ 2 }).track == 2);  // strip bends on track 2 (own target)
        CHECK(kick_hw::routeBend(wheel, 9500, Sel{ 3 }).track == 1);
        CHECK(kick_hw::routeBend(wheel, 8191, Sel{ 3 }).track == 1);  // near-centre ends the wheel hold
        CHECK(!wheel.active() && strip.active());
        CHECK(kick_hw::routeBend(strip, 8192, Sel{ 3 }).track == 2);
        CHECK(!strip.active());
        kick_hw::HeldTarget<Sel> cc;
        CHECK(kick_hw::routeCc(cc, 0, Sel{ 5 }).track == 5);          // 0 without hold -> current
        CHECK(kick_hw::routeCc(cc, 10, Sel{ 1 }).track == 1);
        CHECK(kick_hw::routeCc(cc, 60, Sel{ 2 }).track == 1);
        CHECK(kick_hw::routeCc(cc, 0, Sel{ 2 }).track == 1);          // 0 returns to track 1
        CHECK(!cc.active());
    }
    if (g_fail == 0) std::printf("hw_targets: all passed\n");
    return g_fail ? 1 : 0;
}
