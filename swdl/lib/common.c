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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#define NIMG_DECLARE_PTYPE_NAMES
#include "nImage.h"

#define BUF_SIZE ((size_t)8192)

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
    fprintf(fp, "%ssize:   %lu (0x%lx)\n", prefix, (unsigned long)p->size, (unsigned long)p->size);
    fprintf(fp, "%soffset: %lu (0x%lx)\n", prefix, (unsigned long)p->offset, (unsigned long)p->offset);
    fprintf(fp, "%scrc32:  0x%x\n",        prefix, p->crc32);
}

nimg_hdr_check_e nimg_hdr_check(const nimg_hdr_t *h)
{
    if (h->magic != NIMG_HDR_MAGIC)
        return NIMG_HDR_CHECK_BAD_MAGIC;
    if (h->version != NIMG_HDR_VERSION)
        return NIMG_HDR_CHECK_BAD_VERSION;
    if (h->n_parts > NIMG_MAX_PARTS)
        return NIMG_HDR_CHECK_TOO_MANY_PARTS;

    uint32_t crc = 0;
    xcrc32(&crc, (const uint8_t*)h, NIMG_HDR_SIZE-4);
    if (h->hdr_crc32 != crc)
        return NIMG_HDR_CHECK_BAD_CRC;

    return NIMG_HDR_CHECK_SUCCESS;
}

nimg_phdr_check_e nimg_phdr_check(const nimg_phdr_t *h)
{
    if (h->magic != NIMG_PHDR_MAGIC)
        return NIMG_PHDR_CHECK_BAD_MAGIC;
    if (h->type > NIMG_PTYPE_LAST)
        return NIMG_PHDR_CHECK_BAD_TYPE;

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
    }
    return NULL;
}

/* Copy len bytes from fd_in to fd_out, calculating the CRC32 along the way.
 * If len is negative, read until EOF.
 * If fd_out is -1, don't copy, just read and CRC.
 * Returns the number of bytes copied, -1 on read error, or -2 on write error.
 * The caller should initialize crc to 0 or some other starting value
 */
ssize_t file_copy_crc32(uint32_t *crc, long len, int fd_in, int fd_out)
{
    uint8_t *buf = malloc(BUF_SIZE);
    assert(buf != NULL);

    ssize_t total_read = 0;
    while ((len < 0) || (total_read != len))
    {
        size_t to_read = (len > 0) ? min(BUF_SIZE, (size_t)(len - total_read)) : BUF_SIZE;
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

FILE * open_file(const char *name, const char *mode)
{
    if (!strcmp(name, "-"))
        return (mode[0] == 'r') ? stdin : stdout;
    return fopen(name, mode);
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
