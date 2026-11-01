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
// - SysEx payloads are skipped. A new status byte (0x80..0xEF) inside a SysEx
//   aborts it (lost F7) and is processed normally.
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

    void reset() {
        status = 0;
        data_idx = 0;
        expected_data = 0;
        swallow = 0;
        in_sysex = false;
    }

    template<typename Callback>
    void feed(const uint8_t* buf, ssize_t n, Callback&& cb) {
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
                status = 0;
                data_idx = 0;
                expected_data = 0;
                swallow = 0;
                in_sysex = false;
                switch (b) {
                    case 0xF0: in_sysex = true; break;
                    case 0xF1: swallow = 1; break; // MTC quarter frame
                    case 0xF2: swallow = 2; break; // Song position
                    case 0xF3: swallow = 1; break; // Song select
                    default:   break;              // 0xF4..0xF7: no data
                }
                continue;
            }

            // Data byte
            if (in_sysex) {
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
