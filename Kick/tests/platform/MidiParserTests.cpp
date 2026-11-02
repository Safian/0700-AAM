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

using Sx = std::vector<std::vector<uint8_t>>;

static Sx runSx(MidiParser& p, std::vector<uint8_t> bytes, std::vector<Msg>* msgs = nullptr)
{
    Sx out;
    p.feed(bytes.data(), (ssize_t)bytes.size(),
           [&](uint8_t s, uint8_t a, uint8_t b) { if (msgs) msgs->push_back({ s, a, b }); },
           [&](const uint8_t* m, size_t n) { out.emplace_back(m, m + n); });
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
    // ---- SysEx assembly (4-argument feed) ----
    const std::vector<uint8_t> vol = { 0xF0, 0x47, 0x7F, 0x4B, 0x5B, 0x00, 0x01, 0x40, 0xF7 };
    { // complete message
        MidiParser p;
        auto x = runSx(p, vol);
        check(x.size() == 1 && x[0] == vol, "sysex: complete message reported whole");
    }
    { // split at every possible byte boundary, one read per piece
        for (size_t cut = 1; cut < vol.size(); ++cut) {
            MidiParser p;
            auto a = runSx(p, std::vector<uint8_t>(vol.begin(), vol.begin() + (long)cut));
            check(a.empty(), "sysex: nothing before F7");
            auto b = runSx(p, std::vector<uint8_t>(vol.begin() + (long)cut, vol.end()));
            check(b.size() == 1 && b[0] == vol, "sysex: split message reported once, complete");
        }
    }
    { // byte by byte
        MidiParser p;
        size_t got = 0;
        for (uint8_t b : vol) got += runSx(p, { b }).size();
        check(got == 1, "sysex: byte-wise feed reports once");
    }
    { // two back to back, with a channel message and a real-time byte in between
        MidiParser p;
        std::vector<Msg> msgs;
        std::vector<uint8_t> in = vol;
        in.push_back(0xB0); in.push_back(100); in.push_back(1);
        in.push_back(0xF8);
        in.insert(in.end(), vol.begin(), vol.end());
        auto x = runSx(p, in, &msgs);
        check(x.size() == 2 && msgs.size() == 1 && msgs[0].s == 0xB0 && msgs[0].a == 100 && msgs[0].b == 1, "sysex: back to back + channel message");
    }
    { // real-time byte inside the SysEx does not corrupt it
        MidiParser p;
        auto x = runSx(p, { 0xF0, 0x47, 0xF8, 0x7F, 0x4B, 0x5B, 0xFE, 0x00, 0x01, 0x10, 0xF7 });
        check(x.size() == 1 && x[0].size() == 9 && x[0][7] == 0x10, "sysex: realtime ignored inside");
    }
    { // too long: dropped whole, never truncated; parser stays usable
        MidiParser p;
        std::vector<uint8_t> big = { 0xF0 };
        for (size_t i = 0; i < MidiParser::kSysexMax + 10; ++i) big.push_back(0x11);
        big.push_back(0xF7);
        check(runSx(p, big).empty(), "sysex: too long dropped");
        check(runSx(p, vol).size() == 1, "sysex: usable after overflow");
        // exactly at the limit is still kept: F0 + (kSysexMax - 2) data + F7
        std::vector<uint8_t> fit = { 0xF0 };
        for (size_t i = 0; i < MidiParser::kSysexMax - 2; ++i) fit.push_back(0x22);
        fit.push_back(0xF7);
        auto x = runSx(p, fit);
        check(x.size() == 1 && x[0].size() == MidiParser::kSysexMax, "sysex: exactly kSysexMax kept");
        fit.insert(fit.begin() + 1, 0x22);
        check(runSx(p, fit).empty(), "sysex: kSysexMax + 1 dropped");
    }
    { // truncated (no F7): a following channel status aborts it, nothing reported, channel message parsed
        MidiParser p;
        std::vector<Msg> msgs;
        auto x = runSx(p, { 0xF0, 0x47, 0x7F, 0x4B, 0x90, 60, 100 }, &msgs);
        check(x.empty() && msgs.size() == 1 && msgs[0].s == 0x90, "sysex: truncated aborted, not reported");
        auto y = runSx(p, { 0xF7 });
        check(y.empty(), "sysex: stray F7 reports nothing");
    }
    { // a second F0 restarts the assembly: only the complete one is reported
        MidiParser p;
        auto x = runSx(p, { 0xF0, 0x47, 0x7F, 0xF0, 0x01, 0x02, 0xF7 });
        check(x.size() == 1 && x[0] == (std::vector<uint8_t>{ 0xF0, 0x01, 0x02, 0xF7 }), "sysex: F0 restarts");
    }
    { // stray F7 without F0, and empty F0 F7
        MidiParser p;
        check(runSx(p, { 0x10, 0xF7 }).empty(), "sysex: F7 without F0");
        auto x = runSx(p, { 0xF0, 0xF7 });
        check(x.size() == 1 && x[0].size() == 2, "sysex: empty F0 F7");
    }
    { // reset drops a half-assembled SysEx
        MidiParser p;
        runSx(p, { 0xF0, 0x47, 0x7F });
        p.reset();
        check(runSx(p, { 0x4B, 0xF7 }).empty(), "sysex: reset clears assembly");
    }
    { // the 3-argument feed still skips SysEx (compat)
        MidiParser p;
        auto m = run(p, { 0xF0, 0x47, 0xF7, 0x90, 60, 100 });
        check(m.size() == 1 && m[0].a == 60, "3-arg feed unchanged");
    }
    std::puts("midi parser tests passed");
    return 0;
}
