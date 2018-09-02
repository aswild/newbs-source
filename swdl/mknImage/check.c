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
#include <fcntl.h>
#include <unistd.h>

#include "mknImage.h"

void cmd_help_check(void)
{
    static const char msg[] =
        "    Inspect and check an nImage.\n"
        "    usage: mknImage check IMAGE_FILE\n"
        "    IMAGE_FILE can be a filename or - for stdin\n"
    "";
    fputs(msg, stdout);
}

int cmd_check(int argc, char **argv)
{
    int ret = 1;
    bool nonfatal_err = false; // found a non-fatal error. keep checking but return failure at the end

    if (argc != 2)
        DIE_USAGE("Wrong number of arguments for check");

    int fd = -1;
    if (!strcmp(argv[1], "-"))
        fd = STDIN_FILENO;
    else
        fd = open(argv[1], O_RDONLY);
    if (fd == -1)
        DIE_ERRNO("Unable to open '%s' for reading", argv[1]);

    log_info("Checking image %s", argv[1]);

    nimg_hdr_t hdr;
    if (read_n(fd, &hdr, NIMG_HDR_SIZE) < NIMG_HDR_SIZE)
    {
        if (errno)
            DIE_ERRNO("Failed to read image header");
        else
            DIE("Failed to read image header: unexpected EOF");
    }

    nimg_hdr_check_e hcheck = nimg_hdr_check(&hdr);
    if (hcheck != NIMG_HDR_CHECK_SUCCESS)
    {
        log_error("Invalid image header: %s", nimg_hdr_check_str(hcheck));
        if (hcheck == NIMG_HDR_CHECK_BAD_CRC)
            nonfatal_err = true; // continue if everything but the CRC is ok
        else
            goto out; // everything else is fatal
    }

    log_info("Image Name:      %.*s", NIMG_NAME_LEN, hdr.name);
    log_info("Image Magic:     0x%016llx", (unsigned long long)hdr.magic);
    log_info("Image Version:   %u", hdr.version);
    log_info("Number of Parts: %u", hdr.n_parts);
    log_info("Header CRC32:    0x%08x", hdr.hdr_crc32);
    if (hcheck == NIMG_HDR_CHECK_BAD_CRC)
        log_error("Header CRC32 is invalid!");

    uint64_t parts_bytes = 0;
    for (int i = 0; i < hdr.n_parts; i++)
    {
        nimg_phdr_t *p = &hdr.parts[i];
        log_info("Part %d", i);
        print_part_info(p, "  ", stdout);

        size_t padding = p->offset - parts_bytes;
        if (padding > 0)
        {
            char pbuf[padding];
            if (read_n(fd, pbuf, padding) < padding)
            {
                log_error("failed to read %zu inter-image padding bytes", padding);
                if (errno)
                    log_error("%s", strerror(errno));
                goto out;
            }
        }

        uint32_t crc = 0;
        if (file_copy_crc32(&crc, (long)p->size, fd, -1) != (ssize_t)p->size)
        {
            log_error("failed to read image data: %s", strerror(errno));
            goto out;
        }
        parts_bytes += p->size;

        if (crc != p->crc32)
        {
            log_error("CRC32 Mismatch! expected 0x%08x, got 0x%08x", p->crc32, crc);
            nonfatal_err = true;
        }
    }

    if (!nonfatal_err)
    {
        log_info("Image checked SUCCESS");
        ret = 0;
    }
    else
        log_info("Image check FAILURE");
out:
    if (fd != -1)
        close(fd);
    return ret;
}
