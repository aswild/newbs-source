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

#include "mknImage.h"

void cmd_help_crc32(void)
{
    static const char msg[] =
        "    usage: mknImage crc32 FILE [SIZE]\n"
        "    Prints the crc32 of FILE in 0x00000000 format\n"
        "    FILE: can be '-' to use stdin\n"
        "    SIZE: checksum first SIZE bytes\n"
    "";
    fputs(msg, stdout);
}

int cmd_crc32(int argc, char **argv)
{
    if (argc < 2)
        DIE_USAGE("crc32 command requires an argument");

    const char *filename = argv[1];
    FILE *fp = open_file(filename, "r");
    if (fp == NULL)
        DIE_ERRNO("Failed to open file %s", filename);

    long crc_len = -1;
    if (argc > 2)
    {
        if ((check_strtol(argv[2], 0, &crc_len) < 0) || (crc_len <= 0))
            DIE("Invalid SIZE argument '%s'", argv[2]);
    }

    const size_t buf_size = 8192;
    uint8_t *buf = malloc(buf_size);
    assert(buf != NULL);

    uint32_t crc = 0;
    size_t total_read = 0;
    while (true)
    {
        ssize_t to_read = (crc_len > 0) ? MIN(buf_size, crc_len - total_read) : buf_size;
        ssize_t nread = fread(buf, 1, to_read, fp);
        if (nread <= 0)
            break;
        xcrc32(&crc, buf, nread);
        total_read += nread;
    }
    free(buf);
    bool read_error = ((crc_len < 0) && !feof(fp)) ||
                       ((crc_len > 0) && ((ssize_t)total_read < crc_len));
    fclose(fp);
    if (read_error)
        DIE("Failed to read file %s. Expected %ld bytes but got only %zu", filename, crc_len, total_read);

    printf("0x%08x\n", crc);

    return 0;
}
