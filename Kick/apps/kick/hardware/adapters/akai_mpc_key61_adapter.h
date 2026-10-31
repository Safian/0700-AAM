#pragma once

#include "../hardware_adapter.h"
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
            if (poll(&pfd, 1, 50) <= 0) continue;
            ssize_t n = read(m_fd, &ev, sizeof(ev));
            if (n != sizeof(ev)) continue;

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
                        slot->was_down = false;
                        slot->has_x = false;
                        slot->has_y = false;
                    }
                } else if (ev.code == ABS_MT_POSITION_X || ev.code == ABS_X) {
                    m_slots[m_current_slot].raw_x = ev.value;
                    m_slots[m_current_slot].has_x = true;
                } else if (ev.code == ABS_MT_POSITION_Y || ev.code == ABS_Y) {
                    m_slots[m_current_slot].raw_y = ev.value;
                    m_slots[m_current_slot].has_y = true;
                }
            } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
                if (ev.value == 0) {
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
                    if (!slot->active || !slot->has_x || !slot->has_y) continue;

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
    pthread_t m_thread;
    volatile bool m_running;
    IHardwareEventListener* m_listener;

    static void* thread_entry(void* arg) {
        ((AkaiMpcKey61ControlSurfaceAdapter*)arg)->run();
        return NULL;
    }

    void handle_packet(const uint8_t* buf, ssize_t n, int port) {
        if (!m_listener) return;

        for (ssize_t i = 0; i < n; ++i) {
            uint8_t b = buf[i];
            if ((b & 0xF0) == 0x90 && (i + 2 < n)) {
                int ch = b & 0x0F;
                int note = buf[i+1], vel = buf[i+2];

                // 1. Q-Link capacitive touch tops (Port 1, ch 15, Notes 84..87)
                if (port == 1 && ch == 15 && (note >= 84 && note <= 87)) {
                    m_listener->on_encoder_touch(note - 84, vel > 0);
                }
                // 2. Hardware Octave buttons (Note 7 = Down, Note 10 = Up)
                else if (note == 7) {
                    if (vel > 0) m_listener->on_button_press(HW_BTN_OCTAVE_DOWN);
                    else m_listener->on_button_release(HW_BTN_OCTAVE_DOWN);
                } else if (note == 10) {
                    if (vel > 0) m_listener->on_button_press(HW_BTN_OCTAVE_UP);
                    else m_listener->on_button_release(HW_BTN_OCTAVE_UP);
                }
                // 3. Transport buttons
                else if (note == 81 || note == 117) {
                    if (vel > 0) m_listener->on_button_press(HW_BTN_STOP);
                    else m_listener->on_button_release(HW_BTN_STOP);
                } else if (note == 82 || note == 118) {
                    if (vel > 0) m_listener->on_button_press(HW_BTN_PLAY);
                    else m_listener->on_button_release(HW_BTN_PLAY);
                } else if (note == 83) {
                    if (vel > 0) m_listener->on_button_press(HW_BTN_PLAY_START);
                    else m_listener->on_button_release(HW_BTN_PLAY_START);
                } else if (note == 73 || note == 119) {
                    if (vel > 0) m_listener->on_button_press(HW_BTN_REC);
                    else m_listener->on_button_release(HW_BTN_REC);
                } else if (note == 80 || note == 116) {
                    if (vel > 0) m_listener->on_button_press(HW_BTN_OVERDUB);
                    else m_listener->on_button_release(HW_BTN_OVERDUB);
                }
                // 4. Tap Tempo
                else if ((note == 53 || note == 35) && (ch == 15 || ch == 0)) {
                    if (vel > 0) m_listener->on_button_press(HW_BTN_TAP_TEMPO);
                }
                // 5. Value +/- buttons
                else if (note == 54) {
                    if (vel > 0) m_listener->on_button_press(HW_BTN_PLUS);
                    else m_listener->on_button_release(HW_BTN_PLUS);
                } else if (note == 55) {
                    if (vel > 0) m_listener->on_button_press(HW_BTN_MINUS);
                    else m_listener->on_button_release(HW_BTN_MINUS);
                }
                // 6. Data Wheel Click (ENTER) & Cursors
                else if (note == 111 || note == 60) {
                    if (vel > 0) m_listener->on_button_press(HW_BTN_ENTER);
                    else m_listener->on_button_release(HW_BTN_ENTER);
                } else if (note == 56 || note == 65) {
                    if (vel > 0) m_listener->on_button_press(HW_BTN_CURSOR_UP);
                    else m_listener->on_button_release(HW_BTN_CURSOR_UP);
                } else if (note == 57 || note == 66) {
                    if (vel > 0) m_listener->on_button_press(HW_BTN_CURSOR_DOWN);
                    else m_listener->on_button_release(HW_BTN_CURSOR_DOWN);
                }
                // 7. Navigation & Modes
                else if (note == 67 || note == 20) {
                    if (vel > 0) m_listener->on_button_press(HW_BTN_UNDO);
                } else if (note == 9 || note == 21) {
                    if (vel > 0) m_listener->on_button_press(HW_BTN_ERASE);
                } else if (note == 39) {
                    if (vel > 0) m_listener->on_button_press(HW_BTN_FULL_LEVEL);
                } else if (note == 40 || note == 59) {
                    if (vel > 0) m_listener->on_button_press(HW_BTN_16_LEVELS);
                } else if (note == 11) {
                    if (vel > 0) m_listener->on_button_press(HW_BTN_NOTE_REPEAT);
                } else if (note == 49) {
                    if (vel > 0) m_listener->on_button_press(HW_BTN_SHIFT);
                    else m_listener->on_button_release(HW_BTN_SHIFT);
                } else if (note == 51) {
                    if (vel > 0) m_listener->on_button_press(HW_BTN_MAIN);
                } else if (note == 50) {
                    if (vel > 0) m_listener->on_button_press(HW_BTN_BROWSE);
                }
                // 8. Performance Keys & Pads
                else {
                    if (ch == 9) { // Drum Pads
                        int pad_idx = note - 36;
                        if (pad_idx >= 0 && pad_idx < 16) {
                            if (vel > 0) m_listener->on_pad_note_on(pad_idx, vel);
                            else m_listener->on_pad_note_off(pad_idx);
                        }
                    } else { // 61-Key Keybed
                        if (vel > 0) m_listener->on_keybed_note_on(note, vel);
                        else m_listener->on_keybed_note_off(note);
                    }
                }
                i += 2;
            } else if ((b & 0xF0) == 0x80 && (i + 2 < n)) {
                int ch = b & 0x0F;
                int note = buf[i+1];

                if (port == 1 && ch == 15 && (note >= 84 && note <= 87)) {
                    m_listener->on_encoder_touch(note - 84, false);
                } else if (note == 7) {
                    m_listener->on_button_release(HW_BTN_OCTAVE_DOWN);
                } else if (note == 10) {
                    m_listener->on_button_release(HW_BTN_OCTAVE_UP);
                } else if (note == 81 || note == 82 || note == 83 || note == 73 || note == 80 ||
                           note == 117 || note == 118 || note == 119 || note == 116 ||
                           note == 53 || note == 35 || note == 54 || note == 55 ||
                           note == 56 || note == 57 || note == 65 || note == 66 ||
                           note == 111 || note == 60 || note == 67 || note == 20 ||
                           note == 9 || note == 21 || note == 39 || note == 40 ||
                           note == 59 || note == 11 || note == 49 || note == 50 || note == 51) {
                    if (note == 49) m_listener->on_button_release(HW_BTN_SHIFT);
                } else {
                    if (ch == 9) {
                        int pad_idx = note - 36;
                        if (pad_idx >= 0 && pad_idx < 16) m_listener->on_pad_note_off(pad_idx);
                    } else {
                        m_listener->on_keybed_note_off(note);
                    }
                }
                i += 2;
            } else if ((b & 0xF0) == 0xB0 && (i + 2 < n)) {
                int cc_num = buf[i+1], cc_val = buf[i+2];
                if (cc_num >= 16 && cc_num <= 19) {
                    int k = cc_num - 16;
                    int delta = (cc_val < 64) ? cc_val : (cc_val - 128);
                    m_listener->on_encoder_turn(k, delta);
                } else if (cc_num == 60) {
                    int delta = (cc_val < 64) ? cc_val : (cc_val - 128);
                    m_listener->on_jog_dial(delta);
                } else if (cc_num == 64) {
                    m_listener->on_sustain_pedal(cc_val >= 64);
                } else if (cc_num == 69) {
                    m_listener->on_touch_strip(cc_val);
                } else if (cc_num == 117) {
                    if (cc_val > 0) m_listener->on_button_press(HW_BTN_STOP);
                } else if (cc_num == 118) {
                    if (cc_val > 0) m_listener->on_button_press(HW_BTN_PLAY);
                } else if (cc_num == 119) {
                    if (cc_val > 0) m_listener->on_button_press(HW_BTN_REC);
                } else if (cc_num == 116) {
                    if (cc_val > 0) m_listener->on_button_press(HW_BTN_OVERDUB);
                }
                i += 2;
            } else if ((b & 0xF0) == 0xE0 && (i + 2 < n)) {
                int lsb = buf[i+1], msb = buf[i+2];
                m_listener->on_pitch_bend((msb << 7) | lsb);
                i += 2;
            }
        }
    }

    void run() {
        uint8_t buf[256];
        while (m_running) {
            if (m_in0) {
                ssize_t n = snd_rawmidi_read(m_in0, buf, sizeof(buf));
                if (n > 0) {
                    if (m_out2) snd_rawmidi_write(m_out2, buf, n);
                    handle_packet(buf, n, 0);
                }
            }
            if (m_in1) {
                ssize_t n = snd_rawmidi_read(m_in1, buf, sizeof(buf));
                if (n > 0) {
                    if (m_out2) snd_rawmidi_write(m_out2, buf, n);
                    handle_packet(buf, n, 1);
                }
            }
            if (m_in2) {
                ssize_t n = snd_rawmidi_read(m_in2, buf, sizeof(buf));
                if (n > 0) {
                    handle_packet(buf, n, 2);
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

        snd_rawmidi_open(&m_in0, &m_out0, "hw:0,0,0", SND_RAWMIDI_NONBLOCK);
        snd_rawmidi_open(&m_in1, &m_out1, "hw:0,0,1", SND_RAWMIDI_NONBLOCK);
        snd_rawmidi_open(&m_in2, &m_out2, "hw:0,0,2", SND_RAWMIDI_NONBLOCK);

        uint8_t inquiry[] = { 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 };
        uint8_t akai_init[] = { 0xF0, 0x47, 0x7F, 0x4B, 0x60, 0x00, 0x04, 0x40, 0x00, 0x00, 0x00, 0xF7 };
        if (m_out0) {
            snd_rawmidi_write(m_out0, inquiry, sizeof(inquiry));
            snd_rawmidi_write(m_out0, akai_init, sizeof(akai_init));
        }
        if (m_out1) {
            snd_rawmidi_write(m_out1, inquiry, sizeof(inquiry));
            snd_rawmidi_write(m_out1, akai_init, sizeof(akai_init));
        }

        m_running = true;
        pthread_create(&m_thread, NULL, thread_entry, this);
        printf("[AKAI-CONTROLLER] RawMIDI (hw:0,0,0 keybed, hw:0,0,1 controls, hw:0,0,2 DIN OUT) -> STARTED\n");
        return true;
    }

    void set_led(HardwareLed led, bool on, uint32_t rgb = 0) override {
        (void)rgb;
        if (!m_out1) return;
        int note = -1;
        switch (led) {
            case HW_LED_PLAY: note = 82; break;
            case HW_LED_STOP: note = 81; break;
            case HW_LED_REC: note = 73; break;
            case HW_LED_OCTAVE_DOWN: note = 7; break;
            case HW_LED_OCTAVE_UP: note = 10; break;
            case HW_LED_FULL_LEVEL: note = 39; break;
            case HW_LED_16_LEVELS: note = 40; break;
            case HW_LED_NOTE_REPEAT: note = 11; break;
            default: break;
        }
        if (note != -1) {
            uint8_t msg[3] = { (uint8_t)0x90, (uint8_t)note, (uint8_t)(on ? 127 : 0) };
            snd_rawmidi_write(m_out1, msg, 3);
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
