#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <alsa/asoundlib.h>

static volatile int running = 1;
void sig_handler(int sig) { (void)sig; running = 0; }

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    signal(SIGINT, sig_handler);
    signal(SIGALRM, sig_handler);

    snd_rawmidi_t *midi_in0 = NULL, *midi_out0 = NULL;
    snd_rawmidi_t *midi_in1 = NULL, *midi_out1 = NULL;
    snd_rawmidi_t *midi_in2 = NULL, *midi_out2 = NULL;

    int err0 = snd_rawmidi_open(&midi_in0, &midi_out0, "hw:0,0,0", SND_RAWMIDI_NONBLOCK);
    int err1 = snd_rawmidi_open(&midi_in1, &midi_out1, "hw:0,0,1", SND_RAWMIDI_NONBLOCK);
    int err2 = snd_rawmidi_open(&midi_in2, &midi_out2, "hw:0,0,2", SND_RAWMIDI_NONBLOCK);

    printf("snd_rawmidi_open:\n");
    printf("  hw:0,0,0 (Public):  err=%d (%s)\n", err0, err0 >= 0 ? "SUCCESS" : snd_strerror(err0));
    printf("  hw:0,0,1 (Private): err=%d (%s)\n", err1, err1 >= 0 ? "SUCCESS" : snd_strerror(err1));
    printf("  hw:0,0,2 (Port A):  err=%d (%s)\n", err2, err2 >= 0 ? "SUCCESS" : snd_strerror(err2));

    // Send Device Inquiry to both hw:0,0,0 and hw:0,0,1
    uint8_t inquiry[] = { 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 };
    if (midi_out0) snd_rawmidi_write(midi_out0, inquiry, sizeof(inquiry));
    if (midi_out1) snd_rawmidi_write(midi_out1, inquiry, sizeof(inquiry));

    uint8_t akai_mode[] = { 0xF0, 0x47, 0x7F, 0x4B, 0x60, 0x00, 0x04, 0x40, 0x00, 0x00, 0x00, 0xF7 };
    if (midi_out0) snd_rawmidi_write(midi_out0, akai_mode, sizeof(akai_mode));
    if (midi_out1) snd_rawmidi_write(midi_out1, akai_mode, sizeof(akai_mode));

    printf("Sent Inquiry and Akai Mode to all open ports. Listening for 5 seconds...\n");

    alarm(5);
    uint8_t buf[256];
    while (running) {
        if (midi_in0) {
            ssize_t n = snd_rawmidi_read(midi_in0, buf, sizeof(buf));
            if (n > 0) {
                printf("[hw:0,0,0 Public] %zd bytes: ", n);
                for (ssize_t i = 0; i < n; ++i) printf("%02X ", buf[i]);
                printf("\n");
            }
        }
        if (midi_in1) {
            ssize_t n = snd_rawmidi_read(midi_in1, buf, sizeof(buf));
            if (n > 0) {
                printf("[hw:0,0,1 Private] %zd bytes: ", n);
                for (ssize_t i = 0; i < n; ++i) printf("%02X ", buf[i]);
                printf("\n");
            }
        }
        if (midi_in2) {
            ssize_t n = snd_rawmidi_read(midi_in2, buf, sizeof(buf));
            if (n > 0) {
                printf("[hw:0,0,2 Port A] %zd bytes: ", n);
                for (ssize_t i = 0; i < n; ++i) printf("%02X ", buf[i]);
                printf("\n");
            }
        }
        usleep(5000);
    }

    if (midi_in0) snd_rawmidi_close(midi_in0);
    if (midi_out0) snd_rawmidi_close(midi_out0);
    if (midi_in1) snd_rawmidi_close(midi_in1);
    if (midi_out1) snd_rawmidi_close(midi_out1);
    if (midi_in2) snd_rawmidi_close(midi_in2);
    if (midi_out2) snd_rawmidi_close(midi_out2);

    printf("Closed all MIDI devices cleanly.\n");
    return 0;
}
