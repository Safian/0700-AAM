/*
 * Kick MPC Key 61 - Direct RawMIDI Standalone Piano & DIN Thru Engine
 * Opens hw:0,0,0 (Public), hw:0,0,1 (Private), hw:0,0,2 (Port A DIN OUT), plughw:1,0 (ACVM Audio)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <alsa/asoundlib.h>

#define SAMPLE_RATE      44100
#define NUM_CHANNELS     4
#define BUFFER_FRAMES    128
#define MAX_VOICES       32
#define PI_F             3.14159265358979323846f

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
    float pitch_bend;
    bool sustain_pedal;
    pthread_mutex_t lock;
} SynthEngine;

static volatile bool g_running = true;
static SynthEngine g_synth;

static void sig_handler(int sig) {
    (void)sig;
    g_running = false;
}

static inline float midi_to_freq(int note, float bend_semitones) {
    return 440.0f * powf(2.0f, ((float)note + bend_semitones - 69.0f) / 12.0f);
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
    s->pitch_bend = 0.0f;
    s->sustain_pedal = false;
    for (int i = 0; i < MAX_VOICES; ++i) s->voices[i].active = false;
}

static void synth_note_on(SynthEngine* s, int note, int vel) {
    if (vel <= 0) {
        synth_note_on(s, note, 0);
        return;
    }
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
    v->freq = midi_to_freq(note, s->pitch_bend * 2.0f);
    v->env = v->velocity;
    float decay_sec = 3.5f - ((float)(note - 21) / 88.0f * 2.5f);
    if (decay_sec < 0.5f) decay_sec = 0.5f;
    v->decay_rate = powf(0.001f, 1.0f / (decay_sec * (float)SAMPLE_RATE));
    v->filter_state = 0.0f;
    v->active = true;
    v->releasing = false;
    pthread_mutex_unlock(&s->lock);
}

static void synth_note_off(SynthEngine* s, int note) {
    pthread_mutex_lock(&s->lock);
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (s->voices[i].active && s->voices[i].note == note) {
            if (!s->sustain_pedal) s->voices[i].releasing = true;
        }
    }
    pthread_mutex_unlock(&s->lock);
}

static void synth_render(SynthEngine* s, int32_t* out, size_t nframes) {
    pthread_mutex_lock(&s->lock);
    const float rel_rate = powf(0.001f, 1.0f / (0.05f * (float)SAMPLE_RATE));

    for (size_t f = 0; f < nframes; ++f) {
        float mix_l = 0.0f, mix_r = 0.0f;
        for (int i = 0; i < MAX_VOICES; ++i) {
            Voice* v = &s->voices[i];
            if (!v->active) continue;

            float osc = sinf(v->phase) + 0.4f * sinf(v->phase * 2.002f) + 0.2f * sinf(v->phase * 3.004f);
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

        // Ch 0 & 1: Main Out L/R
        out[f * NUM_CHANNELS + 0] = sl;
        out[f * NUM_CHANNELS + 1] = sr;
        // Ch 2 & 3: Headphone Out L/R
        out[f * NUM_CHANNELS + 2] = sl;
        out[f * NUM_CHANNELS + 3] = sr;
    }
    pthread_mutex_unlock(&s->lock);
}

typedef struct {
    snd_pcm_t* pcm;
} AudioCtx;

static void* audio_thread(void* arg) {
    AudioCtx* ctx = (AudioCtx*)arg;
    int32_t buf[BUFFER_FRAMES * NUM_CHANNELS];
    while (g_running) {
        synth_render(&g_synth, buf, BUFFER_FRAMES);
        snd_pcm_sframes_t f = snd_pcm_writei(ctx->pcm, buf, BUFFER_FRAMES);
        if (f < 0) f = snd_pcm_recover(ctx->pcm, (int)f, 0);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    setvbuf(stdout, NULL, _IONBF, 0);
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    printf("===============================================================\n");
    printf("   KICK MPC KEY 61 - DIRECT RAWMIDI PIANO & DIN THRU ENGINE   \n");
    printf("===============================================================\n");

    synth_init(&g_synth);

    // 1. Open Audio Codec (Card 1 ACVM: plughw:1,0)
    snd_pcm_t* pcm = NULL;
    int err = snd_pcm_open(&pcm, "plughw:1,0", SND_PCM_STREAM_PLAYBACK, 0);
    if (err >= 0) {
        err = snd_pcm_set_params(pcm, SND_PCM_FORMAT_S32_LE, SND_PCM_ACCESS_RW_INTERLEAVED,
                                 NUM_CHANNELS, SAMPLE_RATE, 1, 10000);
        if (err < 0) {
            fprintf(stderr, "[AUDIO ERR] set_params failed: %s\n", snd_strerror(err));
            snd_pcm_close(pcm);
            pcm = NULL;
        } else {
            printf("[AUDIO] ACVM 4-Channel S32_LE (Main + Headphones) -> READY\n");
        }
    } else {
        fprintf(stderr, "[AUDIO ERR] Could not open plughw:1,0: %s\n", snd_strerror(err));
    }

    // 2. Open RawMIDI Ports
    snd_rawmidi_t *midi_in0 = NULL, *midi_out0 = NULL;
    snd_rawmidi_t *midi_in1 = NULL, *midi_out1 = NULL;
    snd_rawmidi_t *midi_in2 = NULL, *midi_out2 = NULL;

    snd_rawmidi_open(&midi_in0, &midi_out0, "hw:0,0,0", SND_RAWMIDI_NONBLOCK);
    snd_rawmidi_open(&midi_in1, &midi_out1, "hw:0,0,1", SND_RAWMIDI_NONBLOCK);
    snd_rawmidi_open(&midi_in2, &midi_out2, "hw:0,0,2", SND_RAWMIDI_NONBLOCK);

    printf("[MIDI] hw:0,0,0 (Public):  In=%p Out=%p\n", (void*)midi_in0, (void*)midi_out0);
    printf("[MIDI] hw:0,0,1 (Private): In=%p Out=%p\n", (void*)midi_in1, (void*)midi_in1);
    printf("[MIDI] hw:0,0,2 (Port A):  In=%p Out=%p (DIN OUT)\n", (void*)midi_in2, (void*)midi_out2);

    // Send Akai Handshake & Wakeup to both Public and Private endpoints
    uint8_t inquiry[] = { 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 };
    uint8_t akai_init[] = { 0xF0, 0x47, 0x7F, 0x4B, 0x60, 0x00, 0x04, 0x40, 0x00, 0x00, 0x00, 0xF7 };
    uint8_t akai_all_notes_on[] = { 0xF0, 0x47, 0x7F, 0x4B, 0x62, 0x00, 0x01, 0x01, 0xF7 };

    if (midi_out0) {
        snd_rawmidi_write(midi_out0, inquiry, sizeof(inquiry));
        snd_rawmidi_write(midi_out0, akai_init, sizeof(akai_init));
        snd_rawmidi_write(midi_out0, akai_all_notes_on, sizeof(akai_all_notes_on));
    }
    if (midi_out1) {
        snd_rawmidi_write(midi_out1, inquiry, sizeof(inquiry));
        snd_rawmidi_write(midi_out1, akai_init, sizeof(akai_init));
        snd_rawmidi_write(midi_out1, akai_all_notes_on, sizeof(akai_all_notes_on));
    }

    pthread_t th;
    AudioCtx actx = { pcm };
    if (pcm) pthread_create(&th, NULL, audio_thread, &actx);

    printf("\n>>> ENGINE ACTIVE! PLAY ON THE KEY 61 KEYBOARD! (Press Ctrl+C to stop) <<<\n\n");

    uint8_t buf[256];
    while (g_running) {
        // Read from Public endpoint (0)
        if (midi_in0) {
            ssize_t n = snd_rawmidi_read(midi_in0, buf, sizeof(buf));
            if (n > 0) {
                // Forward to DIN Port A
                if (midi_out2) snd_rawmidi_write(midi_out2, buf, n);

                // Parse MIDI messages
                for (ssize_t i = 0; i < n; ++i) {
                    uint8_t b = buf[i];
                    if ((b & 0xF0) == 0x90 && (i + 2 < n)) { // Note On
                        int note = buf[i + 1];
                        int vel = buf[i + 2];
                        if (vel > 0) {
                            printf("[KEYBOARD] NoteOn:  %-3s (%3d) | Vel: %3d  ===> [SOUND: PIANO] & [DIN OUT]\n",
                                   note_name(note), note, vel);
                            synth_note_on(&g_synth, note, vel);
                        } else {
                            printf("[KEYBOARD] NoteOff: %-3s (%3d)\n", note_name(note), note);
                            synth_note_off(&g_synth, note);
                        }
                        i += 2;
                    } else if ((b & 0xF0) == 0x80 && (i + 2 < n)) { // Note Off
                        int note = buf[i + 1];
                        printf("[KEYBOARD] NoteOff: %-3s (%3d)\n", note_name(note), note);
                        synth_note_off(&g_synth, note);
                        i += 2;
                    } else if (b == 0xF0) {
                        printf("[SYSEX Public] ");
                        while (i < n && buf[i] != 0xF7) { printf("%02X ", buf[i]); i++; }
                        if (i < n) printf("%02X\n", buf[i]);
                    }
                }
            }
        }

        // Read from Private endpoint (1)
        if (midi_in1) {
            ssize_t n = snd_rawmidi_read(midi_in1, buf, sizeof(buf));
            if (n > 0) {
                for (ssize_t i = 0; i < n; ++i) {
                    uint8_t b = buf[i];
                    if ((b & 0xF0) == 0x90 && (i + 2 < n)) { // Note On on Private
                        int note = buf[i + 1];
                        int vel = buf[i + 2];
                        if (midi_out2) snd_rawmidi_write(midi_out2, &buf[i], 3);
                        if (vel > 0) {
                            printf("[KEYBOARD Priv] NoteOn:  %-3s (%3d) | Vel: %3d  ===> [SOUND: PIANO] & [DIN OUT]\n",
                                   note_name(note), note, vel);
                            synth_note_on(&g_synth, note, vel);
                        } else {
                            printf("[KEYBOARD Priv] NoteOff: %-3s (%3d)\n", note_name(note), note);
                            synth_note_off(&g_synth, note);
                        }
                        i += 2;
                    } else if ((b & 0xF0) == 0x80 && (i + 2 < n)) {
                        int note = buf[i + 1];
                        if (midi_out2) snd_rawmidi_write(midi_out2, &buf[i], 3);
                        printf("[KEYBOARD Priv] NoteOff: %-3s (%3d)\n", note_name(note), note);
                        synth_note_off(&g_synth, note);
                        i += 2;
                    } else if (b == 0xF0) {
                        printf("[SYSEX Priv] ");
                        while (i < n && buf[i] != 0xF7) { printf("%02X ", buf[i]); i++; }
                        if (i < n) printf("%02X\n", buf[i]);
                    }
                }
            }
        }

        usleep(1000); // 1ms polling
    }

    printf("\nCleaning up...\n");
    if (pcm) {
        pthread_join(th, NULL);
        snd_pcm_drain(pcm);
        snd_pcm_close(pcm);
    }
    if (midi_in0) snd_rawmidi_close(midi_in0);
    if (midi_out0) snd_rawmidi_close(midi_out0);
    if (midi_in1) snd_rawmidi_close(midi_in1);
    if (midi_out1) snd_rawmidi_close(midi_out1);
    if (midi_in2) snd_rawmidi_close(midi_in2);
    if (midi_out2) snd_rawmidi_close(midi_out2);

    printf("Engine stopped.\n");
    return 0;
}
