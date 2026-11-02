#pragma once

#include "../hardware_adapter.h"
#include "../midi_parser.h"
#include "../main_volume.h"
#include "../led_map.h"
#include "../akai_map.h"
#include "../device_probe.h"
#include "../strip_modes.h"
#include <atomic>
#include <dirent.h>
#include <mutex>
#include <string>
#include <vector>
#include <alsa/asoundlib.h>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <pthread.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Reads a whole (small) text file, e.g. /proc/asound/cards. Empty string on failure.
static inline std::string akai_read_text_file(const char* path) {
    std::string out;
    FILE* f = fopen(path, "r");
    if (!f) return out;
    char buf[512];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    fclose(f);
    return out;
}

// Card index by /proc/asound/cards text, or 'fallback' (the previously hard-coded value) when not found.
static inline int akai_find_card(const std::vector<std::string>& needles, int fallback) {
    return kick_hw::probe::chooseOrFallback(
        kick_hw::probe::findCardIndex(akai_read_text_file("/proc/asound/cards"), needles), fallback);
}

// Touch device path: /sys/class/input/event*/device/name, then /proc/bus/input/devices, else /dev/input/event0.
static inline std::string akai_find_touch_device() {
    DIR* d = opendir("/sys/class/input");
    if (d) {
        struct dirent* e;
        while ((e = readdir(d)) != nullptr) {
            if (strncmp(e->d_name, "event", 5) != 0) continue;
            std::string p = std::string("/sys/class/input/") + e->d_name + "/device/name";
            std::string name = akai_read_text_file(p.c_str());
            if (kick_hw::probe::isTouchDeviceName(name)) {
                std::string path = std::string("/dev/input/") + e->d_name;
                closedir(d);
                return path;
            }
        }
        closedir(d);
    }
    int n = kick_hw::probe::findTouchEventInProcDevices(akai_read_text_file("/proc/bus/input/devices"));
    if (n >= 0) return "/dev/input/event" + std::to_string(n);
    return "/dev/input/event0";
}

// ============================================================================
// Akai MPC Key 61 Hardware Adapter
//
// Encapsulates all Akai-specific hardware quirks:
// 1. Display: 90-degree CW matrix rotation (1280x800 logical -> 800x1280 physical)
// 2. Touch: ILI2117 multi-touch evdev (/dev/input/event0) with inverted matrix
// 3. Control Surface: RawMIDI port 0 (Keybed), port 1 (Control Surface), port 2 (DIN A)
// 4. Audio: Rockchip ACVM 4-Channel Quad I2S Codec (plughw:1,0)
// ============================================================================

class AkaiMpcKey61DisplayAdapter : public IDisplayAdapter {
private:
    int m_fb_fd;
    uint32_t* m_fb_ptr;
    size_t m_fb_size;
    int m_phys_w;
    int m_phys_h;
    int m_logical_w;
    int m_logical_h;

public:
    AkaiMpcKey61DisplayAdapter()
        : m_fb_fd(-1), m_fb_ptr(nullptr), m_fb_size(0),
          m_phys_w(800), m_phys_h(1280), m_logical_w(1280), m_logical_h(800) {}

    virtual ~AkaiMpcKey61DisplayAdapter() { shutdown(); }

    bool init(int logical_w, int logical_h) override {
        if (m_fb_ptr) return true;
        m_logical_w = logical_w;
        m_logical_h = logical_h;

        m_fb_fd = open("/dev/fb0", O_RDWR);
        if (m_fb_fd < 0) {
            perror("[AKAI-DISPLAY] Cannot open /dev/fb0");
            return false;
        }

        struct fb_var_screeninfo vinfo;
        struct fb_fix_screeninfo finfo;
        ioctl(m_fb_fd, FBIOGET_VSCREENINFO, &vinfo);
        ioctl(m_fb_fd, FBIOGET_FSCREENINFO, &finfo);

        m_phys_w = vinfo.xres;
        m_phys_h = vinfo.yres;
        m_fb_size = finfo.line_length * vinfo.yres;

        m_fb_ptr = (uint32_t*)mmap(NULL, m_fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fb_fd, 0);
        if (m_fb_ptr == MAP_FAILED) {
            perror("[AKAI-DISPLAY] mmap /dev/fb0 failed");
            close(m_fb_fd);
            m_fb_fd = -1;
            return false;
        }

        printf("[AKAI-DISPLAY] Physical panel %dx%d (32bpp) mapped -> 90-degree CW matrix active for %dx%d UI\n",
               m_phys_w, m_phys_h, m_logical_w, m_logical_h);
        return true;
    }

