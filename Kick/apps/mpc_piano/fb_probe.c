#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

int main() {
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) { perror("open /dev/fb0"); return 1; }
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    ioctl(fd, FBIOGET_VSCREENINFO, &vinfo);
    ioctl(fd, FBIOGET_FSCREENINFO, &finfo);
    printf("FB: xres=%d, yres=%d, vxres=%d, vyres=%d, bpp=%d\n",
           vinfo.xres, vinfo.yres, vinfo.xres_virtual, vinfo.yres_virtual, vinfo.bits_per_pixel);
    printf("Fix: line_length=%d, smem_len=%d, id=%s\n",
           finfo.line_length, finfo.smem_len, finfo.id);
    printf("RGBA: R offset=%d len=%d, G offset=%d len=%d, B offset=%d len=%d, A offset=%d len=%d\n",
           vinfo.red.offset, vinfo.red.length,
           vinfo.green.offset, vinfo.green.length,
           vinfo.blue.offset, vinfo.blue.length,
           vinfo.transp.offset, vinfo.transp.length);
    close(fd);
    return 0;
}
