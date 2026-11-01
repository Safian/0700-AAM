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
    if (g_fail == 0) std::printf("hw_targets: all passed\n");
    return g_fail ? 1 : 0;
}
