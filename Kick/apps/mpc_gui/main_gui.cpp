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
#include "hardware/device_factory.h"

static IHardwareAdapter* g_app_hw = nullptr;

#define SCREEN_W         1280
#define SCREEN_H         800
#define FB_PHYS_W        800
#define FB_PHYS_H        1280
#define SAMPLE_RATE      44100
#define NUM_CHANNELS     4
#define BUFFER_FRAMES    256
#define MAX_VOICES       32
#define MAX_SAMPLER_VOICES 16
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
    float* data_l;
    float* data_r;
    size_t num_frames;
    char name[64];
} WavSample;

typedef struct {
    int note;
    float velocity;
    int root_note;
    WavSample* sample;
    float pos_fp;
    float pitch_ratio;
    float gain;
    float release_env;
    bool active;
    bool key_down;
    bool releasing;
} PianoVoice;

typedef struct {
    bool active;
    int pad_num;
    size_t pos;
    float gain;
} SamplerVoice;

#define NUM_PIANO_ROOTS 21
static const int g_piano_roots[NUM_PIANO_ROOTS] = {
    36, 39, 42, 45, 48, 51, 54, 57, 60, 63, 66, 69, 72, 75, 78, 81, 84, 87, 90, 93, 96
};

static const char* g_piano_paths[NUM_PIANO_ROOTS] = {
    "/sdcard/Expansions/F9 Instruments/Inst-Piano-F9 Grand Piano-036 C1 105.WAV",
    "/sdcard/Expansions/F9 Instruments/Inst-Piano-F9 Grand Piano-039 Eb1 105.WAV",
    "/sdcard/Expansions/F9 Instruments/Inst-Piano-F9 Grand Piano-042 Gb1 105.WAV",
    "/sdcard/Expansions/F9 Instruments/Inst-Piano-F9 Grand Piano-045 A1 105.WAV",
    "/sdcard/Expansions/F9 Instruments/Inst-Piano-F9 Grand Piano-048 C2 105.WAV",
    "/sdcard/Expansions/F9 Instruments/Inst-Piano-F9 Grand Piano-051 Eb2 105.WAV",
    "/sdcard/Expansions/F9 Instruments/Inst-Piano-F9 Grand Piano-054 Gb2 105.WAV",
    "/sdcard/Expansions/F9 Instruments/Inst-Piano-F9 Grand Piano-057 A2 105.WAV",
    "/sdcard/Expansions/F9 Instruments/Inst-Piano-F9 Grand Piano-060 C3 105.WAV",
    "/sdcard/Expansions/F9 Instruments/Inst-Piano-F9 Grand Piano-063 Eb3 105.WAV",
    "/sdcard/Expansions/F9 Instruments/Inst-Piano-F9 Grand Piano-066 Gb3 105.WAV",
    "/sdcard/Expansions/F9 Instruments/Inst-Piano-F9 Grand Piano-069 A3 105.WAV",
    "/sdcard/Expansions/F9 Instruments/Inst-Piano-F9 Grand Piano-072 C4 105.WAV",
    "/sdcard/Expansions/F9 Instruments/Inst-Piano-F9 Grand Piano-075 Eb4 105.WAV",
    "/sdcard/Expansions/F9 Instruments/Inst-Piano-F9 Grand Piano-078 Gb4 105.WAV",
    "/sdcard/Expansions/F9 Instruments/Inst-Piano-F9 Grand Piano-081 A4 105.WAV",
    "/sdcard/Expansions/F9 Instruments/Inst-Piano-F9 Grand Piano-084 C5 105.WAV",
    "/sdcard/Expansions/F9 Instruments/Inst-Piano-F9 Grand Piano-087 Eb5 105.WAV",
    "/sdcard/Expansions/F9 Instruments/Inst-Piano-F9 Grand Piano-090 Gb5 105.WAV",
    "/sdcard/Expansions/F9 Instruments/Inst-Piano-F9 Grand Piano-093 A5 105.WAV",
    "/sdcard/Expansions/F9 Instruments/Inst-Piano-F9 Grand Piano-096 C6 105.WAV"
};

static WavSample g_pad_samples[16];
static WavSample g_piano_samples[NUM_PIANO_ROOTS];
static SamplerVoice g_sampler_voices[MAX_SAMPLER_VOICES];

static const char* g_pad_names[16] = {
    "KICK DNB",  "KICK 2",    "SNARE",     "RIMSHOT",
    "HAT CLSD",  "HAT OPEN",  "PERC RIM",  "CRASH",
    "BS PLUCK",  "BASS HIT",  "SYNTH STB", "SUB BASS",
    "LEAD SYN",  "VOX HOOK",  "FX RISER",  "DRUM LOOP"
};

static const char* g_sample_paths[16] = {
    "/data/samples/FL_DNB_KICK.wav",
    "/data/samples/Drums_-_Kick_2.wav",
    "/data/samples/Drums_-_Snare.wav",
    "/data/samples/TSP_OVERVIEW_YAANO_snare_fast_rim.wav",
    "/data/samples/14_DNB_Hats.wav",
    "/data/samples/TEDDY_KILLERZ_hihat_06.wav",
    "/data/samples/TSP_OVERVIEW_WINGZ_rim_bigpink_D#.wav",
    "/data/samples/BOS_DTTE_Crash_One_Shot_Seventies_Organic.wav",
    "/data/samples/MORE_GIRAFFES_bass_plait_pluck_C.wav",
    "/data/samples/TEDDY_KILLERZ_bass_short_bass_hit_02_F#.wav",
    "/data/samples/TEDDY_KILLERZ_synth_fear.wav",
    "/data/samples/BOS_NFXL_Bass_One_Shot_Bringing_F.wav",
    "/data/samples/BOS_HHB_152_Synth_Pluck_Loop_Lighter_Dm.wav",
    "/data/samples/DS_HT2_180_vocal_hook_female_life_middle_dry_Dmin.wav",
    "/data/samples/BOS_NFXL_174_FX_Riser_One_Shot_Complete.wav",
    "/data/samples/TEDDY_KILLERZ_172_drum_loop_kit_basics_01.wav"
};

typedef struct {
    PianoVoice piano_voices[MAX_VOICES];
    bool sustain_pedal;
    pthread_mutex_t lock;
} SynthEngine;

static volatile bool g_running = true;
static SynthEngine g_synth;

#define MAX_TOUCH_SLOTS 10

enum NoteSource {
    NOTE_SRC_NONE = 0,
    NOTE_SRC_KEYBED = 1,
    NOTE_SRC_PAD = 2,
    NOTE_SRC_BOTH = 3
};

struct TouchSlot {
    bool active;
    int tracking_id;
    int raw_x;
    int raw_y;
    int ui_x;
    int ui_y;
    bool has_x;
    bool has_y;
    int last_note;
    NoteSource last_src;
};

// UI State
static TouchSlot g_slots[MAX_TOUCH_SLOTS];
static int g_key_press_count[61]; // 61 keys (MIDI 36 to 96, C2 to C7)
static int g_pad_press_count[16]; // 16 MPC drum pads
static bool g_is_playing = false;
static bool g_is_recording = false;
static int g_knob_vals[4] = { 75, 40, 10, 85 };
static bool g_knob_touched[4] = { false, false, false, false };
static float g_master_volume = 0.50f; // Default 50% (-6 dB studio reference headroom)
static pthread_mutex_t g_ui_lock = PTHREAD_MUTEX_INITIALIZER;

static const char* g_default_hardware_name = "GENERIC LINUX DAW";

static float g_bpm = 120.0f;
static uint64_t g_last_tap_time_ms = 0;
static int g_tap_tempo_flash = 0;

enum SelectedParam {
    SEL_PARAM_TEMPO = 0,
    SEL_PARAM_MASTER_VOL = 1,
    SEL_PARAM_OCTAVE = 2,
    SEL_PARAM_CUTOFF = 3,
    SEL_PARAM_RESONANCE = 4,
    SEL_PARAM_ATTACK = 5,
    SEL_PARAM_DECAY = 6,
    NUM_SEL_PARAMS = 7
};