    void blit(const uint32_t* logical_pixels, int w, int h, int line_stride_pixels = 0) override {
        if (!m_fb_ptr || !logical_pixels) return;
        int stride = (line_stride_pixels > 0) ? line_stride_pixels : w;

        int max_y = (h < m_phys_w) ? h : m_phys_w;
        int max_x = (w < m_phys_h) ? w : m_phys_h;

        // 90-degree clockwise rotation:
        // x_phys = y_ui
        // y_phys = (w - 1) - x_ui
        for (int y_ui = 0; y_ui < max_y; ++y_ui) {
            int x_phys = y_ui;
            const uint32_t* row = logical_pixels + y_ui * stride;
            for (int x_ui = 0; x_ui < max_x; ++x_ui) {
                int y_phys = (w - 1) - x_ui;
                if (y_phys >= 0 && y_phys < m_phys_h) {
                    m_fb_ptr[y_phys * m_phys_w + x_phys] = row[x_ui];
                }
            }
        }
    }

    void shutdown() override {
        if (m_fb_ptr && m_fb_ptr != MAP_FAILED) {
            munmap(m_fb_ptr, m_fb_size);
            m_fb_ptr = nullptr;
        }
        if (m_fb_fd >= 0) {
            close(m_fb_fd);
            m_fb_fd = -1;
        }
    }

    int get_physical_width() const override { return m_phys_w; }
    int get_physical_height() const override { return m_phys_h; }
};

class AkaiMpcKey61TouchAdapter : public ITouchAdapter {
private:
    struct Slot {
        bool active;
        bool was_down;
        int raw_x, raw_y;
        bool has_x, has_y;
        int last_x, last_y;
    };
    static const int MAX_SLOTS = 10;
    Slot m_slots[MAX_SLOTS];
    int m_current_slot;

    int m_fd;
    pthread_t m_thread;
    volatile bool m_running;
    IHardwareEventListener* m_listener;
    int m_logical_w;
    int m_logical_h;
    int m_range_x;   // ABS_MT_POSITION_X maximum (EVIOCGABS), 2048 when unknown
    int m_range_y;

    static void* thread_entry(void* arg) {
        ((AkaiMpcKey61TouchAdapter*)arg)->run();
        return NULL;
    }

    void run() {
        struct pollfd pfd = { m_fd, POLLIN, 0 };
        struct input_event ev;

        while (m_running) {
            pfd.revents = 0;
            if (poll(&pfd, 1, 50) <= 0) continue;

            while (read(m_fd, &ev, sizeof(ev)) == sizeof(ev)) {
                if (ev.type == EV_ABS) {
                    if (ev.code == ABS_MT_SLOT) {
                        if (ev.value >= 0 && ev.value < MAX_SLOTS) {
                            m_current_slot = ev.value;
                        }
                    } else if (ev.code == ABS_MT_TRACKING_ID) {
                        Slot* slot = &m_slots[m_current_slot];
                        if (ev.value == -1) {
                            if (slot->active && slot->was_down && m_listener) {
                                m_listener->on_touch_up(m_current_slot);
                            }
                            slot->active = false;
                            slot->was_down = false;
                            slot->has_x = false;
                            slot->has_y = false;
                        } else {
                            slot->active = true;
                        }
                    } else if (ev.code == ABS_MT_POSITION_X || ev.code == ABS_X) {
                        m_slots[m_current_slot].active = true;
                        m_slots[m_current_slot].raw_x = ev.value;
                        m_slots[m_current_slot].has_x = true;
                    } else if (ev.code == ABS_MT_POSITION_Y || ev.code == ABS_Y) {
                        m_slots[m_current_slot].active = true;
                        m_slots[m_current_slot].raw_y = ev.value;
                        m_slots[m_current_slot].has_y = true;
                    }
                } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
                    if (ev.value == 1) {
                        m_slots[0].active = true;
                    } else if (ev.value == 0) {
                        for (int s = 0; s < MAX_SLOTS; ++s) {
                            if (m_slots[s].active && m_slots[s].was_down && m_listener) {
                                m_listener->on_touch_up(s);
                            }
                            m_slots[s].active = false;
                            m_slots[s].was_down = false;
                            m_slots[s].has_x = false;
                            m_slots[s].has_y = false;
                        }
                    }
                } else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
                    for (int s = 0; s < MAX_SLOTS; ++s) {
                        Slot* slot = &m_slots[s];
                        if (!slot->active) continue;
                        if (!slot->has_x && !slot->was_down) continue;
                        if (!slot->has_y && !slot->was_down) continue;

                        // Inverted rotation matrix for ILI2117 (0..m_range, 2048 by default):
                        int ui_x = 0, ui_y = 0;
                        kick_hw::probe::mapTouch(slot->raw_x, slot->raw_y, m_range_x, m_range_y,
                                                 m_logical_w, m_logical_h, ui_x, ui_y);

                        if (!slot->was_down) {
                            printf("[AKAI-TOUCH] down slot=%d raw=(%d,%d) range=%dx%d -> fb=(%d,%d) of %dx%d\n",
                                   s, slot->raw_x, slot->raw_y, m_range_x, m_range_y, ui_x, ui_y, m_logical_w, m_logical_h);
                            fflush(stdout);
                            if (m_listener) {
                                m_listener->on_touch_down(s, ui_x, ui_y);
                            }
                            slot->was_down = true;
                        } else if (ui_x != slot->last_x || ui_y != slot->last_y) {
                            if (m_listener) {
                                m_listener->on_touch_move(s, ui_x, ui_y);
                            }
                        }
                        slot->last_x = ui_x;
                        slot->last_y = ui_y;
                    }
                }
            }
        }
    }

