#pragma once

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

// ============================================================================
// Stream-safe MIDI byte parser (no ALSA / JUCE dependency, host-testable).
//
// - Running status is kept for channel voice messages (0x80..0xEF) and is
//   cancelled by system common messages (0xF0..0xF7), per the MIDI spec.
// - System real-time bytes (0xF8..0xFF) may appear anywhere and are ignored.
// - SysEx payloads are skipped by the 3-argument feed(). The 4-argument feed() additionally reports every
//   complete message F0 .. F7 (kSysexMax bytes at most; a longer one is dropped whole, never truncated) to a
//   second callback: sx(const uint8_t* msg, size_t len), msg[0] = 0xF0, msg[len-1] = 0xF7. The assembly keeps
//   its state across feed() calls, so a SysEx split over several reads is reported once, complete.
//   A new status byte (0x80..0xEF) or another F0 inside a SysEx aborts it (lost F7) and is processed normally.
// - System common messages (0xF1..0xF6) are consumed including their data
//   bytes and never reach the callback.
// - Only complete channel voice messages are reported: cb(status, d0, d1).
//   For one-data-byte messages (0xC0, 0xD0) d1 is 0.
// ============================================================================
struct MidiParser {
    uint8_t status = 0;      // running status (0 = none)
    uint8_t data[2] = { 0, 0 };
    int data_idx = 0;
    int expected_data = 0;
    int swallow = 0;         // remaining data bytes of a system common message to skip
    bool in_sysex = false;

    static constexpr size_t kSysexMax = 32;  // longest SysEx kept whole (the Key 61 messages are <= 12 bytes)
    uint8_t sx[kSysexMax] = { 0 };
    size_t sx_len = 0;
    bool sx_overflow = false;

    void reset() {
        status = 0;
        data_idx = 0;
        expected_data = 0;
        swallow = 0;
        in_sysex = false;
        sx_len = 0;
        sx_overflow = false;
    }

    template<typename Callback>
    void feed(const uint8_t* buf, ssize_t n, Callback&& cb) {
        feed(buf, n, cb, [](const uint8_t*, size_t) {});
    }

    template<typename Callback, typename SysexCallback>
    void feed(const uint8_t* buf, ssize_t n, Callback&& cb, SysexCallback&& sxcb) {
        for (ssize_t i = 0; i < n; ++i) {
            const uint8_t b = buf[i];

            if (b >= 0xF8) {
                continue; // real-time: never disturbs parser state
            }

            if (b & 0x80) {
                if (b < 0xF0) {
                    // Channel voice status; also aborts an unterminated SysEx.
                    in_sysex = false;
                    swallow = 0;
                    status = b;
                    data_idx = 0;
                    switch (b & 0xF0) {
                        case 0xC0:
                        case 0xD0:
                            expected_data = 1;
                            break;
                        default: // 0x80, 0x90, 0xA0, 0xB0, 0xE0
                            expected_data = 2;
                            break;
                    }
                    continue;
                }

                // System common (0xF0..0xF7) cancels running status.
                const bool was_sysex = in_sysex;
                status = 0;
                data_idx = 0;
                expected_data = 0;
                swallow = 0;
                in_sysex = false;
                switch (b) {
                    case 0xF0:
                        in_sysex = true;       // a second F0 restarts the assembly (the old one lost its F7)
                        sx[0] = 0xF0;
                        sx_len = 1;
                        sx_overflow = false;
                        break;
                    case 0xF1: swallow = 1; break; // MTC quarter frame
                    case 0xF2: swallow = 2; break; // Song position
                    case 0xF3: swallow = 1; break; // Song select
                    case 0xF7:
                        if (was_sysex && !sx_overflow && sx_len < kSysexMax) {
                            sx[sx_len++] = 0xF7;
                            sxcb((const uint8_t*)sx, sx_len);
                        }
                        sx_len = 0;
                        sx_overflow = false;
                        break;
                    default:   break;              // 0xF4..0xF6: no data
                }
                continue;
            }

            // Data byte
            if (in_sysex) {
                if (sx_len < kSysexMax - 1) sx[sx_len++] = b;   // one slot stays free for the F7
                else sx_overflow = true;
                continue;
            }
            if (swallow > 0) {
                --swallow;
                continue;
            }
            if (status == 0 || expected_data == 0) {
                continue; // orphan data byte
            }

            data[data_idx++] = b;
            if (data_idx >= expected_data) {
                cb(status, data[0], (expected_data > 1 ? data[1] : (uint8_t)0));
                data_idx = 0; // keep running status
            }
        }
    }
};