static SelectedParam g_selected_param = SEL_PARAM_TEMPO; // Default selected to TEMPO for Jog Wheel / Data Wheel!

static const char* get_selected_param_name(SelectedParam p) {
    switch (p) {
        case SEL_PARAM_TEMPO: return "TEMPO (BPM)";
        case SEL_PARAM_MASTER_VOL: return "MASTER VOL";
        case SEL_PARAM_OCTAVE: return "OCTAVE SHIFT";
        case SEL_PARAM_CUTOFF: return "Q-LINK 1 (CUTOFF)";
        case SEL_PARAM_RESONANCE: return "Q-LINK 2 (RESONANCE)";
        case SEL_PARAM_ATTACK: return "Q-LINK 3 (ATTACK)";
        case SEL_PARAM_DECAY: return "Q-LINK 4 (DECAY)";
        default: return "NONE";
    }
}

static int g_octave_shift = 0; // -3 to +3 octaves transposition
static bool g_octave_down_held = false;
static bool g_octave_up_held = false;
static int g_active_key_pitch[128] = { 0 };

static bool g_shift_down = false;
static bool g_full_level = false;
static bool g_16_levels = false;
static bool g_note_repeat = false;
static bool g_is_overdub = false;

static void adjust_selected_param(int delta) {
    switch (g_selected_param) {
        case SEL_PARAM_TEMPO:
            g_bpm = fminf(300.0f, fmaxf(30.0f, g_bpm + (float)delta * 0.5f));
            g_tap_tempo_flash = 4;
            printf("[PARAM] TEMPO adjusted by %+d -> %.1f BPM\n", delta, g_bpm);
            break;
        case SEL_PARAM_MASTER_VOL:
            g_master_volume = fminf(1.0f, fmaxf(0.0f, g_master_volume + (float)delta * 0.02f));
            printf("[PARAM] MASTER VOL adjusted by %+d -> %.0f%%\n", delta, g_master_volume * 100.0f);
            break;
        case SEL_PARAM_OCTAVE:
            if (delta > 0 && g_octave_shift < 3) g_octave_shift++;
            else if (delta < 0 && g_octave_shift > -3) g_octave_shift--;
            printf("[PARAM] OCTAVE adjusted by %+d -> %+d\n", delta, g_octave_shift);
            break;
        case SEL_PARAM_CUTOFF:
            g_knob_vals[0] = fminf(100, fmaxf(0, g_knob_vals[0] + delta * 2));
            printf("[PARAM] CUTOFF adjusted by %+d -> %d%%\n", delta, g_knob_vals[0]);
            break;
        case SEL_PARAM_RESONANCE:
            g_knob_vals[1] = fminf(100, fmaxf(0, g_knob_vals[1] + delta * 2));
            printf("[PARAM] RESONANCE adjusted by %+d -> %d%%\n", delta, g_knob_vals[1]);
            break;
        case SEL_PARAM_ATTACK:
            g_knob_vals[2] = fminf(100, fmaxf(0, g_knob_vals[2] + delta * 2));
            printf("[PARAM] ATTACK adjusted by %+d -> %d%%\n", delta, g_knob_vals[2]);
            break;
        case SEL_PARAM_DECAY:
            g_knob_vals[3] = fminf(100, fmaxf(0, g_knob_vals[3] + delta * 2));
            printf("[PARAM] DECAY adjusted by %+d -> %d%%\n", delta, g_knob_vals[3]);
            break;
        default:
            break;
    }
}

static uint32_t g_render_buf[SCREEN_W * SCREEN_H];

static void sig_handler(int sig) {
    (void)sig;
    g_running = false;
}

static inline float midi_to_freq(int note) {
    return 440.0f * powf(2.0f, ((float)note - 69.0f) / 12.0f);
}

static const char* note_name(int note) {
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static char bufs[4][16];
    static int idx = 0;
    idx = (idx + 1) % 4;
    if (note < 0) note = 0;
    if (note > 127) note = 127;
    int octave = (note / 12) - 1;
    snprintf(bufs[idx], sizeof(bufs[idx]), "%s%d", names[note % 12], octave);
    return bufs[idx];
}

// -------------------------------------------------------------
// WAV Sample Loader (16-bit and 24-bit PCM)
// -------------------------------------------------------------
static bool load_wav_file(const char* path, WavSample* sample, const char* label) {
    sample->data_l = NULL;
    sample->data_r = NULL;
    sample->num_frames = 0;
    snprintf(sample->name, sizeof(sample->name), "%s", label);

    FILE* f = fopen(path, "rb");
    if (!f) return false;

    char chunk_id[4];
    uint32_t chunk_size = 0;
    char format[4];
    if (fread(chunk_id, 1, 4, f) != 4 || memcmp(chunk_id, "RIFF", 4) != 0) { fclose(f); return false; }
    if (fread(&chunk_size, 4, 1, f) != 1) { fclose(f); return false; }
    if (fread(format, 1, 4, f) != 4 || memcmp(format, "WAVE", 4) != 0) { fclose(f); return false; }

    uint16_t audio_fmt = 0, num_ch = 0, bits_per_sample = 0;
    uint32_t sample_rate = 0;
    uint32_t data_size = 0;
    long data_pos = 0;

    while (fread(chunk_id, 1, 4, f) == 4 && fread(&chunk_size, 4, 1, f) == 1) {
        if (memcmp(chunk_id, "fmt ", 4) == 0) {
            fread(&audio_fmt, 2, 1, f);
            fread(&num_ch, 2, 1, f);
            fread(&sample_rate, 4, 1, f);
            fseek(f, 6, SEEK_CUR); // skip byte_rate, block_align
            fread(&bits_per_sample, 2, 1, f);
            int remaining = chunk_size - 16;
            if (remaining > 0) fseek(f, remaining, SEEK_CUR);
        } else if (memcmp(chunk_id, "data", 4) == 0) {
            data_size = chunk_size;
            data_pos = ftell(f);
            break;
        } else {
            fseek(f, chunk_size, SEEK_CUR);
        }
        if (chunk_size % 2 != 0) fseek(f, 1, SEEK_CUR);
    }

    if (data_pos == 0 || audio_fmt != 1 || num_ch == 0 || data_size == 0) {
        fclose(f);
        return false;
    }

    fseek(f, data_pos, SEEK_SET);
    int bytes_per_sample = bits_per_sample / 8;
    if (bytes_per_sample < 2 || bytes_per_sample > 3) { fclose(f); return false; }

    size_t total_samples = data_size / bytes_per_sample;
    size_t num_frames = total_samples / num_ch;

    sample->data_l = (float*)malloc(num_frames * sizeof(float));
    sample->data_r = (float*)malloc(num_frames * sizeof(float));
    sample->num_frames = num_frames;

    uint8_t* raw_buf = (uint8_t*)malloc(data_size);
    if (!raw_buf) {
        free(sample->data_l); free(sample->data_r);
        fclose(f); return false;
    }
    fread(raw_buf, 1, data_size, f);
    fclose(f);

    uint8_t* p = raw_buf;
    for (size_t i = 0; i < num_frames; ++i) {
        float sl = 0.0f, sr = 0.0f;
        if (bits_per_sample == 16) {
            int16_t val_l = (int16_t)(p[0] | (p[1] << 8));
            sl = (float)val_l / 32768.0f;
            p += 2;
            if (num_ch > 1) {
                int16_t val_r = (int16_t)(p[0] | (p[1] << 8));
                sr = (float)val_r / 32768.0f;
                p += 2;
            } else {
                sr = sl;
            }
        } else if (bits_per_sample == 24) {
            int32_t val_l = (int32_t)((p[0] << 8) | (p[1] << 16) | (p[2] << 24)) >> 8;
            sl = (float)val_l / 8388608.0f;
            p += 3;
            if (num_ch > 1) {
                int32_t val_r = (int32_t)((p[0] << 8) | (p[1] << 16) | (p[2] << 24)) >> 8;
                sr = (float)val_r / 8388608.0f;
                p += 3;
            } else {
                sr = sl;
            }
        }
        sample->data_l[i] = sl;
        sample->data_r[i] = sr;
    }
    free(raw_buf);
    printf("[SAMPLER] Loaded '%s': %zu frames (%d-bit, %d-ch)\n", label, num_frames, bits_per_sample, num_ch);
    return true;
}

