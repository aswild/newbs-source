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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define NIMG_DECLARE_PTYPE_NAMES
#include "nImage.h"

#define BLOCK_SIZE ((size_t)16384)

nimg_ptype_e part_type_from_name(const char *name)
{
    for (int i = 0; i < NIMG_PTYPE_COUNT; i++)
        if (!strcmp(nimg_ptype_names[i], name))
            return (nimg_ptype_e)i;
    return NIMG_PTYPE_INVALID;
}

const char* part_name_from_type(nimg_ptype_e id)
{
    if (id > NIMG_PTYPE_LAST)
        return NULL;
    return nimg_ptype_names[id];
}

void nimg_hdr_init(nimg_hdr_t *h)
{
    memset(h, 0, sizeof(*h));
    h->magic = NIMG_HDR_MAGIC;
    h->version = NIMG_HDR_VERSION;
}

void print_part_info(nimg_phdr_t *p, const char *prefix, FILE *fp)
{
    if (prefix == NULL)
        prefix = "";
    fprintf(fp, "%stype:   %s\n",          prefix, part_name_from_type(p->type));
    fprintf(fp, "%ssize:   %s (%lu, 0x%lx)\n",
            prefix, human_bytes(p->size), (unsigned long)p->size, (unsigned long)p->size);
    fprintf(fp, "%soffset: %s (%lu, 0x%lx)\n",
            prefix, human_bytes(p->offset), (unsigned long)p->offset, (unsigned long)p->offset);
    fprintf(fp, "%scrc32:  0x%x\n",        prefix, p->crc32);
}

nimg_hdr_check_e nimg_hdr_check(const nimg_hdr_t *h)
{
    if (h->magic != NIMG_HDR_MAGIC)
        return NIMG_HDR_CHECK_BAD_MAGIC;
    if (h->version < NIMG_HDR_VERSION_MIN_SUPPORTED || h->version > NIMG_HDR_VERSION_MAX_SUPPORTED)
        return NIMG_HDR_CHECK_BAD_VERSION;
    if (h->n_parts > NIMG_MAX_PARTS)
        return NIMG_HDR_CHECK_TOO_MANY_PARTS;

    uint32_t crc = 0;
    xcrc32(&crc, (const uint8_t*)h, NIMG_HDR_SIZE-4);
    if (h->hdr_crc32 != crc)
        return NIMG_HDR_CHECK_BAD_CRC;

    return NIMG_HDR_CHECK_SUCCESS;
}

nimg_phdr_check_e nimg_phdr_check(const nimg_phdr_t *h, uint8_t hdr_version)
{
    if (h->magic != NIMG_PHDR_MAGIC)
        return NIMG_PHDR_CHECK_BAD_MAGIC;
    if (h->type > NIMG_PTYPE_LAST)
        return NIMG_PHDR_CHECK_BAD_TYPE;
    if (hdr_version < 2 && h->type > NIMG_PTYPE_ROOTFS_RW)
        return NIMG_PHDR_CHECK_WRONG_VERSION;

    return NIMG_PHDR_CHECK_SUCCESS;
}

const char* nimg_hdr_check_str(nimg_hdr_check_e status)
{
    switch (status)
    {
        case NIMG_HDR_CHECK_SUCCESS:
            return "Success";
        case NIMG_HDR_CHECK_BAD_MAGIC:
            return "Invalid header magic";
        case NIMG_HDR_CHECK_BAD_VERSION:
            return "Invalid nImage version";
        case NIMG_HDR_CHECK_TOO_MANY_PARTS:
            return "Too many partitions in image";
        case NIMG_HDR_CHECK_BAD_CRC:
            return "Invalid header CRC32";
    }
    return NULL;
}

const char* nimg_phdr_check_str(nimg_phdr_check_e status)
{
    switch (status)
    {
        case NIMG_PHDR_CHECK_SUCCESS:
            return "Success";
        case NIMG_PHDR_CHECK_BAD_MAGIC:
            return "Invalid part header magic";
        case NIMG_PHDR_CHECK_BAD_TYPE:
            return "Invalid part type";
        case NIMG_PHDR_CHECK_WRONG_VERSION:
            return "Part type not supported in nImage version";
    }
    return NULL;
}

