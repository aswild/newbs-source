#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "spi/neostrip.h"

#define DEV_FILE "/dev/neostrip0"

#define TO_RGB(r, g, b) ((((r) & 0xff) << 16) | \
                         (((g) & 0xff) << 8)  | \
                         ((b) & 0xff))

enum {
    CMD_HELLO,
    CMD_READ,
    CMD_WRITE,
    CMD_RAINBOW,
};

const char *cmd_strings[] = {
    "hello",
    "read",
    "write",
    "rainbow",
};

int get_cmd(const char *buf)
{
    size_t i;
    for (i = 0; i < sizeof(cmd_strings)/sizeof(cmd_strings[0]); i++)
    {
        // check for exact match
        if (!strcasecmp(buf, cmd_strings[i]))
            return i;

        // check for partial match
        if (!strncasecmp(buf, cmd_strings[i], strlen(buf)))
            return i;
    }

    // not found
    return -1;
}

static int write_color(int fd, uint32_t val)
{
    int i;
    struct neostrip_ioc_data ioc_data = {
        .offset = 0,
        .count = 8,
    };

    ioc_data.pixels = (uint32_t*)malloc(8 * sizeof(uint32_t));
    for (i = 0; i < 8; i++)
        ioc_data.pixels[i] = val;

    return ioctl(fd, NEOSTRIP_IOC_WRITE, &ioc_data);
}

static int write_color_rgb(int fd, int r, int g, int b)
{
    if (r < 0)
        r = 0;
    else if (r > 0xff)
        r = 0xff;

    if (g < 0)
        g = 0;
    else if (g > 0xff)
        g = 0xff;

    if (b < 0)
        b = 0;
    else if (b > 0xff)
        b = 0xff;

    uint32_t color = TO_RGB(r, g, b);
    return write_color(fd, color);
}

// rainbow states
enum {
    R2Y,
    Y2G,
    G2C,
    C2B,
    B2M,
    M2R,
};

void rainbow(int fd)
{
    int r = 0xff;
    int g = 0;
    int b = 0;
    int state = R2Y;

    const int step = 2;
    const useconds_t sleep_time_us = 10000;

    write_color_rgb(fd, r, g, b);
    while (1)
    {
        switch (state)
        {
            case R2Y:
                if ((g+=step) >= 0xff)
                    state = Y2G;
                break;
            case Y2G:
                if ((r-=step) <= 0)
                    state = G2C;
                break;
            case G2C:
                if ((b+=step) >= 0xff)
                    state = C2B;
                break;
            case C2B:
                if ((g-=step) <= 0)
                    state = B2M;
                break;
            case B2M:
                if ((r+=step) >= 0xff)
                    state = M2R;
                break;
            case M2R:
                if ((b-=step) <= 0)
                    state = R2Y;
                break;
            default:
                printf("error: unknown state (%d)\n", state);
                return;
                break;
        }
        write_color_rgb(fd, r, g, b);
        usleep(sleep_time_us);
    }
}

int main(int argc, char *argv[])
{
    int fd;
    int ret;
    int cmd;
    uint32_t val;

    if (argc < 2)
    {
        printf("need a command!\n");
        return 1;
    }

    if ((cmd = get_cmd(argv[1])) < 0)
    {
        printf("unknown command: '%s'\n", argv[1]);
        return 1;
    }

    if (argc > 2)
    {
        val = strtoul(argv[2], NULL, 16);
    }
    else
    {
        val = 0xa1e600;
    }

    if ((fd = open(DEV_FILE, O_RDONLY)) < 0)
    {
        printf("Can't open '%s'\n", DEV_FILE);
        return 1;
    }

    switch (cmd)
    {
        case CMD_HELLO:
            ret = ioctl(fd, NEOSTRIP_IOC_HELLO, val);
            printf("returned %d\n", ret);
            break;

        case CMD_READ:
            printf("not implemented\n");
            break;

        case CMD_WRITE:
            ret = write_color(fd, val);
            break;

        case CMD_RAINBOW:
            rainbow(fd);
            break;
    }

    close(fd);

    return 0;
}
