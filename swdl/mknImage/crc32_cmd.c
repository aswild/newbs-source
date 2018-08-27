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
#include <fcntl.h>
#include <unistd.h>

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

    int fd;
    if (!strcmp(filename, "-"))
        fd = STDIN_FILENO;
    else
        fd = open(filename, O_RDONLY);
    if (fd < 0)
        DIE_ERRNO("Failed to open file '%s' for reading", filename);

    long len = -1;
    if (argc > 2)
    {
        if ((check_strtol(argv[2], 0, &len) < 0) || (len <= 0))
            DIE("Invalid SIZE argument '%s'", argv[2]);
    }

    uint32_t crc = 0;
    ssize_t count = file_copy_crc32(&crc, len, fd, -1);
    close(fd);

    if (count < 0)
        DIE_ERRNO("Failed to read from file %s", filename);
    if ((len > 0) && (count != (ssize_t)len))
        DIE("Failed to read file '%s'. Expected %ld bytes but got only %ld", filename, len, (long)count);

    printf("0x%08x\n", crc);

    return 0;
}
