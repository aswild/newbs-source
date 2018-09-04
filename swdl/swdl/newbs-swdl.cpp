/*******************************************************************************
 * Copyright (C) 2018 Allen Wild <allenwild93@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 ******************************************************************************/

#include <cstdio>
#include <unistd.h>

#include "newbs-swdl.h"

SwdlOptions g_opts;

static void usage(const char *arg0)
{
    const char *progname = arg0 ? arg0 : "newbs-swdl";
    const char msg[] =
        "Usage: %s [OPTIONS...] FILE\n"
        "Options:\n"
        "  -h   Show this help text.\n"
        "  -D   Enable debug logginc.\n"
        "  -q   Be more quiet.\n"
        "  -t   Toggle active rootfs bank if rootfs part is in image (default).\n"
        "  -T   Do not toggle active rootfs bank (opposite of -t).\n"
        "  -r   Reboot after download.\n"
        "  -R   Do not reboot after download (default).\n"
        "\n"
        "FILE:  Filename or URL to download\n";
    printf(msg, progname);
}

int main(int argc, char *argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "hDqtTrR")) != -1)
    {
        switch (opt)
        {
            case 'h':
                usage(argv[0]);
                return 0;
            case 'D':
                log_level = LOG_LEVEL_DEBUG;
                break;
            case 'q':
                log_level = LOG_LEVEL_ERROR;
                break;
            case 't':
                g_opts.toggle = true;
                break;
            case 'T':
                g_opts.toggle = false;
                break;
            case 'r':
                g_opts.reboot = true;
                break;
            case 'R':
                g_opts.reboot = false;
                break;

            default:
                usage(argv[0]);
                return 2;
        }
    }
    if (argc < (optind + 1))
    {
        log_error("Missing FILE argument");
        usage(argv[0]);
        return 2;
    }
    else if (argc > (optind + 1))
    {
        log_error("Too many arguments");
        usage(argv[0]);
        return 2;
    }

    string url(argv[optind]);
    //log_info("toggle = %d, reboot = %d, url = %s", g_opts.toggle, g_opts.reboot, url.c_str());

#if 0
    try
    {
        stringvec cmdline = split_words_in_file("cmdline.txt");
        printf("cmdline split:\n");
        //dump_vec(cmdline, std::cout);
        printf("%s\n", join_words(cmdline, " ").c_str());
        printf("cmdline split done\n");
    }
    catch (std::exception& e)
    {
        log_error("Caught exception: %s", e.what());
    }
#endif

    CPipe curl = open_curl(url);
    bool err = false;
    try
    {
        log_info("starting read");
        while (true)
        {
            char buf[4096];
            ssize_t nread = cpipe_read(curl, buf, sizeof(buf));
            if (nread > 0)
                write(1, buf, nread);
            else
                break;
        }
        cpipe_wait(curl, true);
    }
    catch (exception& e)
    {
        log_error("failed to download: %s", e.what());
        err = true;
    }

    log_info("done");

    return err ? 1 : 0;
}