static void trigger_pad_sample(int pad_num, int vel) {
    if (pad_num < 0 || pad_num >= 16) return;
    if (!g_pad_samples[pad_num].data_l) return;

    pthread_mutex_lock(&g_synth.lock);
    // If this pad is already playing (e.g. a drum loop), restart cleanly from frame 0
    int target = -1;
    for (int i = 0; i < MAX_SAMPLER_VOICES; ++i) {
        if (g_sampler_voices[i].active && g_sampler_voices[i].pad_num == pad_num) {
            target = i;
            break;
        }
    }
    if (target == -1) {
        for (int i = 0; i < MAX_SAMPLER_VOICES; ++i) {
            if (!g_sampler_voices[i].active) { target = i; break; }
        }
    }
    if (target == -1) target = 0; // steal voice 0

    SamplerVoice* sv = &g_sampler_voices[target];
    sv->pad_num = pad_num;
    sv->pos = 0;
    sv->gain = ((float)vel / 127.0f) * 0.65f;
    sv->active = true;
    pthread_mutex_unlock(&g_synth.lock);
}

static void stop_all_audio() {
    pthread_mutex_lock(&g_synth.lock);
    // 1. Immediately silence and reset all 16 drum sampler voices
    for (int v = 0; v < MAX_SAMPLER_VOICES; ++v) {
        g_sampler_voices[v].active = false;
        g_sampler_voices[v].pos = 0;
    }
    // 2. Immediately damp / mute all acoustic piano voices
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (g_synth.piano_voices[i].active) {
            g_synth.piano_voices[i].releasing = true;
            g_synth.piano_voices[i].release_env = 0.0f;
            g_synth.piano_voices[i].active = false;
        }
    }
    pthread_mutex_unlock(&g_synth.lock);

    // 3. Reset all UI press counters so keys and pads stop glowing
    pthread_mutex_lock(&g_ui_lock);
    for (int p = 0; p < 16; ++p) g_pad_press_count[p] = 0;
    for (int k = 0; k < 61; ++k) g_key_press_count[k] = 0;
    pthread_mutex_unlock(&g_ui_lock);

    printf("[AUDIO] STOP: All sampler and piano voices silenced immediately.\n");
}

static inline float master_limit(float x) {
    if (x > 1.30f) return 0.99f;
    if (x < -1.30f) return -0.99f;
    return tanhf(x * 0.85f);
}

static int find_closest_piano_root(int note) {
    int best = 0;
    int min_d = 999;
    for (int i = 0; i < NUM_PIANO_ROOTS; ++i) {
        int d = abs(note - g_piano_roots[i]);
        if (d < min_d) {
            min_d = d;
            best = i;
        }
    }
    return best;
}

static void synth_init(SynthEngine* s) {
    pthread_mutex_init(&s->lock, NULL);
    s->sustain_pedal = false;
    for (int i = 0; i < MAX_VOICES; ++i) {
        s->piano_voices[i].active = false;
        s->piano_voices[i].key_down = false;
        s->piano_voices[i].releasing = false;
    }
    for (int i = 0; i < MAX_SAMPLER_VOICES; ++i) g_sampler_voices[i].active = false;
    for (int i = 0; i < 61; ++i) g_key_press_count[i] = 0;
    for (int i = 0; i < 16; ++i) g_pad_press_count[i] = 0;
    for (int i = 0; i < MAX_TOUCH_SLOTS; ++i) {
        g_slots[i].active = false;
        g_slots[i].tracking_id = -1;
        g_slots[i].last_note = -1;
        g_slots[i].last_src = NOTE_SRC_NONE;
        g_slots[i].has_x = false;
        g_slots[i].has_y = false;
        g_slots[i].ui_x = -1;
        g_slots[i].ui_y = -1;
    }

    // 1. Load 16 MPC Drum Samples
    printf("[SAMPLER] Initializing 16 Studio Drum Samples...\n");
    for (int i = 0; i < 16; ++i) {
        load_wav_file(g_sample_paths[i], &g_pad_samples[i], g_pad_names[i]);
    }

    // 2. Load 21 F9 Concert Grand Piano Multi-Samples
    printf("[PIANO] Initializing 21 Acoustic Concert Grand Piano Multi-Samples...\n");
    for (int i = 0; i < NUM_PIANO_ROOTS; ++i) {
        char root_lbl[32];
        snprintf(root_lbl, sizeof(root_lbl), "Grand Piano %s", note_name(g_piano_roots[i]));
        load_wav_file(g_piano_paths[i], &g_piano_samples[i], root_lbl);
    }
}

static void synth_note_on_transposed(SynthEngine* s, int sound_pitch, int phys_note, int vel, NoteSource src = NOTE_SRC_KEYBED) {
    if (vel <= 0) return;

    if (src == NOTE_SRC_PAD) {
        int pad_num = sound_pitch - 36;
        if (pad_num >= 0 && pad_num < 16) {
            trigger_pad_sample(pad_num, vel);
        }
    } else {
        // Genuine Acoustic Concert Grand Piano Multi-Sampled Voice
        int root_idx = find_closest_piano_root(sound_pitch);
        int root_note = g_piano_roots[root_idx];
        float pitch_ratio = powf(2.0f, (float)(sound_pitch - root_note) / 12.0f);

        pthread_mutex_lock(&s->lock);
        int target = -1;
        for (int i = 0; i < MAX_VOICES; ++i) {
            if (!s->piano_voices[i].active) { target = i; break; }
        }
        if (target == -1) {
            for (int i = 0; i < MAX_VOICES; ++i) {
                if (s->piano_voices[i].releasing) { target = i; break; }
            }
            if (target == -1) target = 0;
        }

        PianoVoice* pv = &s->piano_voices[target];
        pv->note = sound_pitch;
        pv->velocity = (float)vel / 127.0f;
        pv->root_note = root_note;
        pv->sample = &g_piano_samples[root_idx];
        pv->pos_fp = 0.0f;
        pv->pitch_ratio = pitch_ratio;
        pv->gain = ((float)vel / 127.0f) * 0.60f;
        pv->release_env = 1.0f;
        pv->key_down = true;
        pv->releasing = false;
        pv->active = true;
        pthread_mutex_unlock(&s->lock);
    }

    // Update UI Key State - completely independent!
    pthread_mutex_lock(&g_ui_lock);
    int key_idx = phys_note - 36;
    if (src == NOTE_SRC_KEYBED || src == NOTE_SRC_BOTH) {
        if (key_idx >= 0 && key_idx < 61) g_key_press_count[key_idx]++;
    }
    int pad_idx = sound_pitch - 36;
    if (src == NOTE_SRC_PAD || src == NOTE_SRC_BOTH) {
        if (pad_idx >= 0 && pad_idx < 16) g_pad_press_count[pad_idx]++;
    }
    pthread_mutex_unlock(&g_ui_lock);
}

static void synth_note_off_transposed(SynthEngine* s, int sound_pitch, int phys_note, NoteSource src = NOTE_SRC_KEYBED) {
    if (src != NOTE_SRC_PAD) {
        pthread_mutex_lock(&s->lock);
        for (int i = 0; i < MAX_VOICES; ++i) {
            if (s->piano_voices[i].active && s->piano_voices[i].note == sound_pitch) {
                s->piano_voices[i].key_down = false;
                // If sustain pedal is NOT pressed, damp the string now
                if (!s->sustain_pedal && !s->piano_voices[i].releasing) {
                    s->piano_voices[i].releasing = true;
                }
            }
        }
        pthread_mutex_unlock(&s->lock);
    }

    // Update UI Key State
    pthread_mutex_lock(&g_ui_lock);
    int key_idx = phys_note - 36;
    if (src == NOTE_SRC_KEYBED || src == NOTE_SRC_BOTH) {
        if (key_idx >= 0 && key_idx < 61) {
            if (g_key_press_count[key_idx] > 0) g_key_press_count[key_idx]--;
        }
    }
    int pad_idx = sound_pitch - 36;
    if (src == NOTE_SRC_PAD || src == NOTE_SRC_BOTH) {
        if (pad_idx >= 0 && pad_idx < 16) {
            if (g_pad_press_count[pad_idx] > 0) g_pad_press_count[pad_idx]--;
        }
    }
    pthread_mutex_unlock(&g_ui_lock);
}

