/*
 * Kick MPC Key 61 - Full Standalone DAW GUI, Touchscreen & Sound Engine
 * Direct Framebuffer (800x1280 -> 1280x800 rotation), ILI2117 Touchscreen,
 * ACVM 4-Channel Audio, and ALSA RawMIDI (hw:0,0,0 / hw:0,0,1 / hw:0,0,2 DIN OUT)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <alsa/asoundlib.h>

#define SCREEN_W         1280
#define SCREEN_H         800
#define FB_PHYS_W        800
#define FB_PHYS_H        1280
#define SAMPLE_RATE      44100
#define NUM_CHANNELS     4
#define BUFFER_FRAMES    128
#define MAX_VOICES       32
#define PI_F             3.14159265358979323846f

// Color definitions (0x00RRGGBB)
#define COLOR_BG         0x00141619
#define COLOR_PANEL      0x001E2228
#define COLOR_PANEL_HI   0x00282E37
#define COLOR_BORDER     0x00323A46
#define COLOR_TEXT_DIM   0x00788290
#define COLOR_TEXT_NORM  0x00C0C8D2
#define COLOR_TEXT_BOLD  0x00FFFFFF
#define COLOR_ORANGE     0x00FF6600
#define COLOR_GOLD       0x00FFAA00
#define COLOR_GREEN      0x0022C55E
#define COLOR_RED        0x00EF4444
#define COLOR_CYAN       0x0006B6D4
#define COLOR_PAD_BASE   0x0023272F
#define COLOR_PAD_ACTIVE 0x00FF8800

typedef struct {
    int note;
    float velocity;
    float phase;
    float freq;
    float env;
    float decay_rate;
    float filter_state;
    bool active;
    bool releasing;
} Voice;

typedef struct {
    Voice voices[MAX_VOICES];
    pthread_mutex_t lock;
} SynthEngine;

static volatile bool g_running = true;
static SynthEngine g_synth;

// UI State
static bool g_pad_active[16];
static bool g_key_active[61]; // 61 keys (MIDI 36 to 96, C2 to C7)
static bool g_is_playing = false;
static bool g_is_recording = false;
static int g_knob_vals[4] = { 75, 40, 10, 85 };
static bool g_touch_active = false;
static int g_touch_x = -1;
static int g_touch_y = -1;
static pthread_mutex_t g_ui_lock = PTHREAD_MUTEX_INITIALIZER;

static uint32_t g_render_buf[SCREEN_W * SCREEN_H];
static uint32_t* g_fb_ptr = NULL;
static size_t g_fb_size = 0;

static void sig_handler(int sig) {
    (void)sig;
    g_running = false;
}

static inline float midi_to_freq(int note) {
    return 440.0f * powf(2.0f, ((float)note - 69.0f) / 12.0f);
}

static const char* note_name(int note) {
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static char buf[16];
    int octave = (note / 12) - 1;
    snprintf(buf, sizeof(buf), "%s%d", names[note % 12], octave);
    return buf;
}

static void synth_init(SynthEngine* s) {
    pthread_mutex_init(&s->lock, NULL);
    for (int i = 0; i < MAX_VOICES; ++i) s->voices[i].active = false;
}

static void synth_note_on(SynthEngine* s, int note, int vel, bool is_pad = false) {
    if (vel <= 0) return;
    pthread_mutex_lock(&s->lock);
    int target = -1;
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (!s->voices[i].active) { target = i; break; }
    }
    if (target == -1) target = 0;

    Voice* v = &s->voices[target];
    v->note = note;
    v->velocity = (float)vel / 127.0f;
    v->phase = 0.0f;
    v->freq = midi_to_freq(note);
    v->env = v->velocity;
    float decay_sec = 3.5f - ((float)(note - 21) / 88.0f * 2.5f);
    if (decay_sec < 0.5f) decay_sec = 0.5f;
    v->decay_rate = powf(0.001f, 1.0f / (decay_sec * (float)SAMPLE_RATE));
    v->filter_state = 0.0f;
    v->active = true;
    v->releasing = false;
    pthread_mutex_unlock(&s->lock);

    // Update UI Key State
    pthread_mutex_lock(&g_ui_lock);
    int key_idx = note - 36;
    if (key_idx >= 0 && key_idx < 61) g_key_active[key_idx] = true;
    if (is_pad) {
        int pad_idx = note - 36;
        if (pad_idx >= 0 && pad_idx < 16) g_pad_active[pad_idx] = true;
    }
    pthread_mutex_unlock(&g_ui_lock);
}

static void synth_note_off(SynthEngine* s, int note) {
    pthread_mutex_lock(&s->lock);
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (s->voices[i].active && s->voices[i].note == note) {
            s->voices[i].releasing = true;
        }
    }
    pthread_mutex_unlock(&s->lock);

    // Update UI Key State
    pthread_mutex_lock(&g_ui_lock);
    int key_idx = note - 36;
    if (key_idx >= 0 && key_idx < 61) g_key_active[key_idx] = false;
    int pad_idx = note - 36;
    if (pad_idx >= 0 && pad_idx < 16) g_pad_active[pad_idx] = false;
    pthread_mutex_unlock(&g_ui_lock);
}

static void synth_render(SynthEngine* s, int32_t* out, size_t nframes) {
    pthread_mutex_lock(&s->lock);
    const float rel_rate = powf(0.001f, 1.0f / (0.05f * (float)SAMPLE_RATE));

    for (size_t f = 0; f < nframes; ++f) {
        float mix_l = 0.0f, mix_r = 0.0f;
        for (int i = 0; i < MAX_VOICES; ++i) {
            Voice* v = &s->voices[i];
            if (!v->active) continue;

            float osc = sinf(v->phase) + 0.45f * sinf(v->phase * 2.002f) + 0.20f * sinf(v->phase * 3.004f);
            float cutoff = 0.10f + (v->velocity * 0.40f) * (v->env / (v->velocity + 0.001f));
            if (cutoff > 0.85f) cutoff = 0.85f;
            v->filter_state += cutoff * (osc - v->filter_state);

            float voice = v->filter_state * v->env;
            float pan = (float)(v->note - 21) / 88.0f;
            if (pan < 0.1f) pan = 0.1f; if (pan > 0.9f) pan = 0.9f;

            mix_l += voice * (1.0f - pan * 0.5f);
            mix_r += voice * (0.5f + pan * 0.5f);

            v->phase += 2.0f * PI_F * v->freq / (float)SAMPLE_RATE;
            if (v->phase >= 2.0f * PI_F) v->phase -= 2.0f * PI_F;

            if (v->releasing) {
                v->env *= rel_rate;
                if (v->env < 0.0001f) v->active = false;
            } else {
                v->env *= v->decay_rate;
                if (v->env < 0.0001f) v->active = false;
            }
        }
        if (mix_l > 1.0f) mix_l = 1.0f; else if (mix_l < -1.0f) mix_l = -1.0f;
        if (mix_r > 1.0f) mix_r = 1.0f; else if (mix_r < -1.0f) mix_r = -1.0f;

        int32_t sl = (int32_t)(mix_l * 1073741824.0f);
        int32_t sr = (int32_t)(mix_r * 1073741824.0f);

        out[f * NUM_CHANNELS + 0] = sl;
        out[f * NUM_CHANNELS + 1] = sr;
        out[f * NUM_CHANNELS + 2] = sl;
        out[f * NUM_CHANNELS + 3] = sr;
    }
    pthread_mutex_unlock(&s->lock);
}

static void* audio_thread_func(void* arg) {
    snd_pcm_t* pcm = (snd_pcm_t*)arg;
    int32_t buf[BUFFER_FRAMES * NUM_CHANNELS];
    while (g_running) {
        synth_render(&g_synth, buf, BUFFER_FRAMES);
        snd_pcm_sframes_t f = snd_pcm_writei(pcm, buf, BUFFER_FRAMES);
        if (f < 0) snd_pcm_recover(pcm, (int)f, 0);
    }
    return NULL;
}

// -------------------------------------------------------------
// 2D Software Renderer
// -------------------------------------------------------------
static inline void draw_pixel(int x, int y, uint32_t color) {
    if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) {
        g_render_buf[y * SCREEN_W + x] = color;
    }
}

static void fill_rect(int x0, int y0, int w, int h, uint32_t color) {
    int x1 = x0 + w;
    int y1 = y0 + h;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > SCREEN_W) x1 = SCREEN_W;
    if (y1 > SCREEN_H) y1 = SCREEN_H;

    for (int y = y0; y < y1; ++y) {
        uint32_t* row = &g_render_buf[y * SCREEN_W + x0];
        for (int x = 0; x < (x1 - x0); ++x) {
            row[x] = color;
        }
    }
}

static void draw_rect(int x0, int y0, int w, int h, uint32_t color) {
    fill_rect(x0, y0, w, 1, color);
    fill_rect(x0, y0 + h - 1, w, 1, color);
    fill_rect(x0, y0, 1, h, color);
    fill_rect(x0 + w - 1, y0, 1, h, color);
}

// 5x7 ASCII font table
static uint8_t g_font5x7[128][5];
static bool g_font_initialized = false;

static void init_font() {
    if (g_font_initialized) return;
    memset(g_font5x7, 0, sizeof(g_font5x7));

    auto set_glyph = [](char c, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4) {
        g_font5x7[(uint8_t)c][0] = b0;
        g_font5x7[(uint8_t)c][1] = b1;
        g_font5x7[(uint8_t)c][2] = b2;
        g_font5x7[(uint8_t)c][3] = b3;
        g_font5x7[(uint8_t)c][4] = b4;
    };

    set_glyph(' ', 0x00, 0x00, 0x00, 0x00, 0x00);
    set_glyph('!', 0x00, 0x00, 0x5F, 0x00, 0x00);
    set_glyph('-', 0x08, 0x08, 0x08, 0x08, 0x08);
    set_glyph('+', 0x08, 0x08, 0x3E, 0x08, 0x08);
    set_glyph(':', 0x00, 0x36, 0x36, 0x00, 0x00);
    set_glyph('.', 0x00, 0x60, 0x60, 0x00, 0x00);
    set_glyph('/', 0x20, 0x10, 0x08, 0x04, 0x02);
    set_glyph('|', 0x00, 0x00, 0x7F, 0x00, 0x00);
    set_glyph('[', 0x00, 0x7F, 0x41, 0x41, 0x00);
    set_glyph(']', 0x00, 0x41, 0x41, 0x7F, 0x00);
    set_glyph('%', 0x23, 0x13, 0x08, 0x64, 0x62);
    set_glyph('0', 0x3E, 0x51, 0x49, 0x45, 0x3E);
    set_glyph('1', 0x00, 0x42, 0x7F, 0x40, 0x00);
    set_glyph('2', 0x42, 0x61, 0x51, 0x49, 0x46);
    set_glyph('3', 0x21, 0x41, 0x45, 0x4B, 0x31);
    set_glyph('4', 0x18, 0x14, 0x12, 0x7F, 0x10);
    set_glyph('5', 0x27, 0x45, 0x45, 0x45, 0x39);
    set_glyph('6', 0x3C, 0x4A, 0x49, 0x49, 0x30);
    set_glyph('7', 0x01, 0x71, 0x09, 0x05, 0x03);
    set_glyph('8', 0x36, 0x49, 0x49, 0x49, 0x36);
    set_glyph('9', 0x06, 0x49, 0x49, 0x29, 0x1E);
    set_glyph('A', 0x7E, 0x11, 0x11, 0x11, 0x7E);
    set_glyph('B', 0x7F, 0x49, 0x49, 0x49, 0x36);
    set_glyph('C', 0x3E, 0x41, 0x41, 0x41, 0x22);
    set_glyph('D', 0x7F, 0x41, 0x41, 0x22, 0x1C);
    set_glyph('E', 0x7F, 0x49, 0x49, 0x49, 0x41);
    set_glyph('F', 0x7F, 0x09, 0x09, 0x09, 0x01);
    set_glyph('G', 0x3E, 0x41, 0x49, 0x49, 0x7A);
    set_glyph('H', 0x7F, 0x08, 0x08, 0x08, 0x7F);
    set_glyph('I', 0x00, 0x41, 0x7F, 0x41, 0x00);
    set_glyph('J', 0x20, 0x40, 0x41, 0x3F, 0x01);
    set_glyph('K', 0x7F, 0x08, 0x14, 0x22, 0x41);
    set_glyph('L', 0x7F, 0x40, 0x40, 0x40, 0x40);
    set_glyph('M', 0x7F, 0x02, 0x0C, 0x02, 0x7F);
    set_glyph('N', 0x7F, 0x04, 0x08, 0x10, 0x7F);
    set_glyph('O', 0x3E, 0x41, 0x41, 0x41, 0x3E);
    set_glyph('P', 0x7F, 0x09, 0x09, 0x09, 0x06);
    set_glyph('Q', 0x3E, 0x41, 0x51, 0x21, 0x5E);
    set_glyph('R', 0x7F, 0x09, 0x19, 0x29, 0x46);
    set_glyph('S', 0x46, 0x49, 0x49, 0x49, 0x31);
    set_glyph('T', 0x01, 0x01, 0x7F, 0x01, 0x01);
    set_glyph('U', 0x3F, 0x40, 0x40, 0x40, 0x3F);
    set_glyph('V', 0x1F, 0x20, 0x40, 0x20, 0x1F);
    set_glyph('W', 0x7F, 0x20, 0x18, 0x20, 0x7F);
    set_glyph('X', 0x63, 0x14, 0x08, 0x14, 0x63);
    set_glyph('Y', 0x07, 0x08, 0x70, 0x08, 0x07);
    set_glyph('Z', 0x61, 0x51, 0x49, 0x45, 0x43);
    set_glyph('a', 0x20, 0x54, 0x54, 0x54, 0x78);
    set_glyph('b', 0x7F, 0x48, 0x44, 0x44, 0x38);
    set_glyph('c', 0x38, 0x44, 0x44, 0x44, 0x20);
    set_glyph('d', 0x38, 0x44, 0x44, 0x48, 0x7F);
    set_glyph('e', 0x38, 0x54, 0x54, 0x54, 0x18);
    set_glyph('i', 0x00, 0x44, 0x7D, 0x40, 0x00);
    set_glyph('k', 0x7F, 0x10, 0x28, 0x44, 0x00);
    set_glyph('l', 0x00, 0x41, 0x7F, 0x40, 0x00);
    set_glyph('m', 0x7C, 0x04, 0x18, 0x04, 0x78);
    set_glyph('n', 0x7C, 0x08, 0x04, 0x04, 0x78);
    set_glyph('o', 0x38, 0x44, 0x44, 0x44, 0x38);
    set_glyph('p', 0x7C, 0x14, 0x14, 0x14, 0x08);
    set_glyph('r', 0x7C, 0x08, 0x04, 0x04, 0x08);
    set_glyph('s', 0x48, 0x54, 0x54, 0x54, 0x20);
    set_glyph('t', 0x04, 0x3F, 0x44, 0x40, 0x20);
    set_glyph('u', 0x3C, 0x40, 0x40, 0x20, 0x7C);
    set_glyph('y', 0x0C, 0x50, 0x50, 0x50, 0x3C);

    g_font_initialized = true;
}

static void draw_char(int x, int y, char c, uint32_t color, int scale) {
    uint8_t uc = (uint8_t)c;
    if (uc > 127) uc = ' ';
    const uint8_t* glyph = g_font5x7[uc];

    for (int col = 0; col < 5; ++col) {
        uint8_t line = glyph[col];
        for (int row = 0; row < 7; ++row) {
            if (line & (1 << row)) {
                fill_rect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

static void draw_string(int x, int y, const char* str, uint32_t color, int scale) {
    while (*str) {
        draw_char(x, y, *str, color, scale);
        x += (5 + 1) * scale;
        str++;
    }
}

// -------------------------------------------------------------
// UI Scene Render
// -------------------------------------------------------------
static void render_ui() {
    // 1. Background
    fill_rect(0, 0, SCREEN_W, SCREEN_H, COLOR_BG);

    // 2. Top Header Bar (Height = 60)
    fill_rect(0, 0, SCREEN_W, 60, COLOR_PANEL);
    fill_rect(0, 59, SCREEN_W, 1, COLOR_BORDER);

    draw_string(24, 20, "KICK DAW", COLOR_ORANGE, 3);
    draw_string(210, 24, "AKAI MPC KEY 61 STANDALONE", COLOR_TEXT_BOLD, 2);

    // BPM & Latency badge
    fill_rect(720, 14, 110, 32, COLOR_PANEL_HI);
    draw_rect(720, 14, 110, 32, COLOR_BORDER);
    draw_string(735, 22, "120.0 BPM", COLOR_GOLD, 2);

    fill_rect(845, 14, 190, 32, COLOR_PANEL_HI);
    draw_rect(845, 14, 190, 32, COLOR_BORDER);
    draw_string(855, 22, "ACVM 4-CH 2.9ms", COLOR_GREEN, 2);

    // Transport buttons
    uint32_t play_bg = g_is_playing ? 0x001B4D2E : COLOR_PANEL_HI;
    uint32_t play_fg = g_is_playing ? 0x0055FF55 : COLOR_GREEN;
    fill_rect(1060, 14, 60, 32, play_bg);
    draw_rect(1060, 14, 60, 32, g_is_playing ? COLOR_GREEN : COLOR_BORDER);
    draw_string(1075, 22, "PLAY", play_fg, 2);

    uint32_t stop_bg = (!g_is_playing && !g_is_recording) ? 0x003A404D : COLOR_PANEL_HI;
    fill_rect(1130, 14, 60, 32, stop_bg);
    draw_rect(1130, 14, 60, 32, (!g_is_playing && !g_is_recording) ? COLOR_CYAN : COLOR_BORDER);
    draw_string(1145, 22, "STOP", (!g_is_playing && !g_is_recording) ? COLOR_TEXT_BOLD : COLOR_TEXT_NORM, 2);

    uint32_t rec_bg = g_is_recording ? 0x005E1A1A : COLOR_PANEL_HI;
    fill_rect(1200, 14, 60, 32, rec_bg);
    draw_rect(1200, 14, 60, 32, g_is_recording ? COLOR_RED : COLOR_BORDER);
    draw_string(1218, 22, "REC", g_is_recording ? 0x00FF4444 : COLOR_RED, 2);

    // 3. Left Inspector Panel (Width = 420, Height = 480)
    fill_rect(24, 80, 420, 480, COLOR_PANEL);
    draw_rect(24, 80, 420, 480, COLOR_BORDER);

    draw_string(44, 100, "TRACK 1: ACOUSTIC GRAND", COLOR_TEXT_BOLD, 2);
    draw_string(44, 130, "ENGINE: CLAP / KEYGROUP SAMPLER", COLOR_TEXT_DIM, 2);
    draw_string(44, 155, "MIDI IN: KEYBED 61 + PADS  |  OUT: DIN A", COLOR_CYAN, 1);

    // 4 Q-Link Knobs Simulation
    const char* knob_labels[4] = { "CUTOFF", "RESONANCE", "ATTACK", "DECAY" };
    for (int k = 0; k < 4; ++k) {
        int kx = 50 + (k % 2) * 190;
        int ky = 200 + (k / 2) * 130;
        fill_rect(kx, ky, 160, 100, COLOR_PANEL_HI);
        draw_rect(kx, ky, 160, 100, COLOR_BORDER);
        draw_string(kx + 15, ky + 15, knob_labels[k], COLOR_TEXT_NORM, 1);

        // Knob circle
        int cx = kx + 80, cy = ky + 60, r = 24;
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (dx*dx + dy*dy <= r*r) {
                    draw_pixel(cx + dx, cy + dy, (dx*dx + dy*dy <= (r-4)*(r-4)) ? COLOR_PANEL : COLOR_ORANGE);
                }
            }
        }
        // Value text
        char val_str[16];
        snprintf(val_str, sizeof(val_str), "%d%%", g_knob_vals[k]);
        draw_string(kx + 115, ky + 55, val_str, COLOR_GOLD, 1);
    }

    // 4. Right 16 Pads Panel (4x4 Matrix)
    fill_rect(470, 80, 786, 480, COLOR_PANEL);
    draw_rect(470, 80, 786, 480, COLOR_BORDER);
    draw_string(490, 100, "16 MPC DRUM & PERFORMANCE PADS", COLOR_TEXT_BOLD, 2);

    pthread_mutex_lock(&g_ui_lock);
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            int pad_num = (3 - r) * 4 + c; // MPC layout: Pad 1 bottom-left, Pad 16 top-right
            int px = 500 + c * 180;
            int py = 135 + r * 100;
            int pw = 165;
            int ph = 88;

            bool active = g_pad_active[pad_num];
            uint32_t pad_color = active ? COLOR_PAD_ACTIVE : COLOR_PAD_BASE;
            uint32_t border_col = active ? COLOR_GOLD : 0x00454D5A;

            fill_rect(px, py, pw, ph, pad_color);
            draw_rect(px, py, pw, ph, border_col);

            // Pad label
            char pad_lbl[16];
            snprintf(pad_lbl, sizeof(pad_lbl), "PAD %02d", pad_num + 1);
            draw_string(px + 12, py + 12, pad_lbl, active ? COLOR_TEXT_BOLD : COLOR_TEXT_NORM, 1);

            int midi_note = 36 + pad_num;
            draw_string(px + 12, py + 55, note_name(midi_note), active ? COLOR_TEXT_BOLD : COLOR_GOLD, 2);
        }
    }
    pthread_mutex_unlock(&g_ui_lock);

    // 5. Bottom 61-Key Keyboard Piano (Height = 200, Y = 580 to 780)
    fill_rect(24, 580, 1232, 200, COLOR_PANEL);
    draw_rect(24, 580, 1232, 200, COLOR_BORDER);

    // Count white keys in 61 keys (from C2 = 36 to C7 = 96)
    // 61 keys has exactly 36 white keys and 25 black keys
    int num_white = 36;
    float kw = (float)(1232 - 16) / (float)num_white; // approx 33.7 pixels per white key

    // First pass: Draw white keys
    int white_idx = 0;
    pthread_mutex_lock(&g_ui_lock);
    for (int i = 0; i < 61; ++i) {
        int note = 36 + i;
        int note_in_octave = note % 12;
        bool is_black = (note_in_octave == 1 || note_in_octave == 3 || note_in_octave == 6 ||
                         note_in_octave == 8 || note_in_octave == 10);
        if (!is_black) {
            int kx = 32 + (int)(white_idx * kw);
            int ky = 590;
            int w = (int)kw - 2;
            int h = 180;

            bool active = g_key_active[i];
            uint32_t col = active ? COLOR_GOLD : 0x00EEEEEE;
            fill_rect(kx, ky, w, h, col);
            draw_rect(kx, ky, w, h, 0x00999999);

            if (note_in_octave == 0) { // Mark C notes
                draw_string(kx + 6, ky + h - 25, note_name(note), 0x00333333, 1);
            }
            white_idx++;
        }
    }

    // Second pass: Draw black keys on top
    white_idx = 0;
    for (int i = 0; i < 61; ++i) {
        int note = 36 + i;
        int note_in_octave = note % 12;
        bool is_black = (note_in_octave == 1 || note_in_octave == 3 || note_in_octave == 6 ||
                         note_in_octave == 8 || note_in_octave == 10);
        if (!is_black) {
            white_idx++;
        } else {
            // Position black key between white_idx-1 and white_idx
            int kx = 32 + (int)((white_idx - 1) * kw) + (int)(kw * 0.65f);
            int ky = 590;
            int w = (int)(kw * 0.70f);
            int h = 110;

            bool active = g_key_active[i];
            uint32_t col = active ? COLOR_ORANGE : 0x00151515;
            fill_rect(kx, ky, w, h, col);
            draw_rect(kx, ky, w, h, 0x00000000);
        }
    }
    // 6. Touch Visual Feedback Indicator
    if (g_touch_active && g_touch_x >= 0 && g_touch_x < SCREEN_W && g_touch_y >= 0 && g_touch_y < SCREEN_H) {
        int tr = 14;
        for (int dy = -tr; dy <= tr; ++dy) {
            for (int dx = -tr; dx <= tr; ++dx) {
                int dist2 = dx*dx + dy*dy;
                if (dist2 <= tr*tr && dist2 >= (tr-3)*(tr-3)) {
                    draw_pixel(g_touch_x + dx, g_touch_y + dy, COLOR_CYAN);
                }
            }
        }
        for (int dy = -2; dy <= 2; ++dy) {
            for (int dx = -2; dx <= 2; ++dx) {
                draw_pixel(g_touch_x + dx, g_touch_y + dy, COLOR_GOLD);
            }
        }
    }
    pthread_mutex_unlock(&g_ui_lock);
}

// -------------------------------------------------------------
// Framebuffer Blit with 90-degree panel rotation
// Physical screen: 800 (W) x 1280 (H)
// Landscape UI:   1280 (W) x 800 (H)
// Mapping: x_phys = y_ui,  y_phys = (SCREEN_W - 1) - x_ui
// -------------------------------------------------------------
static void blit_to_framebuffer() {
    if (!g_fb_ptr) return;

    for (int y_ui = 0; y_ui < SCREEN_H; ++y_ui) {
        for (int x_ui = 0; x_ui < SCREEN_W; ++x_ui) {
            uint32_t pixel = g_render_buf[y_ui * SCREEN_W + x_ui];
            int x_phys = y_ui;
            int y_phys = (SCREEN_W - 1) - x_ui;

            g_fb_ptr[y_phys * FB_PHYS_W + x_phys] = pixel;
        }
    }
}

// Save Screenshot as PPM
static void save_screenshot(const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
    for (int y = 0; y < SCREEN_H; ++y) {
        for (int x = 0; x < SCREEN_W; ++x) {
            uint32_t c = g_render_buf[y * SCREEN_W + x];
            uint8_t rgb[3] = {
                (uint8_t)((c >> 16) & 0xFF),
                (uint8_t)((c >> 8) & 0xFF),
                (uint8_t)(c & 0xFF)
            };
            fwrite(rgb, 1, 3, f);
        }
    }
    fclose(f);
    printf("[SCREENSHOT] Saved UI snapshot to: %s\n", path);
}

// -------------------------------------------------------------
// Touchscreen Coordinate & Hit-Test Helpers
// Physical screen: 800x1280 (portrait). Rotated to 1280x800 landscape.
// ILI2117 digitizer range: 0..2048 x 0..2048.
// -------------------------------------------------------------
static inline void raw_touch_to_ui(int rx, int ry, int* out_x, int* out_y) {
    if (rx < 0) rx = 0;
    if (rx > 2048) rx = 2048;
    if (ry < 0) ry = 0;
    if (ry > 2048) ry = 2048;

    // Mathematical inversion for 90-degree panel rotation:
    // x_phys = y_ui       => y_ui = x_phys
    // y_phys = 1279 - x_ui => x_ui = 1279 - y_phys
    *out_x = ((2048 - ry) * (SCREEN_W - 1)) / 2048;
    *out_y = (rx * (SCREEN_H - 1)) / 2048;
}

static int get_keyboard_note_at(int x, int y) {
    if (x < 32 || x >= 1248 || y < 590 || y >= 770) return -1;

    const int num_white = 36;
    const float kw = (float)(1232 - 16) / (float)num_white;

    // Check black keys first if touching upper part of keybed (y < 700)
    if (y < 700) {
        int white_idx = 0;
        for (int i = 0; i < 61; ++i) {
            int note = 36 + i;
            int note_in_oct = note % 12;
            bool is_black = (note_in_oct == 1 || note_in_oct == 3 || note_in_oct == 6 ||
                             note_in_oct == 8 || note_in_oct == 10);
            if (!is_black) {
                white_idx++;
            } else {
                int kx = 32 + (int)((white_idx - 1) * kw) + (int)(kw * 0.65f);
                int w = (int)(kw * 0.70f);
                if (x >= kx && x < kx + w) {
                    return note;
                }
            }
        }
    }

    // Otherwise white key
    int white_idx = (int)((x - 32) / kw);
    if (white_idx < 0) white_idx = 0;
    if (white_idx >= num_white) white_idx = num_white - 1;

    int curr_white = 0;
    for (int i = 0; i < 61; ++i) {
        int note = 36 + i;
        int note_in_oct = note % 12;
        bool is_black = (note_in_oct == 1 || note_in_oct == 3 || note_in_oct == 6 ||
                         note_in_oct == 8 || note_in_oct == 10);
        if (!is_black) {
            if (curr_white == white_idx) return note;
            curr_white++;
        }
    }
    return -1;
}

static int get_pad_note_at(int x, int y) {
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            int px = 500 + c * 180;
            int py = 135 + r * 100;
            int pw = 165;
            int ph = 88;
            if (x >= px && x < px + pw && y >= py && y < py + ph) {
                int pad_num = (3 - r) * 4 + c; // Pad 0..15 (MPC layout)
                return 36 + pad_num;
            }
        }
    }
    return -1;
}

// -------------------------------------------------------------
// Touchscreen Thread (reads /dev/input/event0 with poll)
// -------------------------------------------------------------
static void* touch_thread_func(void* arg) {
    (void)arg;
    int fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "[WARN] Could not open /dev/input/event0: %s\n", strerror(errno));
        return NULL;
    }

    printf("[TOUCH] Connected to ILI2117 Touchscreen (/dev/input/event0)\n");

    struct pollfd pfd = { fd, POLLIN, 0 };
    struct input_event ev;
    int raw_x = 0, raw_y = 0;
    bool touching = false;
    int last_touched_note = -1;

    while (g_running) {
        if (poll(&pfd, 1, 50) <= 0) continue;

        ssize_t n = read(fd, &ev, sizeof(ev));
        if (n != sizeof(ev)) continue;

        if (ev.type == EV_ABS) {
            if (ev.code == ABS_MT_POSITION_X || ev.code == ABS_X) raw_x = ev.value;
            if (ev.code == ABS_MT_POSITION_Y || ev.code == ABS_Y) raw_y = ev.value;
            if (ev.code == ABS_MT_TRACKING_ID) {
                if (ev.value == -1) {
                    touching = false;
                    if (last_touched_note != -1) {
                        synth_note_off(&g_synth, last_touched_note);
                        last_touched_note = -1;
                    }
                    g_touch_active = false;
                } else {
                    touching = true;
                }
            }
        } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
            touching = (ev.value > 0);
            if (!touching) {
                if (last_touched_note != -1) {
                    synth_note_off(&g_synth, last_touched_note);
                    last_touched_note = -1;
                }
                g_touch_active = false;
            }
        } else if (ev.type == EV_SYN && ev.code == SYN_REPORT && touching) {
            int ui_x = 0, ui_y = 0;
            raw_touch_to_ui(raw_x, raw_y, &ui_x, &ui_y);

            g_touch_active = true;
            g_touch_x = ui_x;
            g_touch_y = ui_y;

            // 1. Transport Buttons
            if (ui_y >= 14 && ui_y < 46) {
                if (ui_x >= 1060 && ui_x < 1120) {
                    g_is_playing = true;
                } else if (ui_x >= 1130 && ui_x < 1190) {
                    g_is_playing = false;
                    g_is_recording = false;
                } else if (ui_x >= 1200 && ui_x < 1260) {
                    g_is_recording = !g_is_recording;
                    if (g_is_recording) g_is_playing = true;
                }
            }

            // 2. Q-Link Knobs (Vertical Drag or Tap)
            for (int k = 0; k < 4; ++k) {
                int kx = 50 + (k % 2) * 190;
                int ky = 200 + (k / 2) * 130;
                if (ui_x >= kx && ui_x < kx + 160 && ui_y >= ky && ui_y < ky + 100) {
                    int val = 100 - ((ui_y - ky - 10) * 100 / 80);
                    if (val < 0) val = 0;
                    if (val > 100) val = 100;
                    g_knob_vals[k] = val;
                }
            }

            // 3. MPC 16 Drum Pads
            int pad_note = get_pad_note_at(ui_x, ui_y);
            if (pad_note != -1) {
                if (pad_note != last_touched_note) {
                    if (last_touched_note != -1) synth_note_off(&g_synth, last_touched_note);
                    synth_note_on(&g_synth, pad_note, 115, true /* is_pad */);
                    last_touched_note = pad_note;
                }
            } else {
                // 4. 61-Key Keyboard Piano
                int kb_note = get_keyboard_note_at(ui_x, ui_y);
                if (kb_note != -1) {
                    if (kb_note != last_touched_note) {
                        if (last_touched_note != -1) synth_note_off(&g_synth, last_touched_note);
                        synth_note_on(&g_synth, kb_note, 105, false /* is_pad */);
                        last_touched_note = kb_note;
                    }
                } else {
                    if (last_touched_note != -1) {
                        synth_note_off(&g_synth, last_touched_note);
                        last_touched_note = -1;
                    }
                }
            }
        }
    }

    close(fd);
    return NULL;
}

