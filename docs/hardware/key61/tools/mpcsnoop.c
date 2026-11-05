// SPDX-License-Identifier: AGPL-3.0-or-later
// LD_PRELOAD observer for the stock MPC application: logs every MIDI byte it writes to the control surface, with a
// timestamp, to /tmp/mpc_midi.log. Read-only observation (the call is forwarded unchanged). It wraps libasound's
// snd_rawmidi_open / snd_rawmidi_open_lconf / snd_rawmidi_write and the plain write()/writev() on /dev/snd/midi* fds.
// Build (armv7, in the kick-armv7-builder image):
//   arm-linux-gnueabihf-gcc -shared -fPIC -O1 -o mpcsnoop.so mpcsnoop.c -ldl
// Use on the MPC (the stock app only, Kick stopped):
//   LD_PRELOAD=/data/mpcsnoop.so setarch -R /usr/bin/MPC
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>

typedef struct _snd_rawmidi snd_rawmidi_t;
typedef struct _snd_config snd_config_t;

static FILE* logf_(void)
{
    static FILE* f = NULL;
    if (!f) { f = fopen("/tmp/mpc_midi.log", "a"); if (f) setvbuf(f, NULL, _IOLBF, 0); }
    return f;
}

static void stamp(FILE* f)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    fprintf(f, "%ld.%03ld ", (long)t.tv_sec, t.tv_nsec / 1000000);
}

static void hexdump(const char* tag, const void* p, size_t n)
{
    FILE* f = logf_();
    if (!f) return;
    stamp(f);
    fprintf(f, "%s len=%zu:", tag, n);
    const unsigned char* b = (const unsigned char*)p;
    for (size_t i = 0; i < n && i < 256; ++i) fprintf(f, " %02X", b[i]);
    fprintf(f, "\n");
}

int snd_rawmidi_open(snd_rawmidi_t** in, snd_rawmidi_t** out, const char* name, int mode)
{
    int (*real)(snd_rawmidi_t**, snd_rawmidi_t**, const char*, int) = dlsym(RTLD_NEXT, "snd_rawmidi_open");
    int r = real(in, out, name, mode);
    FILE* f = logf_();
    if (f) { stamp(f); fprintf(f, "OPEN %s in=%p out=%p r=%d\n", name, in ? (void*)*in : NULL, out ? (void*)*out : NULL, r); }
    return r;
}

int snd_rawmidi_open_lconf(snd_rawmidi_t** in, snd_rawmidi_t** out, const char* name, int mode, snd_config_t* lconf)
{
    int (*real)(snd_rawmidi_t**, snd_rawmidi_t**, const char*, int, snd_config_t*) = dlsym(RTLD_NEXT, "snd_rawmidi_open_lconf");
    int r = real(in, out, name, mode, lconf);
    FILE* f = logf_();
    if (f) { stamp(f); fprintf(f, "OPEN_LCONF %s in=%p out=%p r=%d\n", name, in ? (void*)*in : NULL, out ? (void*)*out : NULL, r); }
    return r;
}

ssize_t snd_rawmidi_write(snd_rawmidi_t* h, const void* buf, size_t n)
{
    ssize_t (*real)(snd_rawmidi_t*, const void*, size_t) = dlsym(RTLD_NEXT, "snd_rawmidi_write");
    char tag[48];
    snprintf(tag, sizeof tag, "WRITE h=%p", (void*)h);
    hexdump(tag, buf, n);
    return real(h, buf, n);
}

static int is_midi_fd(int fd)
{
    char link[64], path[128];
    snprintf(link, sizeof link, "/proc/self/fd/%d", fd);
    ssize_t k = readlink(link, path, sizeof path - 1);
    if (k <= 0) return 0;
    path[k] = 0;
    return strstr(path, "/dev/snd/midi") != NULL;
}

ssize_t write(int fd, const void* buf, size_t n)
{
    ssize_t (*real)(int, const void*, size_t) = dlsym(RTLD_NEXT, "write");
    if (fd > 2 && is_midi_fd(fd)) { char tag[32]; snprintf(tag, sizeof tag, "FDWRITE fd=%d", fd); hexdump(tag, buf, n); }
    return real(fd, buf, n);
}

ssize_t writev(int fd, const struct iovec* iov, int cnt)
{
    ssize_t (*real)(int, const struct iovec*, int) = dlsym(RTLD_NEXT, "writev");
    if (fd > 2 && is_midi_fd(fd)) {
        for (int i = 0; i < cnt; ++i) { char tag[32]; snprintf(tag, sizeof tag, "FDWRITEV fd=%d", fd); hexdump(tag, iov[i].iov_base, iov[i].iov_len); }
    }
    return real(fd, iov, cnt);
}