static void synth_note_on(SynthEngine* s, int note, int vel, NoteSource src = NOTE_SRC_KEYBED) {
    synth_note_on_transposed(s, note, note, vel, src);
}

static void synth_note_off(SynthEngine* s, int note, NoteSource src = NOTE_SRC_KEYBED) {
    synth_note_off_transposed(s, note, note, src);
}

static void synth_cc(SynthEngine* s, int cc, int val) {
    if (cc == 64) { // Damper / Sustain Pedal (Standard CC 64)
        bool pedal_down = (val >= 64);
        pthread_mutex_lock(&s->lock);
        s->sustain_pedal = pedal_down;
        if (!pedal_down) {
            // Damper pedal released: damp all strings where key was already released
            for (int i = 0; i < MAX_VOICES; ++i) {
                PianoVoice* pv = &s->piano_voices[i];
                if (pv->active && !pv->key_down && !pv->releasing) {
                    pv->releasing = true;
                }
            }
        }
        pthread_mutex_unlock(&s->lock);
        printf("[PEDAL] CC 64 Sustain: %d -> %s\n", val, pedal_down ? "ACTIVE (SUSTAIN)" : "OFF (DAMP)");
    } else if (cc == 117) { // Hardware STOP Button (CC 117)
        if (val > 0) {
            g_is_playing = false;
            g_is_recording = false;
            stop_all_audio();
        }
        printf("[HARDWARE-CC] STOP (CC 117 = %d) -> All audio stopped\n", val);
    } else if (cc == 118) { // Hardware PLAY Button (CC 118)
        if (val > 0) {
            g_is_playing = true;
        }
        printf("[HARDWARE-CC] PLAY (CC 118 = %d)\n", val);
    } else if (cc == 119) { // Hardware REC Button (CC 119)
        if (val > 0) {
            g_is_recording = !g_is_recording;
            if (g_is_recording) g_is_playing = true;
        }
        printf("[HARDWARE-CC] REC (CC 119 = %d) -> %s\n", val, g_is_recording ? "ON" : "OFF");
    } else if (cc == 116) { // Hardware OVERDUB Button (CC 116)
        if (val > 0) {
            g_is_recording = !g_is_recording;
        }
        printf("[HARDWARE-CC] OVERDUB (CC 116 = %d)\n", val);
    } else if (cc == 7 || cc == 11) { // MIDI Volume / Expression
        g_master_volume = (float)val / 127.0f;
        printf("[MIDI] Master Volume CC%d -> %.0f%%\n", cc, g_master_volume * 100.0f);
    } else if (cc == 69) { // Akai Touch Strip
        g_knob_vals[0] = (val * 100) / 127;
        printf("[TOUCH-STRIP] Position: %d -> Cutoff: %d%%\n", val, g_knob_vals[0]);
    } else if (cc == 60) { // Akai Data Wheel (Relative Rotary Jog)
        int delta = (val < 64) ? val : (val - 128);
        adjust_selected_param(delta);
    } else {
        printf("[MIDI] CC %d = %d\n", cc, val);
    }
}

static void synth_render_floats(SynthEngine* s, float* out_l, float* out_r, size_t nframes) {
    pthread_mutex_lock(&s->lock);
    const float rel_rate = powf(0.001f, 1.0f / (0.15f * (float)SAMPLE_RATE));

    for (size_t f = 0; f < nframes; ++f) {
        float mix_l = 0.0f, mix_r = 0.0f;

        // 1. Genuine Acoustic Concert Grand Piano Multi-Sampled Voices
        for (int i = 0; i < MAX_VOICES; ++i) {
            PianoVoice* pv = &s->piano_voices[i];
            if (!pv->active || !pv->sample || !pv->sample->data_l) continue;

            size_t idx0 = (size_t)pv->pos_fp;
            size_t idx1 = idx0 + 1;
            float frac = pv->pos_fp - (float)idx0;

            float s_l = 0.0f, s_r = 0.0f;
            if (idx1 < pv->sample->num_frames) {
                s_l = pv->sample->data_l[idx0] + frac * (pv->sample->data_l[idx1] - pv->sample->data_l[idx0]);
                s_r = pv->sample->data_r[idx0] + frac * (pv->sample->data_r[idx1] - pv->sample->data_r[idx0]);
            } else if (idx0 < pv->sample->num_frames) {
                s_l = pv->sample->data_l[idx0];
                s_r = pv->sample->data_r[idx0];
            } else {
                pv->active = false;
                continue;
            }

            float vol = pv->gain * pv->release_env;
            mix_l += s_l * vol;
            mix_r += s_r * vol;

            pv->pos_fp += pv->pitch_ratio;

            if (pv->releasing) {
                pv->release_env *= rel_rate;
                if (pv->release_env < 0.001f) {
                    pv->active = false;
                }
            }
        }

        // 2. WAV Studio Drum Sampler Voices (16 MPC Pads)
        for (int v = 0; v < MAX_SAMPLER_VOICES; ++v) {
            SamplerVoice* sv = &g_sampler_voices[v];
            if (!sv->active) continue;
            WavSample* ws = &g_pad_samples[sv->pad_num];
            if (!ws->data_l || sv->pos >= ws->num_frames) {
                sv->active = false;
                continue;
            }

            mix_l += ws->data_l[sv->pos] * sv->gain;
            mix_r += ws->data_r[sv->pos] * sv->gain;
            sv->pos++;
        }

        // 3. Master Soft-Knee Limiter with Master Volume
        out_l[f] = master_limit(mix_l * 0.45f) * g_master_volume;
        out_r[f] = master_limit(mix_r * 0.45f) * g_master_volume;
    }
    pthread_mutex_unlock(&s->lock);
}