/* Copy len bytes from fd_in to fd_out, calculating the CRC32 along the way.
 * If len is negative, read until EOF.
 * If fd_out is -1, don't copy, just read and CRC.
 * Returns the number of bytes copied, -1 on read error, or -2 on write error.
 * The caller should initialize crc to 0 or some other starting value
 */
ssize_t file_copy_crc32(uint32_t *crc, ssize_t len, int fd_in, int fd_out)
{
    uint8_t *buf = malloc(BLOCK_SIZE);
    assert(buf != NULL);

    ssize_t total_read = 0;
    while ((len < 0) || (total_read != len))
    {
        size_t to_read = (len > 0) ? min(BLOCK_SIZE, (size_t)(len - total_read)) : BLOCK_SIZE;
        ssize_t nread = read(fd_in, buf, to_read);
        if (nread < 0)
        {
            // read error
            total_read = -1;
            break;
        }
        else if (nread == 0)
            break; // EOF

        if (fd_out != -1)
        {
            if (write(fd_out, buf, nread) != nread)
            {
                // write error
                total_read = -2;
                break;
            }
        }

        xcrc32(crc, buf, nread);
        total_read += nread;
    }

    free(buf);
    return total_read;
}

/* Copy len bytes from fd_in, pipe through compressor program, calculating CRC32 of
 * compressed data along the way.
 * crc, len, fd_in, and fd_out work as in file_copy_crc32, except that len must be
 * positive (read to EOF isn't supported).
 * compress_prog is the compressor program (probably "gzip" or "xz").
 * the size of compressed data written to fd_out is returned through compressed_size
 * Returns the number of bytes read from fd_in, which is always len on success, or -1
 * on failure.
 */
