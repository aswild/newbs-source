#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "Neostrip.h"
#include "utils.h"

#define N_PIXELS 8
Neostrip strip(N_PIXELS);

enum {
    CMD_HELLO,
    CMD_READ,
    CMD_WRITE,
    CMD_SRAINBOW, // old static rainbow
    CMD_RAINBOW,
};

const char *cmd_strings[] = {
    "hello",
    "read",
    "write",
    "srainbow",
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

// rainbow states
enum {
    R2Y,
    Y2G,
    G2C,
    C2B,
    B2M,
    M2R,
};

void srainbow(void)
{
    int r = 0xff;
    int g = 0;
    int b = 0;
    int state = R2Y;

    const int step = 2;
    const useconds_t sleep_time_us = 10000;

    while (1)
    {
        switch (state)
        {
            case R2Y:
                if ((g+=step) >= 0xff)
                {
                    g = 0xff;
                    state = Y2G;
                }
                break;
            case Y2G:
                if ((r-=step) <= 0)
                {
                    r = 0;
                    state = G2C;
                }
                break;
            case G2C:
                if ((b+=step) >= 0xff)
                {
                    b = 0xff;
                    state = C2B;
                }
                break;
            case C2B:
                if ((g-=step) <= 0)
                {
                    g = 0;
                    state = B2M;
                }
                break;
            case B2M:
                if ((r+=step) >= 0xff)
                {
                    r = 0xff;
                    state = M2R;
                }
                break;
            case M2R:
                if ((b-=step) <= 0)
                {
                    b = 0;
                    state = R2Y;
                }
                break;
            default:
                printf("error: unknown state (%d)\n", state);
                return;
                break;
        }
        strip.set_all_pixels(TO_RGB(r, g, b));
        strip.write();
        usleep(sleep_time_us);
    }
}

void rainbow(void)
{
    const useconds_t sleep_time_us = 10000;
    const float dh = 360.0 / N_PIXELS;
    float basehue = 0;

    while (1)
    {
        for (int i = 0; i < N_PIXELS; i++)
            strip.set_pixel(i, hue_to_rgb((dh * i) - basehue));

        if (++basehue > 360)
            basehue = 0;

        strip.write();
        usleep(sleep_time_us);
    }
}

int main(int argc, char *argv[])
{
    int ret = 0;
    int cmd, opt;
    uint32_t val;

    while ((opt = getopt(argc, argv, "s:")) != -1)
    {
        switch (opt)
        {
            case 's':
                {
                    float scale;
                    if (!sscanf(optarg, "%f", &scale))
                    {
                        PRINT_ERR("invalid scale: '%s'\n", optarg);
                        return -1;
                    }
                    strip.set_scale(scale);
                }
                break;
            default:
                PRINT_ERR("unknown option: '%c'", opt);
                return -1;
                break;
        }
    }

    if (argc - optind < 1)
    {
        printf("need a command!\n");
        return 1;
    }

    if ((cmd = get_cmd(argv[optind])) < 0)
    {
        printf("unknown command: '%s'\n", argv[1]);
        return 1;
    }

    if (argc - optind > 1)
    {
        val = strtoul(argv[optind+1], NULL, 16);
    }
    else
    {
        val = 0xa1e600;
    }

    strip.open_fd();

    switch (cmd)
    {
        case CMD_READ:
            printf("not implemented\n");
            break;

        case CMD_WRITE:
            strip.set_all_pixels(val);
            ret = strip.write();
            break;

        case CMD_SRAINBOW:
            srainbow();
            break;

        case CMD_RAINBOW:
            rainbow();
            break;
    }

    strip.close_fd();
    return ret;
}
