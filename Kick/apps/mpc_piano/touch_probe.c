#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/input.h>

int main() {
    int fd = open("/dev/input/event0", O_RDONLY);
    if (fd < 0) { perror("open event0"); return 1; }
    struct input_absinfo abs_x, abs_y;
    ioctl(fd, EVIOCGABS(ABS_X), &abs_x);
    ioctl(fd, EVIOCGABS(ABS_Y), &abs_y);
    printf("Touch ABS_X: min=%d max=%d\n", abs_x.minimum, abs_x.maximum);
    printf("Touch ABS_Y: min=%d max=%d\n", abs_y.minimum, abs_y.maximum);
    struct input_absinfo abs_mt_x, abs_mt_y;
    ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &abs_mt_x);
    ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &abs_mt_y);
    printf("Touch MT_X:  min=%d max=%d\n", abs_mt_x.minimum, abs_mt_x.maximum);
    printf("Touch MT_Y:  min=%d max=%d\n", abs_mt_y.minimum, abs_mt_y.maximum);
    close(fd);
    return 0;
}