ssize_t file_copy_crc32_compress(uint32_t *crc, ssize_t len, int fd_in, int fd_out,
                                 const char *compressor, size_t *compressed_size)
{
    // true streaming would make this function even more complicated, so cheat
    // by buffering all the file data first
    uint8_t *data = malloc(len);
    assert(data != NULL);

    ssize_t read_from_in = read_n(fd_in, data, len);
    if (read_from_in != len)
    {
        free(data);
        return -1;
    }

    int inpipe[2]; // pipe from fd_in to the compressor (buffered by us)
    if (pipe2(inpipe, O_NONBLOCK | O_CLOEXEC) < 0)
    {
        log_error("inpipe pipe() failed: %s", strerror(errno));
        free(data);
        return -1;
    }

    int outpipe[2]; // pipe from the compressor to fd_out (buffered and crc'd by us)
    if (pipe2(outpipe, O_NONBLOCK | O_CLOEXEC) < 0)
    {
        log_error("outpipe pipe() failed: %s", strerror(errno));
        close(inpipe[0]); close(inpipe[1]);
        free(data);
        return -1;
    }

    pid_t cpid = fork();
    if (cpid < 0)
    {
        log_error("fork() failed: %s", strerror(errno));
        close(inpipe[0]); close(inpipe[1]);
        close(outpipe[0]); close(outpipe[1]);
        free(data);
        return -1;
    }
    else if (cpid == 0)
    {
        // child process
        dup2(inpipe[0], STDIN_FILENO);
        dup2(outpipe[1], STDOUT_FILENO);
        execlp(compressor, compressor, NULL);
        fprintf(stderr, "execlp failed to run '%s': %s", compressor, strerror(errno));
        _exit(99);
    }

    // main process
    close(inpipe[0]);
    close(outpipe[1]);

    uint8_t *buf = malloc(BLOCK_SIZE);
    assert(buf != NULL);
    ssize_t comp_read = 0;
    ssize_t comp_written = 0;
    bool success = false;

    while (true)
    {
        // write as much as we can to the pipe
        while (comp_written < len)
        {
            const size_t to_write = min(BLOCK_SIZE, (size_t)(len - comp_written));
            const ssize_t nwritten = write(inpipe[1], data + comp_written, to_write);
            if (nwritten > 0)
            {
                // write success, keep going
                //log_debug("wrote %zd bytes to compressor", nwritten);
                comp_written += nwritten;
                if (comp_written == len)
                {
                    log_debug("finished writing to compressor");
                    // change the compressor read pipe to blocking mode so
                    // that the read loop waits for compression to finish
                    int flags = fcntl(outpipe[0], F_GETFL);
                    flags &= ~O_NONBLOCK;
                    fcntl(outpipe[0], F_SETFL, flags);

                    // close the compressor's input so it knows to finish.
                    // set to -1 so we don't try to close it again below
                    close(inpipe[1]);
                    inpipe[1] = -1;
                }
            }
            else if (errno == EAGAIN)
                break; // write would block, done for now
            else
            {
                log_error("write to compressor pipe failed: %s", strerror(errno));
                goto done_error;
            }
        }

        // read as much as we can from the pipe and write it to fd_out
        while (true)
        {
            const ssize_t nread = read(outpipe[0], buf, BLOCK_SIZE);
            if (nread > 0)
            {
                //log_debug("read %zd bytes from compressor", nread);
                // write that to fd_out. Assume we can write it all at once to avoid another loop
                if (write(fd_out, buf, nread) != nread)
                {
                    log_error("write failed: %s", strerror(errno));
                    goto done_error;
                }
                xcrc32(crc, buf, nread);
                comp_read += nread;
            }
            else if ((nread == 0) || (errno == EAGAIN))
            {
                // EOF or read would block
                if (comp_written < len)
                    break; // not done writing, do more of that
                else
                    goto done; // EOF and we done writing to the compressor
            }
            else
            {
                log_error("read from compressor pipe failed: %s", strerror(errno));
                goto done_error;
            }
        }
    }

done:
    success = true;
done_error:
    free(data);
    free(buf);
    close(outpipe[0]);
    if (inpipe[1] != -1)
        close(inpipe[1]);

    log_debug("compressor: wrote %zd bytes, read %zd bytes", comp_written, comp_read);

    int wstatus;
    pid_t waitret = waitpid(cpid, &wstatus, 0);
    if (waitret > 0)
    {
        if (WIFEXITED(wstatus))
        {
            int status = WEXITSTATUS(wstatus);
            if (status == 0)
                log_debug("compressor process %d exited successfully", cpid);
            else
            {
                log_error("compressor process %d exited non-zero (%d)", cpid, status);
                success = false;
            }
        }
        else if (WIFSIGNALED(wstatus))
        {
            log_error("child process %d killed by signal %d", cpid, WTERMSIG(wstatus));
            success = false;
        }
        else
            log_warn("compressor process %d exited, but not normally or by signal???", cpid);
    }

    *compressed_size = (size_t)comp_read;
    return success ? len : -1;
}

// check the weird error handling of strtol, returning 0 or negative
// and storing the parsed value into *value.
// Based on the example code in `man 3 strtol`
int check_strtol(const char *str, int base, long *value)
{
    char *endptr = NULL;

    errno = 0;
    *value = strtol(str, &endptr, base);
    return (errno || endptr == str) ? -1 : 0;
}

// read count bytes from fd into buf, retrying indefinitely as long as we get
// at least one byte.
// If read returns 0, we assume EOF and set errno to 0.
// If read return < 0, we assume error and that errno is set by libc
size_t read_n(int fd, void *buf, size_t count)
{
    char *cbuf = (char*)buf;
    size_t total = 0;
    while (total < count)
    {
        ssize_t n = read(fd, cbuf, count - total);
        if (n < 1)
        {
            if (n == 0)
                errno = 0;
            break;
        }
        total += n;
        cbuf += n;
    }
    return total;
}

// format a number of bytes into a human-readable format
// returns a pointer to a static buffer
const char* human_bytes(size_t s)
{
    static char buf[32];
    const char *suffix;
    int div;
    if (s > (1<<30))
    {
        suffix = "GB";
        div = 1<<30;
    }
    else if (s > (1<<20))
    {
        suffix = "MB";
        div = 1<<20;
    }
    else if (s > (1<<10))
    {
        suffix = "KB";
        div = 1<<10;
    }
    else
    {
        // special case for no div, don't do floating-point stuff
        snprintf(buf, sizeof(buf), "%zu bytes", s);
        return buf;
    }

    snprintf(buf, sizeof(buf), "%.2f %s", ((double)s / (double)div), suffix);
    return buf;
}