public:
    AkaiMpcKey61TouchAdapter()
        : m_current_slot(0), m_fd(-1), m_running(false), m_listener(nullptr),
          m_logical_w(1280), m_logical_h(800),
          m_range_x(kick_hw::probe::kDefaultTouchRange), m_range_y(kick_hw::probe::kDefaultTouchRange) {
        memset(m_slots, 0, sizeof(m_slots));
    }

    virtual ~AkaiMpcKey61TouchAdapter() { stop(); }

    bool start(IHardwareEventListener* listener, int logical_w, int logical_h) override {
        m_listener = listener;
        m_logical_w = logical_w;
        m_logical_h = logical_h;

        // Discovery with safe fallback: ILI2117 by name, else the previously hard-coded /dev/input/event0.
        const std::string dev = akai_find_touch_device();
        m_fd = open(dev.c_str(), O_RDONLY | O_NONBLOCK);
        if (m_fd < 0 && dev != "/dev/input/event0") {
            perror("[AKAI-TOUCH] Cannot open discovered touch device, trying /dev/input/event0");
            m_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
        }
        if (m_fd < 0) {
            perror("[AKAI-TOUCH] Cannot open touch device");
            return false;
        }

        // Real ABS ranges; falls back to 2048 when the ioctl fails.
        struct input_absinfo ai;
        memset(&ai, 0, sizeof(ai));
        m_range_x = kick_hw::probe::touchRangeOrDefault(
            ioctl(m_fd, EVIOCGABS(ABS_MT_POSITION_X), &ai) == 0 ? ai.maximum : 0);
        memset(&ai, 0, sizeof(ai));
        m_range_y = kick_hw::probe::touchRangeOrDefault(
            ioctl(m_fd, EVIOCGABS(ABS_MT_POSITION_Y), &ai) == 0 ? ai.maximum : 0);

        m_running = true;
        pthread_create(&m_thread, NULL, thread_entry, this);
        printf("[AKAI-TOUCH] Connected to ILI2117 (%s, range %dx%d) -> Transform to %dx%d UI active\n",
               dev.c_str(), m_range_x, m_range_y, m_logical_w, m_logical_h);
        return true;
    }

    void stop() override {
        if (m_running) {
            m_running = false;
            pthread_join(m_thread, NULL);
        }
        if (m_fd >= 0) {
            close(m_fd);
            m_fd = -1;
        }
    }
};

class AkaiMpcKey61ControlSurfaceAdapter : public IControlSurfaceAdapter {
private:
    snd_rawmidi_t *m_in0, *m_in1, *m_in2;
    snd_rawmidi_t *m_out0, *m_out1, *m_out2;
    // T5 runtime switch: false = no Port 0 writes at all (init SysEx + pad RGB). Set once in start() from the
    // environment variable KICK_AKAI_PORT0 (value "0" disables; unset / anything else = current behaviour, writes on).
    bool m_port0_writes = true;
    std::atomic<int> m_strip_msb { 0 };   // last CC33 / CC34 of the touch strip (14-bit position)
    std::atomic<int> m_strip_lsb { 0 };
    std::atomic<bool> m_out1_err_logged { false }; // first snd_rawmidi_write error per output handle
    std::atomic<bool> m_out0_err_logged { false };
    kick_hw::LedLevelCache m_led_cache; // last level per button LED (only changes are written)
    pthread_t m_thread;
    volatile bool m_running;
    IHardwareEventListener* m_listener;

    static void* thread_entry(void* arg) {
        ((AkaiMpcKey61ControlSurfaceAdapter*)arg)->run();
        return NULL;
    }

    MidiParser m_parser[3];          // one per port; shared by the reader thread and inject_bytes()
    std::mutex m_parser_mutex;       // guards m_parser[] only; never held while listener callbacks run
    std::mutex m_out0_mutex;         // one write guard per output handle (several threads write LEDs / pads)
    std::mutex m_out1_mutex;

    // The write is the only thing under the lock (plus the LED cache update that has to stay ordered with it).
    inline void locked_write(std::mutex& mx, snd_rawmidi_t* out, const uint8_t* data, size_t len) {
        std::lock_guard<std::mutex> lock(mx);
        const ssize_t r = snd_rawmidi_write(out, data, len);
        if (r < 0) log_write_error(out, r);
    }

