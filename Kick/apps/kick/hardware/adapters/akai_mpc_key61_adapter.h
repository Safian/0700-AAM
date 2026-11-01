#pragma once

#include "../hardware_adapter.h"
#include "../midi_parser.h"
#include "../led_map.h"
#include <alsa/asoundlib.h>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <pthread.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

                        // Inverted rotation matrix for ILI2117 (0..2048):
                        int rx = slot->raw_x, ry = slot->raw_y;
                        if (rx < 0) rx = 0;
                        if (rx > 2048) rx = 2048;
                        if (ry < 0) ry = 0;
                        if (ry > 2048) ry = 2048;

                        int ui_x = ((2048 - ry) * (m_logical_w - 1)) / 2048;
                        int ui_y = (rx * (m_logical_h - 1)) / 2048;

                        if (!slot->was_down) {
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
          m_logical_w(1280), m_logical_h(800) {
        memset(m_slots, 0, sizeof(m_slots));
    }

    virtual ~AkaiMpcKey61TouchAdapter() { stop(); }

    bool start(IHardwareEventListener* listener, int logical_w, int logical_h) override {
        m_listener = listener;
        m_logical_w = logical_w;
        m_logical_h = logical_h;

        m_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
        if (m_fd < 0) {
            perror("[AKAI-TOUCH] Cannot open /dev/input/event0");
            return false;
        }

        m_running = true;
        pthread_create(&m_thread, NULL, thread_entry, this);
        printf("[AKAI-TOUCH] Connected to ILI2117 (/dev/input/event0) -> Transform to %dx%d UI active\n",
               m_logical_w, m_logical_h);
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
    kick_hw::LedLevelCache m_led_cache; // last level per button LED (only changes are written)
    pthread_t m_thread;
    volatile bool m_running;
    IHardwareEventListener* m_listener;

    static void* thread_entry(void* arg) {
        ((AkaiMpcKey61ControlSurfaceAdapter*)arg)->run();
        return NULL;
    }

    MidiParser m_parser0;
    MidiParser m_parser1;
    MidiParser m_parser2;

    static inline int get_akai_pad_index(int note) {
        switch (note) {
            case 37: return 0;  // Pad 1  (A01)
            case 36: return 1;  // Pad 2  (A02)
            case 42: return 2;  // Pad 3  (A03)
            case 82: return 3;  // Pad 4  (A04)
            case 40: return 4;  // Pad 5  (A05)
            case 38: return 5;  // Pad 6  (A06)
            case 46: return 6;  // Pad 7  (A07)
            case 44: return 7;  // Pad 8  (A08)
            case 48: return 8;  // Pad 9  (A09)
            case 47: return 9;  // Pad 10 (A10)
            case 45: return 10; // Pad 11 (A11)
            case 43: return 11; // Pad 12 (A12)
            case 49: return 12; // Pad 13 (A13)
            case 55: return 13; // Pad 14 (A14)
            case 51: return 14; // Pad 15 (A15)
            case 53: return 15; // Pad 16 (A16)
            default: return -1;
        }
    }

    static inline HardwareButton get_akai_button(int note) {
        switch (note) {
            // Bal szekcio (Tempo / Metro / Arp / Octave)
            case 53:  return HW_BTN_TAP_TEMPO;
            case 112: return HW_BTN_METRONOME;
            case 118: return HW_BTN_ARP;
            case 119: return HW_BTN_LATCH;
            case 7:   return HW_BTN_OCTAVE_DOWN;
            case 10:  return HW_BTN_OCTAVE_UP;

            // Bal-kozepso szekcio (Bank A-D + Touch Strip)
            case 35:  return HW_BTN_BANK_A;
            case 36:  return HW_BTN_BANK_B;
            case 37:  return HW_BTN_BANK_C;
            case 38:  return HW_BTN_BANK_D;
            case 4:   return HW_BTN_TOUCH_STRIP;
            case 42:  return HW_BTN_TOUCH_FX;
            case 11:  return HW_BTN_NOTE_REPEAT;

            // Kozepso 2x4 oszlop
            case 39:  return HW_BTN_FULL_LEVEL;
            case 40:  return HW_BTN_16_LEVELS;
            case 9:   return HW_BTN_ERASE;
            case 122: return HW_BTN_COPY;
            case 50:  return HW_BTN_SHIFT; // Left Shift
            case 123: return HW_BTN_MENU;
            case 121: return HW_BTN_SOUNDS;
            case 52:  return HW_BTN_MAIN;

            // Jobb felso (Q-Link gomb + Data Dial + +/-)
            case 0:   return HW_BTN_QLINK;
            case 111: return HW_BTN_ENTER; // Data Dial Push
            case 55:  return HW_BTN_MINUS;
            case 54:  return HW_BTN_PLUS;

            // 2x5 Mod matrix
            case 2:   return HW_BTN_PROG_EDIT;
            case 117: return HW_BTN_GRID;
            case 116: return HW_BTN_TRACK_MIX;
            case 43:  return HW_BTN_TRACK_MUTE;
            case 75:  return HW_BTN_AUTOMATION;
            case 120: return HW_BTN_KEYBOARD_CONTROL;
            case 6:   return HW_BTN_SAMPLE_EDIT;
            case 12:  return HW_BTN_TC;
            case 67:  return HW_BTN_UNDO;
            case 49:  return HW_BTN_SHIFT; // Right Shift

            // Transzport es Locate
            case 68:  return HW_BTN_STEP_PREV; // <
            case 69:  return HW_BTN_STEP_NEXT; // >
            case 70:  return HW_BTN_LOCATE;
            case 71:  return HW_BTN_BAR_PREV;  // <<
            case 72:  return HW_BTN_BAR_NEXT;  // >>
            case 73:  return HW_BTN_REC;
            case 80:  return HW_BTN_OVERDUB;
            case 81:  return HW_BTN_STOP;
            case 82:  return HW_BTN_PLAY;
            case 83:  return HW_BTN_PLAY_START;

            default:  return HW_BTN_NONE;
        }
    }

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
                        // Touch strip finger touch
                    } else {
                        HardwareButton btn = get_akai_button(note);
                        if (btn != HW_BTN_NONE) {
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
                    } else {
                        HardwareButton btn = get_akai_button(note);
                        if (btn != HW_BTN_NONE) {
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
                    } else if (cc_num == 33 || cc_num == 69) {
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

    void run() {
        uint8_t buf[256];
        while (m_running) {
            if (m_in0) {
                ssize_t n = snd_rawmidi_read(m_in0, buf, sizeof(buf));
                if (n > 0) {
                    m_parser0.feed(buf, n, [this](uint8_t s, uint8_t d0, uint8_t d1) {
                        handle_midi_message(0, s, d0, d1);
                    });
                }
            }
            if (m_in1) {
                ssize_t n = snd_rawmidi_read(m_in1, buf, sizeof(buf));
                if (n > 0) {
                    m_parser1.feed(buf, n, [this](uint8_t s, uint8_t d0, uint8_t d1) {
                        handle_midi_message(1, s, d0, d1);
                    });
                }
            }
            if (m_in2) {
                ssize_t n = snd_rawmidi_read(m_in2, buf, sizeof(buf));
                if (n > 0) {
                    m_parser2.feed(buf, n, [this](uint8_t s, uint8_t d0, uint8_t d1) {
                        handle_midi_message(2, s, d0, d1);
                    });
                }
            }
            usleep(1000);
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
        const PortSpec ports[] = {
            { &m_in0, &m_out0, "hw:0,0,0" },
            { &m_in1, &m_out1, "hw:0,0,1" },
            { &m_in2, &m_out2, "hw:0,0,2" },
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
            fprintf(stderr, "[AKAI-CONTROLLER] Port 1 (hw:0,0,1) unavailable -> control surface NOT started\n");
            stop();
            return false;
        }

        m_parser0.reset();
        m_parser1.reset();
        m_parser2.reset();

        // Mode 0x41: Port 1 Private enabled, Port 0 Public disabled, transport MMC disabled
        uint8_t akai_init[] = { 0xF0, 0x47, 0x7F, 0x4B, 0x60, 0x00, 0x01, 0x41, 0xF7 };
        // ARCHITECT-CLARIFY: the review says the mode SysEx should reach Port 1 only, but the
        // hardware-verified build also writes it to Port 0 (the host command port?). Keep both
        // until verified on a device; then drop the Port 0 write if it is not needed.
        if (m_out0) snd_rawmidi_write(m_out0, akai_init, sizeof(akai_init));
        snd_rawmidi_write(m_out1, akai_init, sizeof(akai_init));
        m_led_cache.reset(); // after the mode set the device state is unknown: the next write of each LED goes out

        m_running = true;
        pthread_create(&m_thread, NULL, thread_entry, this);
        printf("[AKAI-CONTROLLER] RawMIDI (hw:0,0,0 keybed, hw:0,0,1 controls, hw:0,0,2 DIN OUT) -> STARTED (Mode 0x41 Private)\n");
        return true;
    }

    // Both go through the level based path: the last level per LED is cached, only changes reach ALSA.
    // Called from several threads (message thread, timer, MIDI reader); the 3-byte snd_rawmidi_write is not
    // locked (see T16) - kept cheap, not made thread-safe here.
    void set_led(HardwareLed led, bool on, uint32_t rgb = 0) override {
        (void)rgb;
        set_led_level(led, on ? kick_hw::kLedFullA : kick_hw::kLedOff);
    }

    void set_led_level(HardwareLed led, int level) override {
        if (!m_out1) return;
        bool twoColour = false;
        const int note = kick_hw::akaiLedNote(led, twoColour);
        if (note < 0) return;
        if (!m_led_cache.update(led, level)) return;
        uint8_t msg[3] = { (uint8_t)0x90, (uint8_t)note, kick_hw::akaiLedValue(twoColour, level) };
        snd_rawmidi_write(m_out1, msg, 3);
    }

    void set_touch_strip_leds(int val) override {
        if (!m_out1) return;
        // CC33 (0x21) feedback to physical LED ladder (0..127)
        uint8_t msg[3] = { 0xB0, 0x21, (uint8_t)(val & 0x7F) };
        snd_rawmidi_write(m_out1, msg, 3);
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
            snd_rawmidi_write(m_out1, sysex_all, sizeof(sysex_all));
            snd_rawmidi_write(m_out1, sysex_dev, sizeof(sysex_dev));
        }
        if (m_out0) {
            snd_rawmidi_write(m_out0, sysex_all, sizeof(sysex_all));
            snd_rawmidi_write(m_out0, sysex_dev, sizeof(sysex_dev));
        }
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

        int err = snd_pcm_open(&m_pcm, "plughw:1,0", SND_PCM_STREAM_PLAYBACK, 0);
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