static void audio_render_callback(float* out_l, float* out_r, int num_frames, void* user_data) {
    SynthEngine* s = (SynthEngine*)user_data;
    synth_render_floats(s, out_l, out_r, num_frames);
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
    set_glyph('#', 0x24, 0x7F, 0x24, 0x7F, 0x24);
    set_glyph('&', 0x36, 0x49, 0x55, 0x22, 0x50);
    set_glyph('-', 0x08, 0x08, 0x08, 0x08, 0x08);
    set_glyph('+', 0x08, 0x08, 0x3E, 0x08, 0x08);
    set_glyph(':', 0x00, 0x36, 0x36, 0x00, 0x00);
    set_glyph('.', 0x00, 0x60, 0x60, 0x00, 0x00);
    set_glyph('/', 0x20, 0x10, 0x08, 0x04, 0x02);
    set_glyph('|', 0x00, 0x00, 0x7F, 0x00, 0x00);
    set_glyph('[', 0x00, 0x7F, 0x41, 0x41, 0x00);
    set_glyph(']', 0x00, 0x41, 0x41, 0x7F, 0x00);
    set_glyph('(', 0x00, 0x1C, 0x22, 0x41, 0x00);
    set_glyph(')', 0x00, 0x41, 0x22, 0x1C, 0x00);
    set_glyph(',', 0x00, 0x50, 0x30, 0x00, 0x00);
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
    const char* dev_name = g_app_hw ? g_app_hw->get_device_name() : g_default_hardware_name;
    draw_string(185, 24, dev_name, COLOR_TEXT_BOLD, 2);

    // Hardware mapping badge
    fill_rect(375, 20, 56, 20, 0x001B3A4D);
    draw_rect(375, 20, 56, 20, COLOR_CYAN);
    draw_string(381, 26, "HAL-OK", COLOR_CYAN, 1);

    // Master Volume Control (Touch Buttons & Slider)
    bool vol_sel = (g_selected_param == SEL_PARAM_MASTER_VOL);
    fill_rect(440, 14, 260, 32, vol_sel ? 0x002B3444 : COLOR_PANEL_HI);
    draw_rect(440, 14, 260, 32, vol_sel ? COLOR_GOLD : COLOR_BORDER);
    if (vol_sel) draw_rect(441, 15, 258, 30, COLOR_GOLD);
    draw_string(450, 24, "VOL", vol_sel ? COLOR_GOLD : COLOR_TEXT_BOLD, 1);

    // [-] button
    fill_rect(478, 18, 26, 24, 0x002A303D);
    draw_rect(478, 18, 26, 24, COLOR_BORDER);
    draw_string(487, 22, "-", COLOR_CYAN, 2);

    // Volume Slider Bar
    int bar_x = 512, bar_y = 22, bar_w = 95, bar_h = 16;
    fill_rect(bar_x, bar_y, bar_w, bar_h, COLOR_BG);
    draw_rect(bar_x, bar_y, bar_w, bar_h, COLOR_BORDER);
    int fill_w = (int)(g_master_volume * (float)(bar_w - 2));
    if (fill_w < 0) fill_w = 0;
    if (fill_w > bar_w - 2) fill_w = bar_w - 2;
    if (fill_w > 0) fill_rect(bar_x + 1, bar_y + 1, fill_w, bar_h - 2, COLOR_CYAN);

    // [+] button
    fill_rect(615, 18, 26, 24, 0x002A303D);
    draw_rect(615, 18, 26, 24, COLOR_BORDER);
    draw_string(623, 22, "+", COLOR_CYAN, 2);

    // Volume percentage text
    char vol_pct_str[16];
    snprintf(vol_pct_str, sizeof(vol_pct_str), "%3d%%", (int)(g_master_volume * 100.0f + 0.5f));
    draw_string(648, 24, vol_pct_str, COLOR_TEXT_BOLD, 1);

    // BPM & Latency badge (Pulsing gold flash on Tap Tempo! Glowing border if selected)
    bool bpm_sel = (g_selected_param == SEL_PARAM_TEMPO);
    uint32_t bpm_bg = (g_tap_tempo_flash > 0) ? 0x005E4410 : (bpm_sel ? 0x003A2E10 : COLOR_PANEL_HI);
    uint32_t bpm_fg = (g_tap_tempo_flash > 0 || bpm_sel) ? COLOR_GOLD : COLOR_TEXT_BOLD;
    fill_rect(720, 14, 110, 32, bpm_bg);
    draw_rect(720, 14, 110, 32, (g_tap_tempo_flash > 0 || bpm_sel) ? COLOR_GOLD : COLOR_BORDER);
    if (bpm_sel) draw_rect(721, 15, 108, 30, COLOR_GOLD);
    char bpm_str[32];
    snprintf(bpm_str, sizeof(bpm_str), "%.1f BPM", g_bpm);
    draw_string(728, 22, bpm_str, bpm_fg, 2);
    if (g_tap_tempo_flash > 0) g_tap_tempo_flash--;

    fill_rect(845, 14, 190, 32, COLOR_PANEL_HI);
    draw_rect(845, 14, 190, 32, COLOR_BORDER);
    draw_string(855, 22, "ACVM 4-CH 5.8ms", COLOR_GREEN, 2);

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
    draw_string(44, 130, "ENGINE: F9 CONCERT GRAND (MULTI-SAMPLED)", COLOR_TEXT_DIM, 1);
    draw_string(44, 155, "MIDI IN: KEYBED 61 + PADS  |  OUT: DIN A", COLOR_CYAN, 1);

    bool sustain_on = false;
    pthread_mutex_lock(&g_synth.lock);
    sustain_on = g_synth.sustain_pedal;
    pthread_mutex_unlock(&g_synth.lock);

    fill_rect(44, 172, 115, 20, sustain_on ? 0x001B4D2E : COLOR_PANEL_HI);
    draw_rect(44, 172, 115, 20, sustain_on ? COLOR_GREEN : COLOR_BORDER);
    draw_string(50, 178, sustain_on ? "PEDAL: SUSTAIN" : "PEDAL: OFF", sustain_on ? COLOR_GREEN : COLOR_TEXT_DIM, 1);

    // Master Volume info badge
    fill_rect(168, 172, 130, 20, COLOR_PANEL_HI);
    draw_rect(168, 172, 130, 20, COLOR_BORDER);
    char master_info[32];
    snprintf(master_info, sizeof(master_info), "MASTER: %d%%", (int)(g_master_volume * 100.0f + 0.5f));
    draw_string(176, 178, master_info, COLOR_GOLD, 1);

    // Octave Shift info badge
    bool oct_active = (g_octave_shift != 0);
    fill_rect(308, 172, 116, 20, oct_active ? 0x00452E10 : COLOR_PANEL_HI);
    draw_rect(308, 172, 116, 20, oct_active ? COLOR_GOLD : COLOR_BORDER);
    char oct_info[32];
    snprintf(oct_info, sizeof(oct_info), "OCTAVE: %+d", g_octave_shift);
    draw_string(318, 178, oct_info, oct_active ? COLOR_GOLD : COLOR_TEXT_DIM, 1);

    // 4 Q-Link Knobs Simulation (Hardware Encoders + Touch Sensors)
    const char* knob_labels[4] = { "CUTOFF", "RESONANCE", "ATTACK", "DECAY" };
    for (int k = 0; k < 4; ++k) {
        int kx = 50 + (k % 2) * 190;
        int ky = 200 + (k / 2) * 130;
        bool touched = g_knob_touched[k];
        bool knob_sel = (g_selected_param == (SelectedParam)(SEL_PARAM_CUTOFF + k));

        fill_rect(kx, ky, 160, 100, touched ? 0x001B3A4D : (knob_sel ? 0x002B3444 : COLOR_PANEL_HI));
        uint32_t b_col = touched ? COLOR_CYAN : (knob_sel ? COLOR_GOLD : COLOR_BORDER);
        draw_rect(kx, ky, 160, 100, b_col);
        if (knob_sel) draw_rect(kx + 1, ky + 1, 158, 98, COLOR_GOLD);
        draw_string(kx + 15, ky + 15, knob_labels[k], touched ? COLOR_CYAN : (knob_sel ? COLOR_GOLD : COLOR_TEXT_NORM), 1);

        // Knob circle
        int cx = kx + 80, cy = ky + 60, r = 24;
        uint32_t ring_col = touched ? COLOR_CYAN : COLOR_ORANGE;
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (dx*dx + dy*dy <= r*r) {
                    draw_pixel(cx + dx, cy + dy, (dx*dx + dy*dy <= (r-4)*(r-4)) ? COLOR_PANEL : ring_col);
                }
            }
        }

        // Value pointer line from center towards the angle of g_knob_vals[k]
        float angle = (-135.0f + 270.0f * (float)g_knob_vals[k] / 100.0f) * (PI_F / 180.0f);
        int ptr_x = cx + (int)(17.0f * sinf(angle));
        int ptr_y = cy - (int)(17.0f * cosf(angle));
        for (int pdy = -2; pdy <= 2; ++pdy) {
            for (int pdx = -2; pdx <= 2; ++pdx) {
                draw_pixel(ptr_x + pdx, ptr_y + pdy, COLOR_GOLD);
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

            bool active = (g_pad_press_count[pad_num] > 0);
            uint32_t pad_color = active ? COLOR_PAD_ACTIVE : COLOR_PAD_BASE;
            uint32_t border_col = active ? COLOR_GOLD : 0x00454D5A;

            fill_rect(px, py, pw, ph, pad_color);
            draw_rect(px, py, pw, ph, border_col);

            // Pad label with sample name
            char pad_lbl[32];
            snprintf(pad_lbl, sizeof(pad_lbl), "P%02d: %s", pad_num + 1, g_pad_names[pad_num]);
            draw_string(px + 10, py + 12, pad_lbl, active ? COLOR_TEXT_BOLD : COLOR_TEXT_NORM, 1);

            int midi_note = 36 + pad_num;
            draw_string(px + 12, py + 55, note_name(midi_note), active ? COLOR_TEXT_BOLD : COLOR_GOLD, 2);
        }
    }
    pthread_mutex_unlock(&g_ui_lock);

    // 5a. Keybed Status & Octave Transposition Control Bar (Y = 560 to 586)
    fill_rect(24, 560, 1232, 26, COLOR_PANEL);
    draw_rect(24, 560, 1232, 26, COLOR_BORDER);

    int low_c = 36 + g_octave_shift * 12;
    int high_c = 96 + g_octave_shift * 12;
    char kb_range[64];
    snprintf(kb_range, sizeof(kb_range), "KEYBED 61: %s - %s", note_name(low_c), note_name(high_c));
    draw_string(36, 568, kb_range, COLOR_TEXT_BOLD, 1);

    // Center Octave Buttons
    int oct_btn_x = 540;
    draw_string(oct_btn_x - 90, 568, "OCTAVE:", COLOR_TEXT_NORM, 1);

    // [-]
    fill_rect(oct_btn_x, 563, 24, 20, (g_octave_shift < 0) ? 0x00452E10 : 0x002A303D);
    draw_rect(oct_btn_x, 563, 24, 20, (g_octave_shift < 0) ? COLOR_GOLD : COLOR_BORDER);
    draw_string(oct_btn_x + 8, 568, "-", COLOR_CYAN, 1);

    // [+X] badge
    char oct_str[16];
    snprintf(oct_str, sizeof(oct_str), "%+d", g_octave_shift);
    fill_rect(oct_btn_x + 28, 563, 40, 20, (g_octave_shift != 0) ? 0x00452E10 : COLOR_PANEL_HI);
    draw_rect(oct_btn_x + 28, 563, 40, 20, (g_octave_shift != 0) ? COLOR_GOLD : COLOR_BORDER);
    draw_string(oct_btn_x + 40, 568, oct_str, (g_octave_shift != 0) ? COLOR_GOLD : COLOR_TEXT_NORM, 1);

    // [+]
    fill_rect(oct_btn_x + 72, 563, 24, 20, (g_octave_shift > 0) ? 0x00452E10 : 0x002A303D);
    draw_rect(oct_btn_x + 72, 563, 24, 20, (g_octave_shift > 0) ? COLOR_GOLD : COLOR_BORDER);
    draw_string(oct_btn_x + 80, 568, "+", COLOR_CYAN, 1);

    // Right Side Status: Active Parameter for Data Wheel / Jog Dial
    char jog_hint[64];
    snprintf(jog_hint, sizeof(jog_hint), "JOG DIAL: [%s]", get_selected_param_name(g_selected_param));
    draw_string(870, 568, jog_hint, COLOR_GOLD, 1);

    // 5b. Bottom 61-Key Keyboard Piano (Height = 195, Y = 588 to 783)
    fill_rect(24, 588, 1232, 195, COLOR_PANEL);
    draw_rect(24, 588, 1232, 195, COLOR_BORDER);

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
            int ky = 596;
            int w = (int)kw - 2;
            int h = 180;

            bool active = (g_key_press_count[i] > 0);
            uint32_t col = active ? COLOR_GOLD : 0x00EEEEEE;
            fill_rect(kx, ky, w, h, col);
            draw_rect(kx, ky, w, h, 0x00999999);

            if (note_in_octave == 0) { // Mark C notes with transposed octave number!
                int transposed_note = note + g_octave_shift * 12;
                draw_string(kx + 6, ky + h - 25, note_name(transposed_note), 0x00333333, 1);
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
            int ky = 596;
            int w = (int)(kw * 0.70f);
            int h = 110;

            bool active = (g_key_press_count[i] > 0);
            uint32_t col = active ? COLOR_ORANGE : 0x00151515;
            fill_rect(kx, ky, w, h, col);
            draw_rect(kx, ky, w, h, 0x00000000);
        }
    }
    // 6. Touch Visual Feedback Indicator for all active slots
    for (int s = 0; s < MAX_TOUCH_SLOTS; ++s) {
        if (!g_slots[s].active || !g_slots[s].has_x || !g_slots[s].has_y) continue;
        int tx = g_slots[s].ui_x;
        int ty = g_slots[s].ui_y;
        if (tx >= 0 && tx < SCREEN_W && ty >= 0 && ty < SCREEN_H) {
            int tr = 14;
            for (int dy = -tr; dy <= tr; ++dy) {
                for (int dx = -tr; dx <= tr; ++dx) {
                    int dist2 = dx*dx + dy*dy;
                    if (dist2 <= tr*tr && dist2 >= (tr-3)*(tr-3)) {
                        draw_pixel(tx + dx, ty + dy, COLOR_CYAN);
                    }
                }
            }
            for (int dy = -2; dy <= 2; ++dy) {
                for (int dx = -2; dx <= 2; ++dx) {
                    draw_pixel(tx + dx, ty + dy, COLOR_GOLD);
                }
            }
        }
    }
    pthread_mutex_unlock(&g_ui_lock);
}