    // Logs only the first write error per output handle (out0 / out1; others share the out1 flag).
    void log_write_error(snd_rawmidi_t* out, ssize_t r) {
        std::atomic<bool>& flag = (out == m_out0) ? m_out0_err_logged : m_out1_err_logged;
        if (flag.exchange(true)) return;
        fprintf(stderr, "[AKAI-CONTROLLER] snd_rawmidi_write error on %s: %zd (%s)\n",
                out == m_out0 ? "Port 0" : "Port 1", r, snd_strerror((int)r));
    }

    struct ParsedMsg { uint8_t s, d0, d1; };
    // One complete incoming SysEx, decoded under the parser lock (pure function), acted on after it.
    struct ParsedSysex { kick_hw::mainvol::Result res; uint8_t head[16]; uint8_t head_len; size_t full_len; };
    std::atomic<bool> m_unknown_sysex_logged { false };   // log the first unrecognised Akai SysEx only

    // Parser + dispatch for one port; used by run() and inject_bytes(). The parser runs under m_parser_mutex,
    // collecting complete messages; the listener callbacks run afterwards, without the lock.
    void feed_port(int port, const uint8_t* buf, size_t n) {
        if (port < 0 || port > 2) return;
        while (n > 0) {
            const size_t chunk = n > 256 ? 256 : n;
            ParsedMsg msgs[256];
            ParsedSysex sysex[128];   // a framed SysEx is >= 2 bytes, so a 256 byte chunk holds at most 128
            int count = 0;
            int sx_count = 0;
            {
                std::lock_guard<std::mutex> lock(m_parser_mutex);
                m_parser[port].feed(buf, (ssize_t)chunk, [&](uint8_t s, uint8_t d0, uint8_t d1) {
                    if (count < 256) msgs[count++] = { s, d0, d1 };
                }, [&](const uint8_t* m, size_t len) {
                    if (sx_count >= 128) return;
                    ParsedSysex& e = sysex[sx_count++];
                    e.res = kick_hw::mainvol::decode(m, len);
                    e.full_len = len;
                    e.head_len = (uint8_t)(len < sizeof(e.head) ? len : sizeof(e.head));
                    for (uint8_t k = 0; k < e.head_len; ++k) e.head[k] = m[k];
                });
            }
            for (int i = 0; i < count; ++i) handle_midi_message(port, msgs[i].s, msgs[i].d0, msgs[i].d1);
            for (int i = 0; i < sx_count; ++i) handle_sysex(port, sysex[i]);
            buf += chunk;
            n -= chunk;
        }
    }

    void handle_sysex(int port, const ParsedSysex& e) {
        using kick_hw::mainvol::Kind;
        if (!m_listener || e.res.kind == Kind::NotAkai) return;   // other manufacturers' SysEx: ignored
        if (port == 1 && e.res.kind == Kind::MainVolume) {
            m_listener->on_main_volume(kick_hw::mainvol::toMidi127(e.res.value));
            return;
        }
        // Akai SysEx we do not understand (other command / length, or any Akai SysEx on Port 0/2): not dropped
        // silently, logged once.
        if (m_unknown_sysex_logged.exchange(true)) return;
        char hex[16 * 3 + 1] = { 0 };
        for (uint8_t k = 0; k < e.head_len; ++k) snprintf(hex + k * 3, 4, "%02X ", e.head[k]);
        printf("[AKAI-SYSEX] Unrecognised Akai SysEx on port %d (len %zu): %s%s (logged once)\n",
               port, e.full_len, hex, e.full_len > e.head_len ? "..." : "");
        fflush(stdout);
    }

    // Note tables live in hardware/akai_map.h (ALSA-free, host-tested).
    static inline int get_akai_pad_index(int note) { return kick_hw::akai::get_akai_pad_index(note); }
    static inline HardwareButton get_akai_button(int note) { return kick_hw::akai::get_akai_button(note); }

