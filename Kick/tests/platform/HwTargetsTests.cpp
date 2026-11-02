// Host test for apps/kick/hardware/hw_targets.h. g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined
#include "../../apps/kick/hardware/hw_targets.h"
#include <cstdio>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++g_fail; } } while (0)

struct Sel { int track = 0; int sequence = 0; int program = 0; };

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
        // AA1b: CC1 / CC11 follow the current selection, also at a non-zero value after a track change
        CHECK(kick_hw::routeCc(0, Sel{ 5 }).track == 5);
        CHECK(kick_hw::routeCc(10, Sel{ 1 }).track == 1);
        CHECK(kick_hw::routeCc(60, Sel{ 2 }).track == 2);   // track switched while the wheel is at 60 -> new track
        CHECK(kick_hw::routeCc(127, Sel{ 3 }).track == 3);
        CHECK(kick_hw::routeCc(0, Sel{ 4 }).track == 4);
    }
    { // AF1d: physical pedal + strip Sustain, own targets, shared CC64 lane
        using SS = kick_hw::SustainSources<Sel>;
        auto same = kick_hw::sameSequenceTrack<Sel>; // the production lane identity
        Sel out;
        { // both hold T0: the first release stays silent, the last one sends CC64=0
            SS ss;
            CHECK(ss.press(SS::kPedal, Sel{ 0 }).track == 0);
            CHECK(ss.press(SS::kStrip, Sel{ 0 }).track == 0);
            CHECK(!ss.release(SS::kStrip, Sel{ 0 }, same, out));
            CHECK(ss.release(SS::kPedal, Sel{ 0 }, same, out) && out.track == 0);
        }
        { // strip holds T1, selection moves to T2, pedal pressed: pedal gets T2 (not the strip's stored T1)
            SS ss;
            ss.press(SS::kStrip, Sel{ 1 });
            CHECK(ss.press(SS::kPedal, Sel{ 2 }).track == 2);
            CHECK(ss.release(SS::kStrip, Sel{ 2 }, same, out) && out.track == 1); // other track: own CC64=0
            CHECK(ss.release(SS::kPedal, Sel{ 3 }, same, out) && out.track == 2);
        }
        { // AG1a: same (sequence, track), different program = same CC64 lane -> no CC64=0 while the other source holds
            SS ss;
            ss.press(SS::kPedal, Sel{ 0, 0, 0 });
            ss.press(SS::kStrip, Sel{ 0, 0, 1 }); // program changed in between
            CHECK(!ss.release(SS::kStrip, Sel{ 0, 0, 1 }, same, out));
            CHECK(ss.release(SS::kPedal, Sel{ 0, 0, 1 }, same, out) && out.program == 0);
            ss.press(SS::kStrip, Sel{ 0, 0, 2 });
            ss.press(SS::kPedal, Sel{ 0, 0, 3 });
            CHECK(!ss.release(SS::kPedal, Sel{ 0, 0, 3 }, same, out)); // and the other way round
            CHECK(ss.release(SS::kStrip, Sel{ 0, 0, 3 }, same, out));
            CHECK(!kick_hw::sameSequenceTrack(Sel{ 0, 1, 0 }, Sel{ 0, 0, 0 })); // other sequence = other lane
            CHECK(!kick_hw::sameSequenceTrack(Sel{ 1, 0, 0 }, Sel{ 0, 0, 0 })); // other track
        }
        { // strip alone
            SS ss;
            ss.press(SS::kStrip, Sel{ 4 });
            CHECK(ss.release(SS::kStrip, Sel{ 5 }, same, out) && out.track == 4);
        }
    }
    if (g_fail == 0) std::printf("hw_targets: all passed\n");
    return g_fail ? 1 : 0;
}