// -------------------------------------------------------------
// UI Utilities & Hit Testing
// -------------------------------------------------------------
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

static void update_all_leds() {
    if (!g_app_hw || !g_app_hw->control_surface()) return;
    auto cs = g_app_hw->control_surface();
    cs->set_led(HW_LED_PLAY, g_is_playing);
    cs->set_led(HW_LED_STOP, (!g_is_playing && !g_is_recording));
    cs->set_led(HW_LED_REC, g_is_recording);
    cs->set_led(HW_LED_OCTAVE_DOWN, (g_octave_shift < 0));
    cs->set_led(HW_LED_OCTAVE_UP, (g_octave_shift > 0));
    cs->set_led(HW_LED_FULL_LEVEL, g_full_level);
    cs->set_led(HW_LED_16_LEVELS, g_16_levels);
    cs->set_led(HW_LED_NOTE_REPEAT, g_note_repeat);
}

// ============================================================================
// Kick DAW Application Hardware Event Listener Implementation
// ============================================================================
class KickDawApp : public IHardwareEventListener {
public:
    void on_touch_down(int slot, int x, int y) override {
        if (slot < 0 || slot >= MAX_TOUCH_SLOTS) return;
        TouchSlot* s = &g_slots[slot];
        s->active = true;
        s->has_x = true;
        s->has_y = true;
        s->ui_x = x;
        s->ui_y = y;

        // 1. Header Bar Controls (y: 0..75)
        if (y >= 0 && y < 75) {
            if (x >= 1040 && x < 1120) {
                g_is_playing = true;
                update_all_leds();
                printf("[UI] PLAY Pressed\n");
            } else if (x >= 1120 && x < 1195) {
                g_is_playing = false;
                g_is_recording = false;
                stop_all_audio();
                update_all_leds();
                printf("[UI] STOP Pressed -> Silencing all audio\n");
            } else if (x >= 1195 && x < 1270) {
                g_is_recording = !g_is_recording;
                if (g_is_recording) g_is_playing = true;
                update_all_leds();
                printf("[UI] REC Pressed: %s\n", g_is_recording ? "ON" : "OFF");
            } else if (x >= 715 && x < 840) {
                g_selected_param = SEL_PARAM_TEMPO;
                struct timespec ts;
                clock_gettime(CLOCK_MONOTONIC, &ts);
                uint64_t now_ms = (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
                if (g_last_tap_time_ms > 0) {
                    uint64_t dt = now_ms - g_last_tap_time_ms;
                    if (dt >= 200 && dt <= 2500) {
                        float new_bpm = 60000.0f / (float)dt;
                        if (new_bpm >= 30.0f && new_bpm <= 300.0f) g_bpm = new_bpm;
                    }
                }
                g_last_tap_time_ms = now_ms;
                g_tap_tempo_flash = 8;
                printf("[UI] BPM Badge Tapped -> Selected TEMPO, BPM: %.1f\n", g_bpm);
            } else if (x >= 440 && x < 510) {
                g_selected_param = SEL_PARAM_MASTER_VOL;
                g_master_volume -= 0.05f;
                if (g_master_volume < 0.0f) g_master_volume = 0.0f;
                printf("[VOL] Master Volume: %.0f%%\n", g_master_volume * 100.0f);
            } else if (x >= 610 && x < 685) {
                g_selected_param = SEL_PARAM_MASTER_VOL;
                g_master_volume += 0.05f;
                if (g_master_volume > 1.0f) g_master_volume = 1.0f;
                printf("[VOL] Master Volume: %.0f%%\n", g_master_volume * 100.0f);
            } else if (x >= 510 && x < 610) {
                g_selected_param = SEL_PARAM_MASTER_VOL;
                float new_vol = (float)(x - 512) / 95.0f;
                if (new_vol < 0.0f) new_vol = 0.0f;
                if (new_vol > 1.0f) new_vol = 1.0f;
                g_master_volume = new_vol;
            }
        }

        // 1b. Left Inspector Panel Volume Badge Touch
        if (x >= 160 && x < 300 && y >= 150 && y < 195) {
            g_selected_param = SEL_PARAM_MASTER_VOL;
            if (x < 230) {
                g_master_volume -= 0.05f;
                if (g_master_volume < 0.0f) g_master_volume = 0.0f;
            } else {
                g_master_volume += 0.05f;
                if (g_master_volume > 1.0f) g_master_volume = 1.0f;
            }
            printf("[VOL] Inspector Tap -> Master Volume: %.0f%%\n", g_master_volume * 100.0f);
        }

        // 1c. Keybed Octave Transposition Bar Touch Hitboxes (y: 550..588)
        if (y >= 550 && y < 588) {
            g_selected_param = SEL_PARAM_OCTAVE;
            if (x >= 520 && x < 566) {
                if (g_octave_shift > -3) g_octave_shift--;
                printf("[UI] Octave Shift [-] -> %+d\n", g_octave_shift);
                update_all_leds();
            } else if (x >= 610 && x < 650) {
                if (g_octave_shift < 3) g_octave_shift++;
                printf("[UI] Octave Shift [+] -> %+d\n", g_octave_shift);
                update_all_leds();
            } else if (x >= 566 && x < 610) {
                g_octave_shift = 0;
                printf("[UI] Octave Shift Reset -> 0\n");
                update_all_leds();
            }
        }

        // 2. Q-Link Knobs (Vertical Drag or Tap)
        for (int k = 0; k < 4; ++k) {
            int kx = 50 + (k % 2) * 190;
            int ky = 190 + (k / 2) * 130;
            if (x >= kx && x < kx + 160 && y >= ky && y < ky + 100) {
                g_selected_param = (SelectedParam)(SEL_PARAM_CUTOFF + k);
                int val = 100 - ((y - ky - 10) * 100 / 80);
                if (val < 0) val = 0;
                if (val > 100) val = 100;
                g_knob_vals[k] = val;
            }
        }

        // 3. MPC 16 Drum Pads (ONLY activates pad, NOT keyboard key!)
        int pad_note = get_pad_note_at(x, y);
        if (pad_note != -1) {
            if (s->last_note != pad_note || s->last_src != NOTE_SRC_PAD) {
                if (s->last_note != -1) synth_note_off(&g_synth, s->last_note, s->last_src);
                synth_note_on(&g_synth, pad_note, 115, NOTE_SRC_PAD);
                s->last_note = pad_note;
                s->last_src = NOTE_SRC_PAD;
            }
        } else {
            // 4. 61-Key Keyboard Piano (ONLY activates keyboard key, NOT pad!)
            int kb_note = get_keyboard_note_at(x, y);
            if (kb_note != -1) {
                if (s->last_note != kb_note || s->last_src != NOTE_SRC_KEYBED) {
                    if (s->last_note != -1) {
                        int old_sound = s->last_note + (g_octave_shift * 12);
                        synth_note_off_transposed(&g_synth, old_sound, s->last_note, s->last_src);
                    }
                    int sound_pitch = kb_note + (g_octave_shift * 12);
                    synth_note_on_transposed(&g_synth, sound_pitch, kb_note, 105, NOTE_SRC_KEYBED);
                    s->last_note = kb_note;
                    s->last_src = NOTE_SRC_KEYBED;
                }
            } else {
                // Background / non-note area
                if (s->last_note != -1) {
                    int old_sound = (s->last_src == NOTE_SRC_KEYBED) ?
                                    (s->last_note + (g_octave_shift * 12)) : s->last_note;
                    synth_note_off_transposed(&g_synth, old_sound, s->last_note, s->last_src);
                    s->last_note = -1;
                    s->last_src = NOTE_SRC_NONE;
                }
            }
        }
    }

    void on_touch_move(int slot, int x, int y) override {
        on_touch_down(slot, x, y);
    }

    void on_touch_up(int slot) override {
        if (slot < 0 || slot >= MAX_TOUCH_SLOTS) return;
        TouchSlot* s = &g_slots[slot];
        if (s->active) {
            if (s->last_note != -1) {
                int old_sound = (s->last_src == NOTE_SRC_KEYBED) ?
                                (s->last_note + (g_octave_shift * 12)) : s->last_note;
                synth_note_off_transposed(&g_synth, old_sound, s->last_note, s->last_src);
                s->last_note = -1;
                s->last_src = NOTE_SRC_NONE;
            }
            s->active = false;
            s->has_x = false;
            s->has_y = false;
        }
    }

    void on_button_press(HardwareButton btn) override {
        switch (btn) {
            case HW_BTN_PLAY:
            case HW_BTN_PLAY_START:
                g_is_playing = true;
                printf("[HAL-APP] PLAY Activated\n");
                update_all_leds();
                break;
            case HW_BTN_STOP:
                g_is_playing = false;
                g_is_recording = false;
                stop_all_audio();
                printf("[HAL-APP] STOP -> Silencing all audio\n");
                update_all_leds();
                break;
            case HW_BTN_REC:
                g_is_recording = !g_is_recording;
                if (g_is_recording) g_is_playing = true;
                printf("[HAL-APP] REC: %s\n", g_is_recording ? "ON" : "OFF");
                update_all_leds();
                break;
            case HW_BTN_OVERDUB:
                g_is_overdub = !g_is_overdub;
                printf("[HAL-APP] OVERDUB: %s\n", g_is_overdub ? "ON" : "OFF");
                break;
            case HW_BTN_TAP_TEMPO: {
                struct timespec ts;
                clock_gettime(CLOCK_MONOTONIC, &ts);
                uint64_t now_ms = (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
                if (g_last_tap_time_ms > 0) {
                    uint64_t dt = now_ms - g_last_tap_time_ms;
                    if (dt >= 200 && dt <= 2500) {
                        float new_bpm = 60000.0f / (float)dt;
                        if (new_bpm >= 30.0f && new_bpm <= 300.0f) g_bpm = new_bpm;
                    }
                }
                g_last_tap_time_ms = now_ms;
                g_selected_param = SEL_PARAM_TEMPO;
                g_tap_tempo_flash = 10;
                printf("[HAL-APP] TAP TEMPO -> BPM: %.1f\n", g_bpm);
                break;
            }
            case HW_BTN_PLUS:
                adjust_selected_param(+1);
                break;
            case HW_BTN_MINUS:
                adjust_selected_param(-1);
                break;
            case HW_BTN_ENTER:
                g_selected_param = (SelectedParam)((g_selected_param + 1) % NUM_SEL_PARAMS);
                printf("[HAL-APP] ENTER -> Selected: %s\n", get_selected_param_name(g_selected_param));
                break;
            case HW_BTN_CURSOR_UP:
            case HW_BTN_CURSOR_LEFT:
                g_selected_param = (SelectedParam)((g_selected_param + NUM_SEL_PARAMS - 1) % NUM_SEL_PARAMS);
                printf("[HAL-APP] CURSOR -> Selected: %s\n", get_selected_param_name(g_selected_param));
                break;
            case HW_BTN_CURSOR_DOWN:
            case HW_BTN_CURSOR_RIGHT:
                g_selected_param = (SelectedParam)((g_selected_param + 1) % NUM_SEL_PARAMS);
                printf("[HAL-APP] CURSOR -> Selected: %s\n", get_selected_param_name(g_selected_param));
                break;
            case HW_BTN_OCTAVE_DOWN:
                g_octave_down_held = true;
                if (g_octave_up_held) {
                    g_octave_shift = 0;
                } else if (g_octave_shift > -3) {
                    g_octave_shift--;
                }
                g_selected_param = SEL_PARAM_OCTAVE;
                printf("[HAL-APP] OCTAVE DOWN -> Shift: %+d\n", g_octave_shift);
                update_all_leds();
                break;
            case HW_BTN_OCTAVE_UP:
                g_octave_up_held = true;
                if (g_octave_down_held) {
                    g_octave_shift = 0;
                } else if (g_octave_shift < 3) {
                    g_octave_shift++;
                }
                g_selected_param = SEL_PARAM_OCTAVE;
                printf("[HAL-APP] OCTAVE UP -> Shift: %+d\n", g_octave_shift);
                update_all_leds();
                break;
            case HW_BTN_FULL_LEVEL:
                g_full_level = !g_full_level;
                printf("[HAL-APP] FULL LEVEL -> %s\n", g_full_level ? "ON" : "OFF");
                update_all_leds();
                break;
            case HW_BTN_16_LEVELS:
                g_16_levels = !g_16_levels;
                printf("[HAL-APP] 16 LEVELS -> %s\n", g_16_levels ? "ON" : "OFF");
                update_all_leds();
                break;
            case HW_BTN_NOTE_REPEAT:
                g_note_repeat = !g_note_repeat;
                printf("[HAL-APP] NOTE REPEAT -> %s\n", g_note_repeat ? "ON" : "OFF");
                update_all_leds();
                break;
            case HW_BTN_SHIFT:
                g_shift_down = true;
                break;
            default:
                break;
        }
    }

    void on_button_release(HardwareButton btn) override {
        switch (btn) {
            case HW_BTN_OCTAVE_DOWN:
                g_octave_down_held = false;
                break;
            case HW_BTN_OCTAVE_UP:
                g_octave_up_held = false;
                break;
            case HW_BTN_SHIFT:
                g_shift_down = false;
                break;
            default:
                break;
        }
    }

    void on_jog_dial(int delta) override {
        adjust_selected_param(delta);
    }

    void on_encoder_turn(int encoder_idx, int delta) override {
        if (encoder_idx >= 0 && encoder_idx < 4) {
            int new_val = g_knob_vals[encoder_idx] + delta * 2;
            if (new_val < 0) new_val = 0;
            if (new_val > 100) new_val = 100;
            g_knob_vals[encoder_idx] = new_val;
            g_selected_param = (SelectedParam)(SEL_PARAM_CUTOFF + encoder_idx);
            printf("[HAL-APP] Q-Link %d -> %d%%\n", encoder_idx + 1, new_val);
        }
    }

    void on_encoder_touch(int encoder_idx, bool touched) override {
        if (encoder_idx >= 0 && encoder_idx < 4) {
            g_knob_touched[encoder_idx] = touched;
            if (touched) g_selected_param = (SelectedParam)(SEL_PARAM_CUTOFF + encoder_idx);
            printf("[HAL-APP] Q-Link %d Touch: %s\n", encoder_idx + 1, touched ? "ON" : "OFF");
        }
    }

    void on_touch_strip(int val) override {
        g_knob_vals[0] = (val * 100) / 127;
        printf("[HAL-APP] Touch Strip -> Cutoff %d%%\n", g_knob_vals[0]);
    }

    void on_pitch_bend(int val) override {
        (void)val;
    }

    void on_sustain_pedal(bool down) override {
        synth_cc(&g_synth, 64, down ? 127 : 0);
    }

    void on_keybed_note_on(int pitch, int vel) override {
        if (pitch == 7 || pitch == 10) return; // Filter out hardware octave buttons
        int sound_pitch = pitch + (g_octave_shift * 12);
        if (sound_pitch < 0) sound_pitch = 0;
        if (sound_pitch > 127) sound_pitch = 127;
        g_active_key_pitch[pitch] = sound_pitch;
        synth_note_on_transposed(&g_synth, sound_pitch, pitch, vel, NOTE_SRC_KEYBED);
    }

    void on_keybed_note_off(int pitch) override {
        if (pitch == 7 || pitch == 10) return;
        int sound_pitch = g_active_key_pitch[pitch];
        if (sound_pitch == 0 && pitch != 0) sound_pitch = pitch + (g_octave_shift * 12);
        synth_note_off_transposed(&g_synth, sound_pitch, pitch, NOTE_SRC_KEYBED);
        g_active_key_pitch[pitch] = 0;
    }

    void on_pad_note_on(int pad_num, int vel) override {
        if (pad_num >= 0 && pad_num < 16) {
            int note = 36 + pad_num;
            synth_note_on(&g_synth, note, g_full_level ? 127 : vel, NOTE_SRC_PAD);
        }
    }

    void on_pad_note_off(int pad_num) override {
        if (pad_num >= 0 && pad_num < 16) {
            int note = 36 + pad_num;
            synth_note_off(&g_synth, note, NOTE_SRC_PAD);
        }
    }
};

// -------------------------------------------------------------
// Application Entry Point
// -------------------------------------------------------------
int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    setvbuf(stdout, NULL, _IONBF, 0);
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    init_font();

    printf("===============================================================\n");
    printf("   KICK DAW STANDALONE - HARDWARE ABSTRACTION LAYER (HAL)      \n");
    printf("===============================================================\n");

    // 1. Auto-detect and instantiate hardware adapter via Factory
    g_app_hw = HardwareDeviceFactory::create();
    if (!g_app_hw || !g_app_hw->init()) {
        fprintf(stderr, "[ERR] Hardware initialization failed!\n");
        return 1;
    }

    printf("[HAL] Active Device: %s (Badge: %s)\n",
           g_app_hw->get_device_name(), g_app_hw->get_device_badge());

    synth_init(&g_synth);

    // 2. Start Audio Adapter
    if (g_app_hw->audio()) {
        g_app_hw->audio()->start(audio_render_callback, &g_synth, SAMPLE_RATE, BUFFER_FRAMES);
    }

    // 3. Start Control Surface & Touch with Application Listener
    KickDawApp app;
    if (g_app_hw->control_surface()) {
        g_app_hw->control_surface()->start(&app);
    }
    if (g_app_hw->touch()) {
        g_app_hw->touch()->start(&app, SCREEN_W, SCREEN_H);
    }

    update_all_leds();

    printf("\n>>> KICK DAW RUNNING VIA HAL (Press Ctrl+C to stop) <<<\n");

    // Initial render & screenshot
    render_ui();
    g_app_hw->display()->blit(g_render_buf, SCREEN_W, SCREEN_H);
    save_screenshot("/data/kick_screen.ppm");

    // Main 30 FPS Render Loop
    int frame = 0;
    while (g_running) {
        render_ui();
        g_app_hw->display()->blit(g_render_buf, SCREEN_W, SCREEN_H);
        usleep(33000); // 30 FPS (33ms)
        frame++;
        if (frame == 60) {
            save_screenshot("/data/kick_screen.ppm");
        }
    }

    printf("\nStopping Kick DAW...\n");
    g_app_hw->shutdown();
    delete g_app_hw;
    g_app_hw = nullptr;

    printf("Kick DAW terminated cleanly.\n");
    return 0;
}
