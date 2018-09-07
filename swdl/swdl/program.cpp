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

#include <cstdlib>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include "newbs-swdl.h"

// TODO: these should all be configurable instead of hard-coded
static inline string get_boot_dev(void)
{
#ifdef SWDL_TEST
    return "/dev/null";
#else
    return "/dev/mmcblk0p1";
#endif
}

static inline string _get_inactive_dev(const stringvec& cmdline)
{
#ifdef SWDL_TEST
    (void)cmdline;
    return "/dev/null";
#else
    return get_inactive_dev(cmdline);
#endif
}

static inline string get_boot_dir(void)
{
#ifdef SWDL_TEST
    char test_boot_dir[] = "./boot.XXXXXX"; // modified by mkdtemp, must be non-const
    if (mkdtemp(test_boot_dir) == NULL)
        THROW_ERRNO("mkdtemp failed for test boot directory");
    log_info("SWDL_TEST: using temporary directory %s as boot dir", test_boot_dir);
    return test_boot_dir;
#else
    return "/boot";
#endif
}

// copy between file descriptors, return the crc32, throw an exception if something goes wrong
// prints a . to stderr every chunk_size for progress, returns the crc32
static uint32_t file_copy_crc32_progress(int fd_in, int fd_out, size_t len)
{
    // read and copy block_size bytes at a time, print a progress dot every chunk_size bytes
    const size_t block_size = 8192;
    const size_t chunk_size = 1048576 * 2;

    uint8_t buf[block_size];
    uint32_t crc = 0;
    size_t total = 0, chunk_progress = 0;
    while (total < len)
    {
        size_t to_read = min(block_size, len - total);
        ssize_t nread = read(fd_in, buf, to_read);
        if (nread < 0)
            THROW_ERRNO("read failed");

        ssize_t written = 0;
        while (written < nread)
        {
            ssize_t nwrite = write(fd_out, buf+written, nread-written);
            if (nwrite <= 0)
                THROW_ERRNO("write failed");
            written += nwrite;
        }
        assert(written == nread); // this should never really fail

        xcrc32(&crc, buf, nread);
        total += nread;
        chunk_progress += nread;
        if (chunk_progress >= chunk_size)
        {
            fputc('.', stderr);
            chunk_progress = 0;
        }
    }
    fputc('\n', stderr);
    assert(total == len);
    return crc;
}

static void program_raw(const CPipe& curl, const nimg_phdr_t *p, const string& dev)
{
    log_info("Program raw part type %s (%zu bytes) to %s", part_name_from_type((nimg_ptype_e)p->type), (size_t)p->size, dev.c_str());
    int fd_out = open(dev.c_str(), O_WRONLY);
    if (fd_out == -1)
        THROW_ERRNO("Failed to open %s for writing", dev.c_str());

    uint32_t crc;
    try { crc = file_copy_crc32_progress(curl.fd, fd_out, p->size); }
    catch (exception& e) { close(fd_out); throw; }
    close(fd_out);

    if (crc != p->crc32)
        THROW_ERROR("CRC mismatch! expected 0x%08x, actual 0x%08x", p->crc32, crc);

    log_info("Finished programming part %s", part_name_from_type((nimg_ptype_e)p->type));
}

static void program_boot_tar(const CPipe& curl, const nimg_phdr_t *p, const string& bootdir)
{
    log_info("Program part type %s to %s", part_name_from_type((nimg_ptype_e)p->type), bootdir.c_str());

    int pfd[2];
    if (pipe(pfd) == -1)
        THROW_ERRNO("pipe failed");

    pid_t tar_pid = fork();
    if (tar_pid == -1)
        THROW_ERRNO("fork failed");

    if (tar_pid == 0)
    {
        // child process
        const char *argv[6];
        argv[0] = "tar";
        argv[1] = "-xf-";
        if (p->type == NIMG_PTYPE_BOOT_TARGZ)
            argv[2] = "-z";
        else if (p->type == NIMG_PTYPE_BOOT_TARXZ)
            argv[2] = "-J";
        else
            argv[2] = "--no-auto-compress";
        argv[3] = "-C";
        argv[4] = bootdir.c_str();
        argv[5] = NULL;

        close(pfd[1]); // close write end of pipe
        dup2(pfd[0], STDIN_FILENO); // redirect stdin to read end of pipe
        execvp(argv[0], (char *const *)argv);
        log_error("execvp failed: %s", strerror(errno));
        _exit(99);
    }

    // main process
    close(pfd[0]); // close read end of pipe
    uint32_t crc;
    try { crc = file_copy_crc32_progress(curl.fd, pfd[1], p->size); }
    catch (exception& e)
    {
        close(pfd[1]);
        kill(tar_pid, SIGKILL);
        int wstatus;
        waitpid(tar_pid, &wstatus, 0);
        log_error("Failed to program boot files! Your board may not boot!");
        throw;
    }

    close(pfd[1]);
    CPipe tar_cp = { .pid = tar_pid, .fd = -1, .running = true };
    cpipe_wait(tar_cp, true);

    if (crc != p->crc32)
        THROW_ERROR("CRC mismatch! expected 0x%08x, actual 0x%08x", p->crc32, crc);

    log_info("Finished programming part %s", part_name_from_type((nimg_ptype_e)p->type));
}

// program a partition with the given header and check the CRC
// throw an exception if anything goes wrong
void program_part(CPipe& curl, const nimg_phdr_t *p, const stringvec& cmdline)
{
    nimg_ptype_e type = static_cast<nimg_ptype_e>(p->type);
    if (type > NIMG_PTYPE_LAST)
        THROW_ERROR("invalid part type %u", type);

    log_info("program part type %s", part_name_from_type(type));
    switch (type)
    {
        case NIMG_PTYPE_BOOT_IMG:
            program_raw(curl, p, get_boot_dev());
            break;

        case NIMG_PTYPE_ROOTFS:
        case NIMG_PTYPE_ROOTFS_RW:
            program_raw(curl, p, _get_inactive_dev(cmdline));
            break;

        case NIMG_PTYPE_BOOT_TAR:
        case NIMG_PTYPE_BOOT_TARGZ:
        case NIMG_PTYPE_BOOT_TARXZ:
            program_boot_tar(curl, p, get_boot_dir());
            break;

        default:
            // shouldn't actually get here because we checked the part type
            // earlier, but adding these cases satisfies "enumeration values
            // not handled in switch" warnings
            THROW_ERROR("invalid part type in switch");
    }
}