    void handle_midi_message(int port, uint8_t status, uint8_t d0, uint8_t d1) {
        if (!m_listener) return;

        int type = status & 0xF0;
        int ch   = status & 0x0F;

        if (port == 1) {
            // Port 1 is MPC Private: Control Surface, Pads, Keybed strictly separated by channel
            if (ch == 0) {
                // CH 1: 45 Physical Buttons, Q-Links, Data Dial, Touch Strip
                if (type == 0x90) {
                    int note = d0, vel = d1;
                    if (note >= 84 && note <= 87) {
                        m_listener->on_encoder_touch(note - 84, vel > 0);
                    } else if (note == 78) {
                        // Touch strip finger touch (127) / release (0)
                        m_listener->on_touch_strip_touch(vel > 0);
                    } else {
                        HardwareButton btn = get_akai_button(note);
                        if (btn != HW_BTN_NONE) {
                            printf("[AKAI-BTN] note=%d vel=%d\n", note, vel);
                            fflush(stdout);
                            if (vel > 0) m_listener->on_button_press(btn);
                            else m_listener->on_button_release(btn);
                        } else {
                            printf("[AKAI-BTN] Unmapped Port 1 Note %d (0x%02X) vel=%d\n", note, note, vel);
                            fflush(stdout);
                        }
                    }
                } else if (type == 0x80) {
                    int note = d0;
                    if (note >= 84 && note <= 87) {
                        m_listener->on_encoder_touch(note - 84, false);
                    } else if (note == 78) {
                        m_listener->on_touch_strip_touch(false);
                    } else {
                        HardwareButton btn = get_akai_button(note);
                        if (btn != HW_BTN_NONE) {
                            printf("[AKAI-BTN] note=%d vel=%d\n", note, d1);
                            fflush(stdout);
                            m_listener->on_button_release(btn);
                        }
                    }
                } else if (type == 0xB0) {
                    int cc_num = d0, cc_val = d1;
                    if (cc_num >= 16 && cc_num <= 19) {
                        // Q-Link 1..4 relative rotary
                        int delta = (cc_val < 64) ? cc_val : (cc_val - 128);
                        m_listener->on_encoder_turn(cc_num - 16, delta);
                    } else if (cc_num == 100) {
                        // Data Dial relative rotary (CC 100 on MPC Key 61!)
                        int delta = (cc_val < 64) ? cc_val : (cc_val - 128);
                        m_listener->on_jog_dial(delta);
                    } else if (cc_num == 33) {
                        // Touch strip position MSB. The 0..127 path is unchanged; the 14-bit value (with the last
                        // CC34 LSB) is offered additionally for the pitch bend mode.
                        m_strip_msb = cc_val;
                        m_listener->on_touch_strip(cc_val);
                        m_listener->on_touch_strip_fine(kick_hw::strip::combine14(m_strip_msb, m_strip_lsb));
                    } else if (cc_num == 34) {
                        // Touch strip position LSB: only refines the 14-bit value, no 0..127 event.
                        m_strip_lsb = cc_val;
                        m_listener->on_touch_strip_fine(kick_hw::strip::combine14(m_strip_msb, m_strip_lsb));
                    } else if (cc_num == 69) {
                        m_listener->on_touch_strip(cc_val);
                    }
                }
            } else if (ch == 9) {
                // CH 10: 16 Performance Pads (Akai GM layout)
                int pad_idx = get_akai_pad_index(d0);
                if (pad_idx >= 0 && pad_idx < 16) {
                    if (type == 0x90) {
                        if (d1 > 0) m_listener->on_pad_note_on(pad_idx, d1);
                        else m_listener->on_pad_note_off(pad_idx);
                    } else if (type == 0x80) {
                        m_listener->on_pad_note_off(pad_idx);
                    } else if (type == 0xA0) {
                        // Polyphonic key pressure = pad aftertouch (d0 = note, d1 = pressure)
                        m_listener->on_pad_aftertouch(pad_idx, d1);
                    }
                }
            } else if (ch == 15) {
                // CH 16: 61 Keybed Keys, Wheels, and Rear Pedals (NEVER triggers buttons!)
                if (type == 0x90) {
                    if (d1 > 0) m_listener->on_keybed_note_on(d0, d1);
                    else m_listener->on_keybed_note_off(d0);
                } else if (type == 0x80) {
                    m_listener->on_keybed_note_off(d0);
                } else if (type == 0xE0) {
                    int lsb = d0, msb = d1;
                    m_listener->on_pitch_bend((msb << 7) | lsb);
                } else if (type == 0xD0) {
                    m_listener->on_channel_aftertouch(d0);
                } else if (type == 0xB0) {
                    int cc_num = d0, cc_val = d1;
                    if (cc_num == 64) {
                        m_listener->on_sustain_pedal(cc_val >= 64);
                    } else if (cc_num == 1) {
                        m_listener->on_mod_wheel(cc_val);
                    } else if (cc_num == 11) {
                        m_listener->on_expression(cc_val);
                    } else if (cc_num == 65) {
                        m_listener->on_footswitch2(cc_val >= 64);
                    }
                }
            }
        } else {
            // Port 0 (Keybed) or Port 2 (DIN MIDI In)
            if (ch == 9) {
                int pad_idx = get_akai_pad_index(d0);
                if (pad_idx < 0 && d0 >= 36 && d0 < 52) pad_idx = d0 - 36;
                if (pad_idx >= 0 && pad_idx < 16) {
                    if (type == 0x90) {
                        if (d1 > 0) m_listener->on_pad_note_on(pad_idx, d1);
                        else m_listener->on_pad_note_off(pad_idx);
                    } else if (type == 0x80) {
                        m_listener->on_pad_note_off(pad_idx);
                    }
                }
            } else {
                if (type == 0x90) {
                    if (d1 > 0) m_listener->on_keybed_note_on(d0, d1);
                    else m_listener->on_keybed_note_off(d0);
                } else if (type == 0x80) {
                    m_listener->on_keybed_note_off(d0);
                } else if (type == 0xE0) {
                    int lsb = d0, msb = d1;
                    m_listener->on_pitch_bend((msb << 7) | lsb);
                } else if (type == 0xB0) {
                    int cc_num = d0, cc_val = d1;
                    if (cc_num == 64) {
                        m_listener->on_sustain_pedal(cc_val >= 64);
                    }
                }
            }
        }
    }

