// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the stream-safe MIDI parser used by the hardware adapters.
#include "../../apps/kick/hardware/midi_parser.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

struct Msg { uint8_t s, a, b; };

static void check(bool ok, const char* why)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}

static std::vector<Msg> run(MidiParser& p, std::vector<uint8_t> bytes)
{
    std::vector<Msg> out;
    p.feed(bytes.data(), (ssize_t)bytes.size(), [&](uint8_t s, uint8_t a, uint8_t b) { out.push_back({ s, a, b }); });
    return out;
}

int main()
{
    { // plain note on + running status
        MidiParser p;
        auto m = run(p, { 0x90, 60, 100, 62, 90 });
        check(m.size() == 2 && m[0].s == 0x90 && m[0].a == 60 && m[0].b == 100 && m[1].a == 62 && m[1].b == 90, "running status");
    }
    { // message split across reads
        MidiParser p;
        check(run(p, { 0x93, 40 }).empty(), "partial: nothing yet");
        auto m = run(p, { 127 });
        check(m.size() == 1 && m[0].s == 0x93 && m[0].a == 40 && m[0].b == 127, "partial: completed");
    }
    { // one-data-byte messages
        MidiParser p;
        auto m = run(p, { 0xDF, 55, 0xC2, 7 });
        check(m.size() == 2 && m[0].s == 0xDF && m[0].a == 55 && m[0].b == 0 && m[1].s == 0xC2 && m[1].a == 7, "channel pressure / program change");
    }
    { // real-time bytes inside a message do not break it
        MidiParser p;
        auto m = run(p, { 0x90, 0xF8, 60, 0xFE, 100 });
        check(m.size() == 1 && m[0].a == 60 && m[0].b == 100, "realtime interleaved");
    }
    { // SysEx is skipped, running status is cancelled by it
        MidiParser p;
        auto m = run(p, { 0x90, 60, 100, 0xF0, 0x47, 0x7F, 0x4B, 0xF7, 62, 90 });
        check(m.size() == 1 && m[0].a == 60, "sysex skipped, no stale running status");
    }
    { // lost F7: next status byte aborts the SysEx and is parsed
        MidiParser p;
        auto m = run(p, { 0xF0, 0x01, 0x02, 0x90, 61, 80 });
        check(m.size() == 1 && m[0].s == 0x90 && m[0].a == 61, "unterminated sysex aborted by status");
    }
    { // system common messages with data bytes must not overflow or leak (old OOB bug)
        MidiParser p;
        auto m = run(p, { 0xF1, 0x12, 0xF2, 0x10, 0x20, 0xF3, 0x05, 0xF6, 0x90, 60, 100 });
        check(m.size() == 1 && m[0].s == 0x90 && m[0].a == 60, "system common consumed");
    }
    { // long run of orphan data bytes after system common: no crash, nothing reported
        MidiParser p;
        std::vector<uint8_t> bytes = { 0xF1, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
        check(run(p, bytes).empty(), "orphan data ignored");
        auto m = run(p, { 0xB0, 100, 1 });
        check(m.size() == 1 && m[0].a == 100 && m[0].b == 1, "parser still usable");
    }
    { // data bytes with no status are ignored
        MidiParser p;
        check(run(p, { 10, 20, 30 }).empty(), "data without status");
    }
    { // reset drops partial state
        MidiParser p;
        run(p, { 0x90, 60 });
        p.reset();
        check(run(p, { 100 }).empty(), "reset clears running status");
    }
    std::puts("midi parser tests passed");
    return 0;
}
