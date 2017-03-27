#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "Neostrip.h"

Neostrip strip(8);

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

// rainbow states
enum {
    R2Y,
    Y2G,
    G2C,
    C2B,
    B2M,
    M2R,
};

void rainbow(void)
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
        strip.set_all_pixels(r, g, b);
        strip.write();
        usleep(sleep_time_us);
    }
}

int main(int argc, char *argv[])
{
    int ret = 0;
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

        case CMD_RAINBOW:
            rainbow();
            break;
    }

    strip.close_fd();
    return ret;
}