// -------------------------------------------------------------
// Hardware RawMIDI Thread (reads hw:0,0,0 & hw:0,0,1, writes hw:0,0,2)
// -------------------------------------------------------------
typedef struct {
    snd_rawmidi_t *in0, *in1, *out2;
} MidiCtx;

static void* midi_thread_func(void* arg) {
    MidiCtx* ctx = (MidiCtx*)arg;
    uint8_t buf[256];

    while (g_running) {
        if (ctx->in0) {
            ssize_t n = snd_rawmidi_read(ctx->in0, buf, sizeof(buf));
            if (n > 0) {
                if (ctx->out2) snd_rawmidi_write(ctx->out2, buf, n);
                for (ssize_t i = 0; i < n; ++i) {
                    uint8_t b = buf[i];
                    if ((b & 0xF0) == 0x90 && (i + 2 < n)) {
                        int note = buf[i+1], vel = buf[i+2];
                        if (vel > 0) synth_note_on(&g_synth, note, vel);
                        else synth_note_off(&g_synth, note);
                        i += 2;
                    } else if ((b & 0xF0) == 0x80 && (i + 2 < n)) {
                        synth_note_off(&g_synth, buf[i+1]);
                        i += 2;
                    }
                }
            }
        }
        if (ctx->in1) {
            ssize_t n = snd_rawmidi_read(ctx->in1, buf, sizeof(buf));
            if (n > 0) {
                if (ctx->out2) snd_rawmidi_write(ctx->out2, buf, n);
                for (ssize_t i = 0; i < n; ++i) {
                    uint8_t b = buf[i];
                    if ((b & 0xF0) == 0x90 && (i + 2 < n)) {
                        int note = buf[i+1], vel = buf[i+2];
                        if (vel > 0) synth_note_on(&g_synth, note, vel);
                        else synth_note_off(&g_synth, note);
                        i += 2;
                    } else if ((b & 0xF0) == 0x80 && (i + 2 < n)) {
                        synth_note_off(&g_synth, buf[i+1]);
                        i += 2;
                    }
                }
            }
        }
        usleep(1000);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    setvbuf(stdout, NULL, _IONBF, 0);
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    init_font();

    printf("===============================================================\n");
    printf("   KICK MPC KEY 61 - STANDALONE DAW GRAPHICAL APPLICATION      \n");
    printf("===============================================================\n");

    // 1. Open Framebuffer /dev/fb0
    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        perror("[ERR] Cannot open /dev/fb0");
        return 1;
    }
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
    ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo);

    g_fb_size = finfo.line_length * vinfo.yres;
    g_fb_ptr = (uint32_t*)mmap(NULL, g_fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (g_fb_ptr == MAP_FAILED) {
        perror("[ERR] mmap /dev/fb0 failed");
        close(fb_fd);
        return 1;
    }
    printf("[DISPLAY] rockchipdrmfb mapped: %dx%d (32 bpp, stride %d) -> SUCCESS\n",
           vinfo.xres, vinfo.yres, finfo.line_length);

    synth_init(&g_synth);

    // 2. Open Audio Codec plughw:1,0 (ACVM)
    snd_pcm_t* pcm = NULL;
    int err = snd_pcm_open(&pcm, "plughw:1,0", SND_PCM_STREAM_PLAYBACK, 0);
    if (err >= 0) {
        snd_pcm_set_params(pcm, SND_PCM_FORMAT_S32_LE, SND_PCM_ACCESS_RW_INTERLEAVED,
                           NUM_CHANNELS, SAMPLE_RATE, 1, 10000);
        printf("[AUDIO] ACVM 4-Channel Quad (Main + Headphones) -> READY\n");
    }

    pthread_t a_thread;
    if (pcm) pthread_create(&a_thread, NULL, audio_thread_func, pcm);

    // 3. Open RawMIDI
    snd_rawmidi_t *midi_in0 = NULL, *midi_out0 = NULL;
    snd_rawmidi_t *midi_in1 = NULL, *midi_out1 = NULL;
    snd_rawmidi_t *midi_in2 = NULL, *midi_out2 = NULL;

    snd_rawmidi_open(&midi_in0, &midi_out0, "hw:0,0,0", SND_RAWMIDI_NONBLOCK);
    snd_rawmidi_open(&midi_in1, &midi_out1, "hw:0,0,1", SND_RAWMIDI_NONBLOCK);
    snd_rawmidi_open(&midi_in2, &midi_out2, "hw:0,0,2", SND_RAWMIDI_NONBLOCK);

    uint8_t inquiry[] = { 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 };
    uint8_t akai_init[] = { 0xF0, 0x47, 0x7F, 0x4B, 0x60, 0x00, 0x04, 0x40, 0x00, 0x00, 0x00, 0xF7 };
    if (midi_out0) {
        snd_rawmidi_write(midi_out0, inquiry, sizeof(inquiry));
        snd_rawmidi_write(midi_out0, akai_init, sizeof(akai_init));
    }
    if (midi_out1) {
        snd_rawmidi_write(midi_out1, inquiry, sizeof(inquiry));
        snd_rawmidi_write(midi_out1, akai_init, sizeof(akai_init));
    }

    pthread_t m_thread;
    MidiCtx mctx = { midi_in0, midi_in1, midi_out2 };
    pthread_create(&m_thread, NULL, midi_thread_func, &mctx);

    // 4. Open Touchscreen Thread
    pthread_t t_thread;
    pthread_create(&t_thread, NULL, touch_thread_func, NULL);

    printf("\n>>> KICK DAW GRAPHICAL GUI RUNNING! (Press Ctrl+C to stop) <<<\n");

    // Initial render & screenshot
    render_ui();
    blit_to_framebuffer();
    save_screenshot("/data/kick_screen.ppm");

    // Main 30 FPS Render Loop
    int frame = 0;
    while (g_running) {
        render_ui();
        blit_to_framebuffer();
        usleep(33000); // 30 FPS (33ms)
        frame++;
        if (frame == 60) {
            // Save updated screenshot with active state
            save_screenshot("/data/kick_screen.ppm");
        }
    }

    printf("\nStopping Kick GUI...\n");
    if (pcm) {
        pthread_join(a_thread, NULL);
        snd_pcm_drain(pcm);
        snd_pcm_close(pcm);
    }
    pthread_join(m_thread, NULL);
    pthread_join(t_thread, NULL);

    if (midi_in0) snd_rawmidi_close(midi_in0);
    if (midi_out0) snd_rawmidi_close(midi_out0);
    if (midi_in1) snd_rawmidi_close(midi_in1);
    if (midi_out1) snd_rawmidi_close(midi_out1);
    if (midi_in2) snd_rawmidi_close(midi_in2);
    if (midi_out2) snd_rawmidi_close(midi_out2);

    munmap(g_fb_ptr, g_fb_size);
    close(fb_fd);

    printf("Kick GUI terminated cleanly.\n");
    return 0;
}
