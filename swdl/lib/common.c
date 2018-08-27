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

#include "nImage.h"

#define BUF_SIZE ((size_t)8192)

typedef struct {
    nimg_ptype_e id;
    const char *name;
} ptype_info_t;

static const ptype_info_t ptype_table[] = {
    { NIMG_PART_TYPE_KERNEL,    "kernel" },
    { NIMG_PART_TYPE_BOOT,      "boot" },
    { NIMG_PART_TYPE_ROOTFS,    "rootfs" },
    { NIMG_PART_TYPE_ROOTFS_RW, "rootfs_rw" },
    { 0, NULL },
};

nimg_ptype_e part_type_from_name(const char *name)
{
    for (const ptype_info_t *p = ptype_table; p->name != NULL; p++)
        if (!strcasecmp(name, p->name))
            return p->id;
    return NIMG_PART_TYPE_INVALID;
}

const char * part_name_from_type(nimg_ptype_e id)
{
    for (const ptype_info_t *p = ptype_table; p->name != NULL; p++)
        if (id == p->id)
            return p->name;
    return NULL;
}

void nimg_hdr_init(nimg_hdr_t *h)
{
    memset(h, 0, sizeof(*h));
    h->magic = NIMG_HDR_MAGIC;
    h->ver_major = NIMG_VER_MAJOR;
    h->ver_minor = NIMG_VER_MINOR;
}

/* Copy len bytes from fp_in to fp_out, calculating the CRC32 along the way.
 * If len is negative, read until EOF.
 * Returns the number of bytes copied.
 * The caller should initialize crc to 0 or some other starting value
 */
size_t file_copy_crc32(uint32_t *crc, long len, FILE *fp_in, FILE *fp_out)
{
    uint8_t *buf = malloc(BUF_SIZE);
    assert(buf != NULL);

    size_t total_read = 0;
    while (true)
    {
        ssize_t to_read = (len > 0) ? MIN(BUF_SIZE, len - total_read) : BUF_SIZE;
        ssize_t nread = fread(buf, 1, to_read, fp_in);
        if (nread <= 0)
            break; // read error or EOF

        if (fwrite(buf, 1, nread, fp_out) != (size_t)nread)
            break; // write error

        xcrc32(crc, buf, nread);
        total_read += nread;
    }

    free(buf);
    return total_read;
}

/* Update the CRC32 for crc_len bytes from fp.
 * If crc_len is negative, read until EOF (i.e. fread returns <= 0).
 * Returns the number of bytes crc'd.
 * The caller should initialize crc to 0 or some other starting value
 */
size_t file_crc32(uint32_t *crc, long len, FILE *fp)
{
    uint8_t *buf = malloc(BUF_SIZE);
    assert(buf != NULL);

    size_t total_read = 0;
    while (true)
    {
        ssize_t to_read = (len > 0) ? MIN(BUF_SIZE, len - total_read) : BUF_SIZE;
        ssize_t nread = fread(buf, 1, to_read, fp);
        if (nread <= 0)
            break;
        xcrc32(crc, buf, nread);
        total_read += nread;
    }
    free(buf);
    return total_read;
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
