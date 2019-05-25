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

#include <cstdlib>
#include <errno.h>
#include <fcntl.h>
#include <mntent.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <unistd.h>

#include "newbs-swdl.h"

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
    log_info("Program raw part type %s (%s) to %s",
             part_name_from_type((nimg_ptype_e)p->type), human_bytes(p->size), dev.c_str());
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
    log_info("Program part type %s (%s) to %s",
             part_name_from_type((nimg_ptype_e)p->type), human_bytes(p->size), bootdir.c_str());

    int pfd[2];
    if (pipe2(pfd, O_CLOEXEC) == -1)
        THROW_ERRNO("pipe failed");

    pid_t tar_pid = fork();
    if (tar_pid == -1)
        THROW_ERRNO("fork failed");

    if (tar_pid == 0)
    {
        // child process
        dup2(pfd[0], STDIN_FILENO); // redirect stdin to read end of pipe

        // build tar arguments
        vector<const char*> args{"tar", "-x"};
        if (p->type == NIMG_PTYPE_BOOT_TARGZ)
            args.push_back("-z");
        else if (p->type == NIMG_PTYPE_BOOT_TARXZ)
            args.push_back("-J");
        args.push_back("-C");
        args.push_back(bootdir.c_str());
        args.push_back(NULL);

        do_exec(args);
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
        throw PError("%s\nFailed to program boot files! YOUR BOARD MAY NOT BOOT!", e.what());
    }

    close(pfd[1]);
    CPipe tar_cp = { .pid = tar_pid, .fd = -1, .running = true };
    cpipe_wait(tar_cp, true);

    if (crc != p->crc32)
        THROW_ERROR("CRC mismatch! expected 0x%08x, actual 0x%08x", p->crc32, crc);

    log_info("Finished programming part %s", part_name_from_type((nimg_ptype_e)p->type));
}

static void program_boot_img(const CPipe& curl, const nimg_phdr_t *p)
{
    struct mntent bootmnt = {};
    bool was_mounted = find_mntent(g_opts.boot_dev, &bootmnt);
    if (was_mounted)
    {
        log_info("unmounting %s", bootmnt.mnt_dir);
        if (umount(bootmnt.mnt_dir) < 0)
            THROW_ERRNO("Failed to unmount boot device %s", bootmnt.mnt_dir);
    }

    // program_raw might throw an exception, which we have to catch so we can clean up
    // (i.e. maybe remount /boot)
    string err_msg;
    try
    {
        if (p->type == NIMG_PTYPE_BOOT_IMG)
        {
            // directly flash uncompressed image
            program_raw(curl, p, g_opts.boot_dev);
        }
        else
        {
            // image is compressed, fork to decompressor and write
            const char *decompressor = NULL;
            switch (p->type)
            {
                case NIMG_PTYPE_BOOT_IMG_GZ:
                    decompressor = "gzip";
                    break;
                case NIMG_PTYPE_BOOT_IMG_XZ:
                    decompressor = "xz";
                    break;
                case NIMG_PTYPE_BOOT_IMG_ZSTD:
                    decompressor = "zstd";
                    break;
                default:
                    break;
            }
            if (!decompressor) // if this fails, the switch above is incomplete
                throw PError("BUG! No decompressor found for part type %s", part_name_from_type((nimg_ptype_e)p->type));

            log_info("Program compressed raw part type %s (%s) to %s",
                     part_name_from_type((nimg_ptype_e)p->type), human_bytes(p->size), g_opts.boot_dev.c_str());

            int pfd[2];
            if (pipe2(pfd, O_CLOEXEC) == -1)
                THROW_ERRNO("pipe failed");

            pid_t dec_pid = fork();
            if (dec_pid == -1)
                THROW_ERRNO("fork failed");

            if (dec_pid == 0)
            {
                // child process, open the device for writing as stdout and exec our decompressor
                dup2(pfd[0], STDIN_FILENO); // redirect stdin to read end of pipe
                int dev_fd = open(g_opts.boot_dev.c_str(), O_WRONLY | O_CLOEXEC);
                if (dev_fd < 1)
                {
                    fprintf(stderr, "Failed to open %s for writing: %s\n", g_opts.boot_dev.c_str(), strerror(errno));
                    _exit(98);
                }
                dup2(dev_fd, STDOUT_FILENO); // redirect stdout to device we just opened

                do_exec(vector<const char*>{decompressor, "-dc", NULL});
            }

            // main process
            log_debug("spawned child decompressor process PID %d", dec_pid);
            close(pfd[0]); // close read end of pipe
            uint32_t crc;
            try { crc = file_copy_crc32_progress(curl.fd, pfd[1], p->size); }
            catch (exception& e)
            {
                close(pfd[1]);
                kill(dec_pid, SIGKILL);
                int wstatus;
                waitpid(dec_pid, &wstatus, 0);
                throw PError("%s\nFailed to program boot image! YOUR BOARD MAY NOT BOOT!", e.what());
            }

            close(pfd[1]);
            CPipe dec_cp = { .pid = dec_pid, .fd = -1, .running = true};
            cpipe_wait(dec_cp, true);
            if (crc != p->crc32)
                THROW_ERROR("CRC mismatch! expected 0x%08x, actual 0x%08x", p->crc32, crc);
            log_info("Finished programming part %s", part_name_from_type((nimg_ptype_e)p->type));
        }
    }
    catch (exception& e) { err_msg += e.what(); }

    // try to remount
    if (was_mounted)
    {
        try
        {
            log_info("remounting %s on %s", bootmnt.mnt_fsname, bootmnt.mnt_dir);
            mount_mntent(&bootmnt);
        }
        catch (exception& e)
        {
            // combine the mount error with the program_raw error
            if (err_msg.length() > 0)
                err_msg += '\n';
            err_msg += e.what();
        }

        // clean up copied mntent strings
        free(bootmnt.mnt_fsname);
        free(bootmnt.mnt_dir);
        free(bootmnt.mnt_type);
        free(bootmnt.mnt_opts);
    }

    if (err_msg.length() > 0)
        throw PError(err_msg);

    log_info("Finished programming boot image");
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
        case NIMG_PTYPE_BOOT_IMG_GZ:
        case NIMG_PTYPE_BOOT_IMG_XZ:
        case NIMG_PTYPE_BOOT_IMG_ZSTD:
            program_boot_img(curl, p);
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