    // Reader loop: poll() on the rawmidi input descriptors (20 ms timeout so stop() stays responsive),
    // then a non-blocking read of every port that signalled data.
    void run() {
        snd_rawmidi_t* const ins[3] = { m_in0, m_in1, m_in2 };
        struct pollfd pfds[3 * 4];
        int first[3] = { 0, 0, 0 };
        int cnt[3] = { 0, 0, 0 };
        int total = 0;
        for (int p = 0; p < 3; ++p) {
            if (!ins[p]) continue;
            int c = snd_rawmidi_poll_descriptors_count(ins[p]);
            if (c <= 0 || total + c > (int)(sizeof(pfds) / sizeof(pfds[0]))) continue;
            first[p] = total;
            cnt[p] = snd_rawmidi_poll_descriptors(ins[p], pfds + total, (unsigned)c);
            if (cnt[p] < 0) cnt[p] = 0;
            total += cnt[p];
        }
        uint8_t buf[256];
        while (m_running) {
            if (total == 0) { usleep(20000); continue; }
            int pr = poll(pfds, (nfds_t)total, 20);
            if (pr < 0) { if (errno != EINTR) usleep(20000); continue; }
            if (pr == 0) continue;
            bool error = false;
            for (int p = 0; p < 3; ++p) {
                if (!cnt[p]) continue;
                unsigned short revents = 0;
                snd_rawmidi_poll_descriptors_revents(ins[p], pfds + first[p], (unsigned)cnt[p], &revents);
                if (revents & (POLLERR | POLLHUP | POLLNVAL)) error = true;
                if (!(revents & POLLIN)) continue;
                ssize_t n = snd_rawmidi_read(ins[p], buf, sizeof(buf));
                if (n > 0) feed_port(p, buf, (size_t)n);
            }
            if (error) usleep(10000); // device vanished: do not spin
        }
    }

public:
    AkaiMpcKey61ControlSurfaceAdapter()
        : m_in0(nullptr), m_in1(nullptr), m_in2(nullptr),
          m_out0(nullptr), m_out1(nullptr), m_out2(nullptr),
          m_running(false), m_listener(nullptr) {}

    virtual ~AkaiMpcKey61ControlSurfaceAdapter() { stop(); }

    bool start(IHardwareEventListener* listener) override {
        m_listener = listener;

        if (m_running) return true; // already started

        struct PortSpec { snd_rawmidi_t** in; snd_rawmidi_t** out; const char* name; };
        // Discovery with safe fallback: card by name (M61 / MPC / Key 61), else card 0 as before.
        const int card = akai_find_card({ "M61", "MPC", "Key 61" }, 0);
        const std::string name0 = kick_hw::probe::rawmidiPortName(card, 0);
        const std::string name1 = kick_hw::probe::rawmidiPortName(card, 1);
        const std::string name2 = kick_hw::probe::rawmidiPortName(card, 2);
        const PortSpec ports[] = {
            { &m_in0, &m_out0, name0.c_str() },
            { &m_in1, &m_out1, name1.c_str() },
            { &m_in2, &m_out2, name2.c_str() },
        };
        for (const auto& p : ports) {
            *p.in = nullptr;
            *p.out = nullptr;
            int err = snd_rawmidi_open(p.in, p.out, p.name, SND_RAWMIDI_NONBLOCK);
            if (err < 0) {
                fprintf(stderr, "[AKAI-CONTROLLER] Cannot open %s: %s\n", p.name, snd_strerror(err));
                *p.in = nullptr;
                *p.out = nullptr;
            }
        }

        // Port 1 (MPC Private) carries buttons, pads and keybed. Without it the
        // control surface is useless: report failure instead of a silent "started".
        if (!m_in1 || !m_out1) {
            fprintf(stderr, "[AKAI-CONTROLLER] Port 1 (%s) unavailable -> control surface NOT started\n", name1.c_str());
            stop();
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(m_parser_mutex);
            for (auto& pr : m_parser) pr.reset();
        }

        {
            // T5: runtime switch to test on the device without rebuilding (KICK_AKAI_PORT0=0 -> never write Port 0).
            const char* e = getenv("KICK_AKAI_PORT0");
            m_port0_writes = !(e && strcmp(e, "0") == 0);
            printf("[AKAI-CONTROLLER] Port 0 writes (init SysEx, pad RGB): %s (KICK_AKAI_PORT0=%s)\n",
                   m_port0_writes ? "ENABLED (default)" : "DISABLED", e ? e : "<unset>");
            fflush(stdout);
        }

        // Mode 0x41: Port 1 Private enabled, Port 0 Public disabled, transport MMC disabled
        uint8_t akai_init[] = { 0xF0, 0x47, 0x7F, 0x4B, 0x60, 0x00, 0x01, 0x41, 0xF7 };
        // ARCHITECT-CLARIFY: the review says the mode SysEx should reach Port 1 only, but the
        // hardware-verified build also writes it to Port 0 (the host command port?). Keep both
        // until verified on a device; then drop the Port 0 write if it is not needed. Test with
        // KICK_AKAI_PORT0=0 (see m_port0_writes). The CLARIFY stays until the hardware confirms.
        if (m_out0 && m_port0_writes) locked_write(m_out0_mutex, m_out0, akai_init, sizeof(akai_init));
        locked_write(m_out1_mutex, m_out1, akai_init, sizeof(akai_init));
        m_led_cache.reset(); // after the mode set the device state is unknown: the next write of each LED goes out

        m_running = true;
        pthread_create(&m_thread, NULL, thread_entry, this);
        printf("[AKAI-CONTROLLER] RawMIDI (%s keybed, %s controls, %s DIN OUT) -> STARTED (Mode 0x41 Private)\n",
               name0.c_str(), name1.c_str(), name2.c_str());
        return true;
    }

