/*******************************************************************************
 * Copyright (C) 2018-2019 Allen Wild <allenwild93@gmail.com>
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

static void print_version(void)
{
    fputs("newbs-swdl version " PACKAGE_VERSION "\n", stdout);
}

static void usage(const char *arg0)
{
    const char *progname = arg0 ? arg0 : "newbs-swdl";
    const char msg[] =
        "Usage: %s [OPTIONS...] FILE\n"
        "Options:\n"
        "  -h   Show this help text.\n"
        "  -V   Show program version.\n"
        "  -D   Enable debug logginc.\n"
        "  -q   Be more quiet.\n"
        "  -t   Flip rootfs bank if rootfs part is in image (default).\n"
        "  -r   Flip rootfs bank an reboot after download.\n"
        "  -T   Do not flip rootfs bank or reboot.\n"
        "  -b   boot device node (used for debugging, probably a loop device.\n"
        "       When using a loop device, run losetup manually so the loop isn't\n"
        "       automatically removed when swdl unmounts it.\n"
        "  -c   cmdline.txt location (used for debugging).\n"
        "\n"
        "FILE:  Filename or URL to download. Use '-' for stdin.\n";
    print_version();
    printf(msg, progname);
}

int main(int argc, char *argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "hVDqtrTb:c:")) != -1)
    {
        switch (opt)
        {
            case 'h':
                usage(argv[0]);
                return 0;
            case 'V':
                print_version();
                return 0;
            case 'D':
                log_level = LOG_LEVEL_DEBUG;
                break;
            case 'q':
                log_level = LOG_LEVEL_ERROR;
                break;
            case 't':
                g_opts.success_action = SwdlOptions::FLIP;
                break;
            case 'r':
                g_opts.success_action = SwdlOptions::FLIP_REBOOT;
                break;
            case 'T':
                g_opts.success_action = SwdlOptions::NO_FLIP;
                break;
            case 'b':
#ifdef SWDL_TEST
                if (strncmp(optarg, "/dev/loop", strlen("/dev/loop")))
                {
                    log_error("When compiled with SWDL_TEST, the boot device must be /dev/loopX");
                    return 2;
                }
#endif
                g_opts.boot_dev = optarg;
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
        // fork off to curl to download the image
        curl = open_curl(url);

        // ignore SIGPIPE so that we can handle errors when writes fail.
        // This can happen when programming a corrupted tar part because
        // tar will exit and close the pipe we're writing to.
        // As long as we always check the return code of write(), this should be safe.
        signal(SIGPIPE, SIG_IGN);

        // read the image header
        nimg_hdr_t hdr;
        try { cpipe_read(curl, &hdr, NIMG_HDR_SIZE); }
        catch (exception& e) { log_error("failed to read image header"); throw; }

        // validate the header
        nimg_hdr_check_e hdr_check = nimg_hdr_check(&hdr);
        if (hdr_check != NIMG_HDR_CHECK_SUCCESS)
            throw PError("nImage header validation failed: %s", nimg_hdr_check_str(hdr_check));

        log_info("Image name is %.*s", NIMG_NAME_LEN, hdr.name[0] ? hdr.name : "(empty)");

        if (hdr.n_parts == 0)
        {
            log_warn("No partitions in image, nothing to do!");
            throw SuccessException();
        }

        // get the inactive rootfs device based on what's running unless we're testing
#ifdef SWDL_TEST
        stringvec cmdline = split_words_in_file(g_opts.cmdline_txt);
#else
        stringvec cmdline = split_words_in_file("/proc/cmdline");
#endif

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

        if (g_opts.success_action == SwdlOptions::NO_FLIP)
        {
            log_info("not flipping banks or rebooting");
            throw SuccessException();
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
            // load whatever cmdline.txt we just programmed and update the rootfs bank
            stringvec new_cmdline = split_words_in_file(g_opts.cmdline_txt);
            cmdline_set_root(new_cmdline, get_inactive_dev(cmdline), flip_bank == 2);

            string cmdline_txt_old = g_opts.cmdline_txt + ".old";
            log_debug("backing up old %s as %s", g_opts.cmdline_txt.c_str(), cmdline_txt_old.c_str());
            if (rename(g_opts.cmdline_txt.c_str(), cmdline_txt_old.c_str()) != 0)
                log_error("failed to rename %s to %s: %s",
                          g_opts.cmdline_txt.c_str(), cmdline_txt_old.c_str(), strerror(errno));

            string new_cmdline_s = join_words(new_cmdline, " ");
            log_debug("writing new cmdline '%s'", new_cmdline_s.c_str());
            new_cmdline_s += '\n'; // add the trailing newline after the debug log

            // use low level C APIs because magic C++ streams may throw exceptions and make it hard to use errno
            int fd_write = open(g_opts.cmdline_txt.c_str(), O_WRONLY | O_TRUNC | O_CREAT, 0666);
            if (fd_write == -1)
                THROW_ERRNO("failed to open %s for writing", g_opts.cmdline_txt.c_str());
            ssize_t nwritten = write(fd_write, new_cmdline_s.c_str(), new_cmdline_s.length());
            close(fd_write);
            if (nwritten != (ssize_t)new_cmdline_s.length())
                THROW_ERRNO("failed to write to %s", g_opts.cmdline_txt.c_str());
        }
        else
        {
            log_info("no rootfs download, bank flip not needed");
        }
    }
    catch (SuccessException&) { /* no-op */ }
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

    log_info("syncing filesystems");
    sync();

    if (err)
    {
        log_error("newbs_swdl completed FAILURE");
    }
    else
    {
        log_info("newbs-swdl completed SUCCESS");
        if (g_opts.success_action == SwdlOptions::FLIP_REBOOT)
        {
            const int reboot_wait_sec = 5;
            log_info("Reboot in %d seconds. Press Ctrl-C to cancel", reboot_wait_sec);
            sleep(5); // default SIGINT handler here will kill the process
#ifdef SWDL_TEST
            log_info("SWDL test enabled, not actually rebooting!");
#else
            log_info("Rebooting now!");
            if (geteuid() == 0)
                system("reboot"); // system command because I'm lazy
            else
                system("sudo reboot");
#endif
        }
    }

    return err;
}
