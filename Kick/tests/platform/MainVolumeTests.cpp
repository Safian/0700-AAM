// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the Akai MPC Key 61 Main Volume SysEx decoder (F0 47 7F 4B 5B 00 01 <v> F7, v = 0x00..0x75, measured)
// together with the SysEx assembly of MidiParser.
#include "../../apps/kick/hardware/main_volume.h"
#include "../../apps/kick/hardware/midi_parser.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace kick_hw::mainvol;

static void check(bool ok, const char* why)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}

static std::vector<uint8_t> msgWith(uint8_t v) { return { 0xF0, 0x47, 0x7F, 0x4B, 0x5B, 0x00, 0x01, v, 0xF7 }; }

static Result dec(const std::vector<uint8_t>& m) { return decode(m.data(), m.size()); }

int main()
{
    { // limits of the measured range
        Result lo = dec(msgWith(0x00)), hi = dec(msgWith(0x75));
        check(lo.kind == Kind::MainVolume && lo.value == 0 && !lo.clamped, "v = 0x00");
        check(hi.kind == Kind::MainVolume && hi.value == 0x75 && !hi.clamped, "v = 0x75");
        check(toMidi127(0) == 0 && toMidi127(0x75) == 127, "scale endpoints 0 -> 0, 0x75 -> 127");
    }
    { // above the measured range: clamped
        for (int v = 0x76; v <= 0x7F; ++v) {
            Result r = dec(msgWith((uint8_t)v));
            check(r.kind == Kind::MainVolume && r.value == 0x75 && r.clamped, "v > 0x75 clamped");
        }
        check(toMidi127(0x7F) == 127 && toMidi127(-5) == 0, "toMidi127 clamps");
    }
    { // every step maps monotonically into 0..127
        int prev = -1;
        for (int v = 0; v <= 0x75; ++v) {
            int m = toMidi127(v);
            check(m >= prev && m >= 0 && m <= 127, "monotonic 0..127");
            prev = m;
        }
    }
    { // any other command / length / device is an unknown Akai message, never MainVolume
        auto cmd = msgWith(0x10); cmd[4] = 0x5C;
        auto dev = msgWith(0x10); dev[2] = 0x00;
        auto prod = msgWith(0x10); prod[3] = 0x4C;
        auto len = msgWith(0x10); len[6] = 0x02;
        std::vector<uint8_t> longer = { 0xF0, 0x47, 0x7F, 0x4B, 0x5B, 0x00, 0x01, 0x10, 0x11, 0xF7 };
        std::vector<uint8_t> shorter = { 0xF0, 0x47, 0x7F, 0x4B, 0x5B, 0x00, 0x01, 0xF7 };
        check(dec(cmd).kind == Kind::UnknownAkai, "other command byte");
        check(dec(dev).kind == Kind::UnknownAkai, "other device id");
        check(dec(prod).kind == Kind::UnknownAkai, "other product byte");
        check(dec(len).kind == Kind::UnknownAkai, "other length field");
        check(dec(longer).kind == Kind::UnknownAkai, "too long");
        check(dec(shorter).kind == Kind::UnknownAkai, "too short");
    }
    { // other manufacturers and malformed framing are ignored
        std::vector<uint8_t> identity = { 0xF0, 0x7E, 0x00, 0x06, 0x02, 0x47, 0xF7 };
        check(dec(identity).kind == Kind::NotAkai, "universal SysEx ignored");
        auto noEnd = msgWith(0x10); noEnd[8] = 0x00;
        check(dec(noEnd).kind == Kind::NotAkai, "no F7");
        check(decode(nullptr, 0).kind == Kind::NotAkai && decode(msgWith(1).data(), 2).kind == Kind::NotAkai, "null / tiny");
    }
    { // end to end: byte stream in odd chunks -> parser -> decoder; other SysEx in between is ignored
        MidiParser p;
        std::vector<uint8_t> stream;
        auto a = msgWith(0x00), b = msgWith(0x75), c = msgWith(0x7A);
        std::vector<uint8_t> other = { 0xF0, 0x7E, 0x7F, 0x06, 0x02, 0xF7 };
        for (auto* m : { &a, &other, &b, &c }) stream.insert(stream.end(), m->begin(), m->end());
        std::vector<int> values;
        int ignored = 0;
        for (size_t i = 0; i < stream.size(); i += 5) {   // 5-byte reads cut messages in the middle
            size_t n = stream.size() - i < 5 ? stream.size() - i : 5;
            p.feed(stream.data() + i, (ssize_t)n, [](uint8_t, uint8_t, uint8_t) {},
                   [&](const uint8_t* m, size_t len) {
                       Result r = decode(m, len);
                       if (r.kind == Kind::MainVolume) values.push_back(r.value); else ++ignored;
                   });
        }
        check(values.size() == 3 && values[0] == 0 && values[1] == 0x75 && values[2] == 0x75 && ignored == 1, "stream end to end");
    }
    std::puts("main volume tests passed");
    return 0;
}