    // Both go through the level based path: the last level per LED is cached, only changes reach ALSA.
    // Called from several threads (message thread, timer, MIDI reader): every snd_rawmidi_write goes through
    // locked_write() (one mutex per output handle).
    void set_led(HardwareLed led, bool on, uint32_t rgb = 0) override {
        (void)rgb;
        set_led_level(led, on ? kick_hw::kLedFullA : kick_hw::kLedOff);
    }

    void set_led_level(HardwareLed led, int level) override {
        if (!m_out1) return;
        bool twoColour = false;
        const int note = kick_hw::akaiLedNote(led, twoColour);
        if (note < 0) return;
        // Cache update and write under one lock so the cache never disagrees with what reached the device.
        std::lock_guard<std::mutex> lock(m_out1_mutex);
        if (!m_led_cache.update(led, level)) return;
        uint8_t msg[3];
        kick_hw::makeLedMessage(note, kick_hw::akaiLedValue(twoColour, level), msg);
        const ssize_t r = snd_rawmidi_write(m_out1, msg, 3);
        if (r < 0) log_write_error(m_out1, r);
        // Same value to the mirrored LED (right Shift), under the same lock.
        const int mirror = kick_hw::akaiLedMirrorNote(led);
        if (mirror >= 0) {
            kick_hw::makeLedMessage(mirror, kick_hw::akaiLedValue(twoColour, level), msg);
            const ssize_t r2 = snd_rawmidi_write(m_out1, msg, 3);
            if (r2 < 0) log_write_error(m_out1, r2);
        }
    }

    void set_pad_rgb(int pad, uint8_t r, uint8_t g, uint8_t b) override {
        if (pad < 0 || pad > 15) return;
        uint8_t p = (uint8_t)(pad & 0x0F);
        uint8_t red = (uint8_t)(r & 0x7F);
        uint8_t grn = (uint8_t)(g & 0x7F);
        uint8_t blu = (uint8_t)(b & 0x7F);

        // Akai MPC RGB SysEx: F0 47 [DevID] 4B 65 00 04 [pad] [R] [G] [B] F7
        uint8_t sysex_all[] = { 0xF0, 0x47, 0x7F, 0x4B, 0x65, 0x00, 0x04, p, red, grn, blu, 0xF7 };
        uint8_t sysex_dev[] = { 0xF0, 0x47, 0x00, 0x4B, 0x65, 0x00, 0x04, p, red, grn, blu, 0xF7 };

        if (m_out1) {
            locked_write(m_out1_mutex, m_out1, sysex_all, sizeof(sysex_all));
            locked_write(m_out1_mutex, m_out1, sysex_dev, sizeof(sysex_dev));
        }
        if (m_out0 && m_port0_writes) {
            locked_write(m_out0_mutex, m_out0, sysex_all, sizeof(sysex_all));
            locked_write(m_out0_mutex, m_out0, sysex_dev, sizeof(sysex_dev));
        }
    }

    // Hardware-free byte level test: runs 'data' through the same parser and dispatch as the reader thread
    // for 'port' (0 keybed, 1 control surface, 2 DIN). Thread-safe (shared parser guarded by m_parser_mutex).
    void inject_bytes(int port, const uint8_t* data, size_t len) override {
        if (!data) return;
        feed_port(port, data, len);
    }

    void stop() override {
        if (m_running) {
            m_running = false;
            pthread_join(m_thread, NULL);
        }
        if (m_in0) snd_rawmidi_close(m_in0);
        if (m_in1) snd_rawmidi_close(m_in1);
        if (m_in2) snd_rawmidi_close(m_in2);
        if (m_out0) snd_rawmidi_close(m_out0);
        if (m_out1) snd_rawmidi_close(m_out1);
        if (m_out2) snd_rawmidi_close(m_out2);
        m_in0 = m_in1 = m_in2 = nullptr;
        m_out0 = m_out1 = m_out2 = nullptr;
    }
};

