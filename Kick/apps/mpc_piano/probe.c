#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

static volatile int running = 1;
void sig_handler(int sig) { (void)sig; running = 0; }

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    signal(SIGINT, sig_handler);
    signal(SIGALRM, sig_handler);

    int fd0 = open("/dev/snd/midiC0D0", O_RDWR | O_NONBLOCK);
    int fd1 = open("/dev/snd/midiC0D1", O_RDWR | O_NONBLOCK);
    int fd2 = open("/dev/snd/midiC0D2", O_RDWR | O_NONBLOCK);

    printf("Opened: midiC0D0=%d (%s), midiC0D1=%d (%s), midiC0D2=%d (%s)\n",
           fd0, fd0 >= 0 ? "ok" : strerror(errno),
           fd1, fd1 >= 0 ? "ok" : strerror(errno),
           fd2, fd2 >= 0 ? "ok" : strerror(errno));

    // Send Device Inquiry on fd1
    if (fd0 >= 0) {
        // 1. Device Inquiry
        uint8_t inquiry[] = { 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 };
        write(fd0, inquiry, sizeof(inquiry));

        // 2. Akai MPC Mode Initialization command (0x60)
        uint8_t akai_init1[] = { 0xF0, 0x47, 0x7F, 0x4B, 0x60, 0x00, 0x04, 0x40, 0x00, 0x00, 0x00, 0xF7 };
        write(fd0, akai_init1, sizeof(akai_init1));

        uint8_t akai_init2[] = { 0xF0, 0x47, 0x7F, 0x4B, 0x60, 0x00, 0x04, 0x42, 0x00, 0x00, 0x00, 0xF7 };
        write(fd0, akai_init2, sizeof(akai_init2));

        printf("Sent Device Inquiry & Akai Mode 0x60 Inits to midiC0D0\n");
    }
    printf("Listening for 10 seconds... Please press keys, pads, and wheels!\n");

    alarm(10); // Run for 10 seconds max

    uint8_t buf[256];
    while (running) {
        if (fd0 >= 0) {
            int n = read(fd0, buf, sizeof(buf));
            if (n > 0) {
                printf("[midiC0D0 (Public)] %d bytes: ", n);
                for (int i = 0; i < n; ++i) printf("%02X ", buf[i]);
                printf("\n");
                fflush(stdout);
            }
        }
        if (fd1 >= 0) {
            int n = read(fd1, buf, sizeof(buf));
            if (n > 0) {
                printf("[midiC0D1 (Private)] %d bytes: ", n);
                for (int i = 0; i < n; ++i) printf("%02X ", buf[i]);
                printf("\n");
                fflush(stdout);
            }
        }
        if (fd2 >= 0) {
            int n = read(fd2, buf, sizeof(buf));
            if (n > 0) {
                printf("[midiC0D2 (Port A)] %d bytes: ", n);
                for (int i = 0; i < n; ++i) printf("%02X ", buf[i]);
                printf("\n");
                fflush(stdout);
            }
        }
        usleep(5000); // 5ms sleep
    }

    if (fd0 >= 0) close(fd0);
    if (fd1 >= 0) close(fd1);
    if (fd2 >= 0) close(fd2);
    printf("Exiting probe.\n");
    return 0;
}
