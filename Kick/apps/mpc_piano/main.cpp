/*
 * Kick MPC Key 61 - Real-Time Polyphonic Piano & MIDI Thru Engine
 * Target: Akai MPC Key 61 (Rockchip RK3288 ARMv7l, Linux 6.6 PREEMPT_RT)
 *
 * Capabilities:
 *  1. Connects to ALSA Sequencer port 16:0 ("MPC Public" - 61-key bed, aftertouch, pads, pitch bend)
 *  2. Forwards all MIDI events in real-time (<0.2ms) to port 16:2 ("MPC MIDI Port A" - 5-pin DIN OUT)
 *  3. Generates 32-voice polyphonic acoustic/electric piano sound directly to ALSA Audio (Card 1 ACVM: plughw:1,0 or default)
 *  4. Supports Sustain Pedal (CC 64), Velocity dynamics, Pitch Bend, and Channel Aftertouch
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>
#include <poll.h>
#include <pthread.h>
#include <alsa/asoundlib.h>

#define SAMPLE_RATE      44100
#define NUM_CHANNELS     4   // InMusic ACVM: Ch 1-2 = Main Out, Ch 3-4 = Headphone Out
#define BUFFER_FRAMES    128
#define MAX_VOICES       32
#define PI_F             3.14159265358979323846f

typedef struct {
    int note;
    float velocity;
    float phase;
    float phase2;
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

static void sigint_handler(int sig) {
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
    for (int i = 0; i < MAX_VOICES; ++i) {
        s->voices[i].active = false;
        s->voices[i].releasing = false;
        s->voices[i].env = 0.0f;
    }
}

static void synth_note_on(SynthEngine* s, int note, int vel) {
    if (vel <= 0) {
        // Note off in disguise
        synth_note_on(s, note, 0);
        return;
    }
    pthread_mutex_lock(&s->lock);

    // Look for existing voice with same note
    int target = -1;
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (s->voices[i].active && s->voices[i].note == note) {
            target = i;
            break;
        }
    }

    // Otherwise find an inactive voice
    if (target == -1) {
        for (int i = 0; i < MAX_VOICES; ++i) {
            if (!s->voices[i].active) {
                target = i;
                break;
            }
        }
    }

    // Otherwise steal the quietest voice
    if (target == -1) {
        float min_env = 1000.0f;
        for (int i = 0; i < MAX_VOICES; ++i) {
            if (s->voices[i].env < min_env) {
                min_env = s->voices[i].env;
                target = i;
            }
        }
    }

    if (target >= 0) {
        Voice* v = &s->voices[target];
        v->note = note;
        v->velocity = (float)vel / 127.0f;
        v->phase = 0.0f;
        v->phase2 = 0.0f;
        v->freq = midi_to_freq(note, s->pitch_bend * 2.0f);
        v->env = v->velocity * 1.0f;
        // Natural piano decay: higher notes decay much faster
        float note_factor = (float)(note - 21) / 88.0f; // 0 to 1 across 88 keys
        float decay_time_sec = 4.0f - (note_factor * 3.2f); // 4.0s low notes down to 0.8s high notes
        if (decay_time_sec < 0.4f) decay_time_sec = 0.4f;
        v->decay_rate = powf(0.001f, 1.0f / (decay_time_sec * (float)SAMPLE_RATE));
        v->filter_state = 0.0f;
        v->active = true;
        v->releasing = false;
    }

    pthread_mutex_unlock(&s->lock);
}

static void synth_note_off(SynthEngine* s, int note) {
    pthread_mutex_lock(&s->lock);
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (s->voices[i].active && s->voices[i].note == note) {
            if (s->sustain_pedal) {
                // Held by sustain pedal
                s->voices[i].releasing = false;
            } else {
                s->voices[i].releasing = true;
            }
        }
    }
    pthread_mutex_unlock(&s->lock);
}

static void synth_set_sustain(SynthEngine* s, bool pedal_down) {
    pthread_mutex_lock(&s->lock);
    s->sustain_pedal = pedal_down;
    if (!pedal_down) {
        // Release all sustained voices
        for (int i = 0; i < MAX_VOICES; ++i) {
            if (s->voices[i].active) {
                s->voices[i].releasing = true;
            }
        }
    }
    pthread_mutex_unlock(&s->lock);
}

static void synth_set_pitch_bend(SynthEngine* s, float bend) {
    pthread_mutex_lock(&s->lock);
    s->pitch_bend = bend;
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (s->voices[i].active) {
            s->voices[i].freq = midi_to_freq(s->voices[i].note, s->pitch_bend * 2.0f);
        }
    }
    pthread_mutex_unlock(&s->lock);
}

static void synth_render_s32(SynthEngine* s, int32_t* output_buffer, size_t num_frames) {
    pthread_mutex_lock(&s->lock);

    const float release_rate = powf(0.001f, 1.0f / (0.05f * (float)SAMPLE_RATE));

    for (size_t f = 0; f < num_frames; ++f) {
        float sample_l = 0.0f;
        float sample_r = 0.0f;

        for (int i = 0; i < MAX_VOICES; ++i) {
            Voice* v = &s->voices[i];
            if (!v->active) continue;

            float osc1 = sinf(v->phase);
            float osc2 = 0.45f * sinf(v->phase * 2.002f);
            float osc3 = 0.20f * sinf(v->phase * 3.005f);
            float raw_voice = (osc1 + osc2 + osc3);

            float cutoff = 0.08f + (v->velocity * 0.45f) * (v->env / (v->velocity + 0.001f));
            if (cutoff > 0.85f) cutoff = 0.85f;
            v->filter_state += cutoff * (raw_voice - v->filter_state);

            float voice_out = v->filter_state * v->env;

            float pan = (float)(v->note - 21) / 88.0f;
            if (pan < 0.1f) pan = 0.1f;
            if (pan > 0.9f) pan = 0.9f;

            sample_l += voice_out * (1.0f - pan * 0.5f);
            sample_r += voice_out * (0.5f + pan * 0.5f);

            float delta = 2.0f * PI_F * v->freq / (float)SAMPLE_RATE;
            v->phase += delta;
            if (v->phase >= 2.0f * PI_F) v->phase -= 2.0f * PI_F;

            if (v->releasing) {
                v->env *= release_rate;
                if (v->env < 0.0001f) v->active = false;
            } else {
                v->env *= v->decay_rate;
                if (v->env < 0.0001f) v->active = false;
            }
        }

        if (sample_l > 1.0f) sample_l = 1.0f;
        else if (sample_l < -1.0f) sample_l = -1.0f;
        if (sample_r > 1.0f) sample_r = 1.0f;
        else if (sample_r < -1.0f) sample_r = -1.0f;

        // 32-bit signed PCM (scale to 2^30)
        int32_t val_l = (int32_t)(sample_l * 1073741824.0f);
        int32_t val_r = (int32_t)(sample_r * 1073741824.0f);

        // Ch 0 & 1: Main Out L/R
        output_buffer[f * NUM_CHANNELS + 0] = val_l;
        output_buffer[f * NUM_CHANNELS + 1] = val_r;
        // Ch 2 & 3: Headphone Out L/R
        output_buffer[f * NUM_CHANNELS + 2] = val_l;
        output_buffer[f * NUM_CHANNELS + 3] = val_r;
    }

    pthread_mutex_unlock(&s->lock);
}

// Audio output thread
typedef struct {
    const char* device_name;
    snd_pcm_t* pcm_handle;
} AudioContext;

static void* audio_thread_func(void* arg) {
    AudioContext* ctx = (AudioContext*)arg;
    int32_t buffer[BUFFER_FRAMES * NUM_CHANNELS];

    printf("[AUDIO] Playback thread started on device: %s (4-channel S32_LE)\n", ctx->device_name);

    while (g_running) {
        synth_render_s32(&g_synth, buffer, BUFFER_FRAMES);

        snd_pcm_sframes_t frames = snd_pcm_writei(ctx->pcm_handle, buffer, BUFFER_FRAMES);
        if (frames < 0) {
            frames = snd_pcm_recover(ctx->pcm_handle, (int)frames, 0);
            if (frames < 0) {
                fprintf(stderr, "[AUDIO ERR] snd_pcm_writei failed: %s\n", snd_strerror((int)frames));
                usleep(2000);
            }
        }
    }

    printf("[AUDIO] Playback thread terminated.\n");
    return NULL;
}

int main(int argc, char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    const char* audio_device = "hw:1,0";
    int mpc_in_client = 16;
    int mpc_in_port = 0;   // MPC Public
    int mpc_out_client = 16;
    int mpc_out_port = 2;  // MPC MIDI Port A (DIN OUT)

    if (argc >= 2) {
        audio_device = argv[1];
    }

    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    printf("===============================================================\n");
    printf("   KICK MPC KEY 61 - STANDALONE PIANO & MIDI THRU ENGINE      \n");
    printf("   Cross-compiled for RK3288 ARMv7l PREEMPT_RT Linux           \n");
    printf("===============================================================\n");
    printf("[CONFIG] Audio Target:   %s (%d Hz, S32_LE 4-Ch Quad)\n", audio_device, SAMPLE_RATE);
    printf("[CONFIG] MIDI Source:    Port %d:%d (MPC Public Keybed/Pads)\n", mpc_in_client, mpc_in_port);
    printf("[CONFIG] MIDI Forward:   Port %d:%d (MPC MIDI Port A - DIN OUT)\n", mpc_out_client, mpc_out_port);

    synth_init(&g_synth);

    // 1. Initialize ALSA Audio PCM
    snd_pcm_t* pcm_handle = NULL;
    int err = snd_pcm_open(&pcm_handle, audio_device, SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        fprintf(stderr, "[WARN] Could not open audio device '%s': %s\n", audio_device, snd_strerror(err));
        const char* fallbacks[] = { "plughw:1,0", "hw:1,0", "default" };
        for (size_t i = 0; i < sizeof(fallbacks)/sizeof(fallbacks[0]); ++i) {
            if (strcmp(audio_device, fallbacks[i]) == 0) continue;
            printf("[AUDIO] Trying fallback device: %s...\n", fallbacks[i]);
            err = snd_pcm_open(&pcm_handle, fallbacks[i], SND_PCM_STREAM_PLAYBACK, 0);
            if (err >= 0) {
                audio_device = fallbacks[i];
                printf("[AUDIO] Successfully opened fallback audio: %s\n", audio_device);
                break;
            }
        }
    }

    if (pcm_handle != NULL) {
        err = snd_pcm_set_params(pcm_handle,
                                 SND_PCM_FORMAT_S32_LE,
                                 SND_PCM_ACCESS_RW_INTERLEAVED,
                                 NUM_CHANNELS,
                                 SAMPLE_RATE,
                                 1,     // soft resample
                                 10000); // 10ms low latency
        if (err < 0) {
            fprintf(stderr, "[WARN] snd_pcm_set_params S32_LE failed: %s, trying S16_LE...\n", snd_strerror(err));
            err = snd_pcm_set_params(pcm_handle,
                                     SND_PCM_FORMAT_S16_LE,
                                     SND_PCM_ACCESS_RW_INTERLEAVED,
                                     2,
                                     SAMPLE_RATE,
                                     1,
                                     25000);
        }
        if (err < 0) {
            fprintf(stderr, "[WARN] Audio params setup failed: %s\n", snd_strerror(err));
            snd_pcm_close(pcm_handle);
            pcm_handle = NULL;
        }
    }

    if (pcm_handle == NULL) {
        printf("[AUDIO INFO] Running in pure MIDI Thru / Monitor mode (Hardware Soundcard busy by MPC).\n");
    }

    // 2. Initialize ALSA Sequencer
    snd_seq_t* seq = NULL;
    err = snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0);
    if (err < 0) {
        fprintf(stderr, "[ERR] snd_seq_open failed: %s\n", snd_strerror(err));
        if (pcm_handle) snd_pcm_close(pcm_handle);
        return 1;
    }

    snd_seq_set_client_name(seq, "Kick-MPC-Piano");

    // Create a sequencer queue for timestamped incoming events
    int queue = snd_seq_alloc_named_queue(seq, "KickQueue");
    snd_seq_start_queue(seq, queue, NULL);
    snd_seq_drain_output(seq);

    // Create input port with timestamping support
    snd_seq_port_info_t* pinfo = NULL;
    snd_seq_port_info_alloca(&pinfo);
    snd_seq_port_info_set_capability(pinfo, SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE);
    snd_seq_port_info_set_type(pinfo, SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    snd_seq_port_info_set_midi_channels(pinfo, 16);
    snd_seq_port_info_set_timestamping(pinfo, 1);
    snd_seq_port_info_set_timestamp_real(pinfo, 1);
    snd_seq_port_info_set_timestamp_queue(pinfo, queue);
    snd_seq_port_info_set_name(pinfo, "In");
    int in_port = snd_seq_create_port(seq, pinfo);

    int out_port = snd_seq_create_simple_port(seq, "Out",
        SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
        SND_SEQ_PORT_TYPE_APPLICATION);

    printf("[ALSA SEQ] Created ports: In (%d), Out (%d), Queue (%d)\n", in_port, out_port, queue);

    // Connect from MPC Public (16:0) -> our In port with queue & timestamping
    snd_seq_port_subscribe_t* subs = NULL;
    snd_seq_port_subscribe_alloca(&subs);
    snd_seq_addr_t sender = { (unsigned char)mpc_in_client, (unsigned char)mpc_in_port };
    snd_seq_addr_t dest = { (unsigned char)snd_seq_client_id(seq), (unsigned char)in_port };
    snd_seq_port_subscribe_set_sender(subs, &sender);
    snd_seq_port_subscribe_set_dest(subs, &dest);
    snd_seq_port_subscribe_set_queue(subs, queue);
    snd_seq_port_subscribe_set_time_update(subs, 1);
    snd_seq_port_subscribe_set_time_real(subs, 1);

    err = snd_seq_subscribe_port(seq, subs);
    if (err >= 0) {
        printf("[ALSA SEQ] Subscribed to %d:%d (MPC Public Keybed) -> SUCCESS\n", mpc_in_client, mpc_in_port);
    } else {
        printf("[ALSA SEQ WARN] Subscribed to %d:%d failed: %s\n",
               mpc_in_client, mpc_in_port, snd_strerror(err));
    }

    // Auto-connect from our Out port -> MPC MIDI Port A (16:2)
    err = snd_seq_connect_to(seq, out_port, mpc_out_client, mpc_out_port);
    if (err >= 0) {
        printf("[ALSA SEQ] Connected to %d:%d (MPC MIDI Port A DIN OUT) -> SUCCESS\n", mpc_out_client, mpc_out_port);
    } else {
        printf("[ALSA SEQ WARN] Auto-connect to %d:%d failed: %s\n",
               mpc_out_client, mpc_out_port, snd_strerror(err));
    }

    // Set up non-blocking poll descriptors for sequencer
    int npfd = snd_seq_poll_descriptors_count(seq, POLLIN);
    struct pollfd* pfd = (struct pollfd*)alloca(npfd * sizeof(struct pollfd));
    snd_seq_poll_descriptors(seq, pfd, npfd, POLLIN);

    // Start Audio Thread
    pthread_t audio_thread;
    AudioContext audio_ctx = { audio_device, pcm_handle };
    if (pcm_handle != NULL) {
        pthread_create(&audio_thread, NULL, audio_thread_func, &audio_ctx);
    }

    printf("\n>>> ENGINE READY! PLAY ON THE MPC KEY 61 KEYBOARD! (Press Ctrl+C to exit) <<<\n\n");

    // MIDI Event Processing Loop
    while (g_running) {
        if (poll(pfd, npfd, 50) <= 0) continue;

        while (snd_seq_event_input_pending(seq, 1) > 0) {
            snd_seq_event_t* ev = NULL;
            int status = snd_seq_event_input(seq, &ev);
            if (status < 0 || ev == NULL) break;

            // Process message for our sound engine
            switch (ev->type) {
                case SND_SEQ_EVENT_NOTEON: {
                    int note = ev->data.note.note;
                    int vel = ev->data.note.velocity;
                    if (vel > 0) {
                        printf("[MIDI IN] NoteOn:  %-3s (%3d) | Vel: %3d | Ch: %2d  ---> [SOUND] Piano Trigger  | [THRU] -> DIN Port A\n",
                               note_name(note), note, vel, ev->data.note.channel + 1);
                        synth_note_on(&g_synth, note, vel);
                    } else {
                        printf("[MIDI IN] NoteOff: %-3s (%3d) | Vel:   0 | Ch: %2d  ---> [SOUND] Piano Release  | [THRU] -> DIN Port A\n",
                               note_name(note), note, ev->data.note.channel + 1);
                        synth_note_off(&g_synth, note);
                    }
                    break;
                }
                case SND_SEQ_EVENT_NOTEOFF: {
                    int note = ev->data.note.note;
                    printf("[MIDI IN] NoteOff: %-3s (%3d) | Ch: %2d         ---> [SOUND] Piano Release  | [THRU] -> DIN Port A\n",
                           note_name(note), note, ev->data.note.channel + 1);
                    synth_note_off(&g_synth, note);
                    break;
                }
                case SND_SEQ_EVENT_CONTROLLER: {
                    int param = ev->data.control.param;
                    int val = ev->data.control.value;
                    if (param == 64) { // Sustain pedal
                        bool pedal_down = (val >= 64);
                        printf("[MIDI IN] Sustain Pedal: %s (CC 64 = %d)\n", pedal_down ? "ON (DOWN)" : "OFF (UP)", val);
                        synth_set_sustain(&g_synth, pedal_down);
                    } else {
                        printf("[MIDI IN] Controller: CC %3d = %3d\n", param, val);
                    }
                    break;
                }
                case SND_SEQ_EVENT_PITCHBEND: {
                    float bend = (float)ev->data.control.value / 8192.0f; // -1.0 to +1.0
                    printf("[MIDI IN] Pitch Bend: %+.2f\n", bend);
                    synth_set_pitch_bend(&g_synth, bend);
                    break;
                }
                case SND_SEQ_EVENT_CHANPRESS: {
                    printf("[MIDI IN] Channel Aftertouch: %d\n", ev->data.control.value);
                    break;
                }
                default:
                    break;
            }

            // Forward event directly to Out port -> MPC MIDI Port A (DIN OUT)
            snd_seq_ev_set_source(ev, out_port);
            snd_seq_ev_set_subs(ev);
            snd_seq_ev_set_direct(ev);
            snd_seq_event_output_direct(seq, ev);

            snd_seq_free_event(ev);
        }
    }

    printf("\nShutting down engine...\n");

    if (pcm_handle != NULL) {
        pthread_join(audio_thread, NULL);
        snd_pcm_drain(pcm_handle);
        snd_pcm_close(pcm_handle);
    }

    if (seq != NULL) {
        snd_seq_close(seq);
    }

    printf("Cleanup complete. Goodbye!\n");
    return 0;
}