class AkaiMpcKey61AudioAdapter : public IAudioAdapter {
private:
    snd_pcm_t* m_pcm;
    pthread_t m_thread;
    volatile bool m_running;
    AudioCallback m_callback;
    void* m_user_data;
    int m_sample_rate;
    int m_buffer_size;

    static void* thread_entry(void* arg) {
        ((AkaiMpcKey61AudioAdapter*)arg)->run();
        return NULL;
    }

    void run() {
        // ACVM 4-channel Quad (Main L/R + Headphones L/R)
        const int NUM_CH = 4;
        const int FRAMES_PER_BLOCK = 256;
        int32_t buf32[FRAMES_PER_BLOCK * NUM_CH];
        float out_l[FRAMES_PER_BLOCK];
        float out_r[FRAMES_PER_BLOCK];

        while (m_running) {
            if (m_callback) {
                m_callback(out_l, out_r, FRAMES_PER_BLOCK, m_user_data);
            } else {
                memset(out_l, 0, sizeof(out_l));
                memset(out_r, 0, sizeof(out_r));
            }

            for (int i = 0; i < FRAMES_PER_BLOCK; ++i) {
                float sl = fminf(0.99f, fmaxf(-0.99f, out_l[i]));
                float sr = fminf(0.99f, fmaxf(-0.99f, out_r[i]));
                int32_t s32_l = (int32_t)(sl * 2147483647.0f);
                int32_t s32_r = (int32_t)(sr * 2147483647.0f);

                buf32[i * 4 + 0] = s32_l; // Main L
                buf32[i * 4 + 1] = s32_r; // Main R
                buf32[i * 4 + 2] = s32_l; // Headphone L
                buf32[i * 4 + 3] = s32_r; // Headphone R
            }

            snd_pcm_sframes_t written = snd_pcm_writei(m_pcm, buf32, FRAMES_PER_BLOCK);
            if (written < 0) {
                snd_pcm_prepare(m_pcm);
            }
        }
    }

public:
    AkaiMpcKey61AudioAdapter()
        : m_pcm(nullptr), m_running(false), m_callback(nullptr), m_user_data(nullptr),
          m_sample_rate(44100), m_buffer_size(256) {}

    virtual ~AkaiMpcKey61AudioAdapter() { stop(); }

    bool start(AudioCallback cb, void* user_data, int sample_rate, int buffer_size) override {
        m_callback = cb;
        m_user_data = user_data;
        m_sample_rate = sample_rate;
        m_buffer_size = buffer_size;

        int err = snd_pcm_open(&m_pcm,
            kick_hw::probe::plughwName(akai_find_card({ "ACVM" }, 1)).c_str(), SND_PCM_STREAM_PLAYBACK, 0);
        if (err < 0) {
            // Fallback to default
            err = snd_pcm_open(&m_pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
            if (err < 0) {
                fprintf(stderr, "[AKAI-AUDIO] Cannot open PCM: %s\n", snd_strerror(err));
                return false;
            }
        }

        snd_pcm_set_params(m_pcm, SND_PCM_FORMAT_S32_LE, SND_PCM_ACCESS_RW_INTERLEAVED,
                           4, m_sample_rate, 1, 30000);

        m_running = true;
        pthread_create(&m_thread, NULL, thread_entry, this);
        printf("[AKAI-AUDIO] ACVM 4-Channel Quad (Main + Headphones) -> READY (5.8ms period)\n");
        return true;
    }

    void stop() override {
        if (m_running) {
            m_running = false;
            pthread_join(m_thread, NULL);
        }
        if (m_pcm) {
            snd_pcm_drain(m_pcm);
            snd_pcm_close(m_pcm);
            m_pcm = nullptr;
        }
    }
};

class AkaiMpcKey61Adapter : public IHardwareAdapter {
private:
    AkaiMpcKey61DisplayAdapter m_display;
    AkaiMpcKey61TouchAdapter m_touch;
    AkaiMpcKey61ControlSurfaceAdapter m_controls;
    AkaiMpcKey61AudioAdapter m_audio;

public:
    AkaiMpcKey61Adapter() {}
    virtual ~AkaiMpcKey61Adapter() { shutdown(); }

    const char* get_device_name() const override { return "AKAI MPC KEY 61"; }
    const char* get_device_badge() const override { return "MPC KEY 61"; }

    IDisplayAdapter* display() override { return &m_display; }
    ITouchAdapter* touch() override { return &m_touch; }
    IControlSurfaceAdapter* control_surface() override { return &m_controls; }
    IAudioAdapter* audio() override { return &m_audio; }

    bool init() override {
        return m_display.init(1280, 800);
    }

    void shutdown() override {
        m_touch.stop();
        m_controls.stop();
        m_audio.stop();
        m_display.shutdown();
    }
};
