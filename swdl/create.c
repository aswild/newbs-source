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
#include <string.h>
#include <assert.h>

#include "mknImage.h"

void cmd_help_create(void)
{
    static const char msg[] =
        "    Create an nImage.\n"
        "    usage: mknImage create IMAGE_FILE TYPE1:FILE1 [TYPE2:FILE2]...\n"
        "      FILE:  Output image file, can be '-' for stdout\n"
        "      TYPEn: Image type. Valid options are: kernel, boot, rootfs, rootfs_rw\n"
        "      FILEn: Input partition data filename\n"
        "    Type kernel is the bare Linux kernel image. Type boot is the full boot partition\n"
    "";
    fputs(msg, stdout);
}

int cmd_create(int argc, char **argv)
{
    return 0;
}
