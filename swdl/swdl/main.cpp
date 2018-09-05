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
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
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
        "  -c   cmdline.txt location (default /boot/cmdline.txt).\n"
        "\n"
        "FILE:  Filename or URL to download\n";
    printf(msg, progname);
}

int main(int argc, char *argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "hDqtTrRc:")) != -1)
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
            case 'c':
                g_opts.cmdline_txt = optarg;
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
    string url = argv[optind];

    // done with argument parsing, time to do stuff
    CPipe curl;
    int err = 0;
    try
    {
        // parse cmdline.txt (will use it later)
        stringvec cmdline = split_words_in_file(g_opts.cmdline_txt);

        // fork off to curl to download the image
        curl = open_curl(url);

        // read the image header
        nimg_hdr_t hdr;
        ssize_t nread;
        try { nread = cpipe_read(curl, &hdr, NIMG_HDR_SIZE); }
        catch (exception& e) { log_error("failed to read image header"); throw; }

        // validate the header
        nimg_hdr_check_e hdr_check = nimg_hdr_check(&hdr);
        if (hdr_check != NIMG_HDR_CHECK_SUCCESS)
            throw PError("nImage header validation failed: %s", nimg_hdr_check_str(hdr_check));

        uint64_t parts_bytes = 0;
        for (int i = 0; i < hdr.n_parts; i++)
        {
            nimg_phdr_t *p = &hdr.parts[i];
            ssize_t padding = p->offset - parts_bytes;
            if (padding < 0)
                throw PError("bad offset for part %d. offset=%llu but parts_read=%llu",
                             i, (unsigned long long)p->offset, (unsigned long long)parts_bytes);
            if (padding > 0)
            {
                char pbuf[padding];
                if (cpipe_read(curl, pbuf, padding) < (size_t)padding)
                    throw PError("failed to read %zu padding bytes before part %d", padding, i);
            }

            // this does the real work, and throws an exception for any failure
            program_part(curl, p, cmdline);
            parts_bytes += p->size;
        }

        // finished programming, see if we need to flip banks
        // 0 = no bank flip, 1 = flip to ro rootfs, 2 = flip to rw rootfs
        int flip_bank = 0;
        for (int i = 0; i < hdr.n_parts; i++)
        {
            if (hdr.parts[i].type == NIMG_PTYPE_ROOTFS)
                flip_bank = 1;
            else if (hdr.parts[i].type == NIMG_PTYPE_ROOTFS_RW)
                flip_bank = 2;
        }
        if (flip_bank)
        {
            cmdline_flip_bank(cmdline, flip_bank == 2);
            string cmdline_txt_old = g_opts.cmdline_txt + ".old";
            log_debug("backing up old %s as %s", g_opts.cmdline_txt.c_str(), cmdline_txt_old.c_str());
            if (rename(g_opts.cmdline_txt.c_str(), cmdline_txt_old.c_str()) != 0)
                log_error("failed to rename %s to %s: %s",
                          g_opts.cmdline_txt.c_str(), cmdline_txt_old.c_str(), strerror(errno));

            string new_cmdline = join_words(cmdline, " ");
            log_debug("writing new cmdline '%s'", new_cmdline.c_str());

            // use low level C APIs because magic C++ streams may throw exceptions and make it hard to use errno
            new_cmdline += '\n';
            int fd_write = open(g_opts.cmdline_txt.c_str(), O_WRONLY | O_TRUNC | O_CREAT, 0666);
            if (fd_write == -1)
                THROW_ERRNO("failed to open %s for writing", g_opts.cmdline_txt.c_str());
            ssize_t nwritten = write(fd_write, new_cmdline.c_str(), new_cmdline.length());
            close(fd_write);
            if (nwritten != (ssize_t)new_cmdline.length())
                THROW_ERRNO("failed to write to %s", g_opts.cmdline_txt.c_str());
        }
    }
    catch (exception& e)
    {
        log_error("%s", e.what());
        if (curl.running)
            kill(curl.pid, SIGTERM);
        err++;
    }

    // clean up
    if (curl.fd != -1)
        close(curl.fd);
    try { cpipe_wait(curl, true); }
    catch (exception& e)
    {
        // if there was an error above, we killed curl so don't complain about that
        if (!err)
            log_error("image download failed: %s", e.what());
        err++;
    }

    log_debug("syncing filesystems");
    sync();

    if (!err)
        log_info("newbs-swdl completed SUCCESS");
    else
        log_error("newbs_swdl completed FAILURE");
    return err;
}
