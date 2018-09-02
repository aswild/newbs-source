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

#ifndef MKNIMAGE_H
#define MKNIMAGE_H

#include "nImage.h"

typedef struct {
    const char    *name;
    int(*handler)(int argc, char **argv);
    void(*help_func)(void);
} cmd_t;

// command handlers with a side of macro magic
#define CMD_LIST(xform) \
    xform(create) \
    xform(check) \
    xform(crc32)

#define DECLARE_CMD_HANDLERS(name) \
    extern int  cmd_##name(int argc, char **argv); \
    extern void cmd_help_##name(void);

CMD_LIST(DECLARE_CMD_HANDLERS)

#define DECLARE_CMD_DATA(name) {#name, cmd_##name, cmd_help_##name},
#define DECLARE_CMD_TABLE(table_name) \
        cmd_t table_name[] = { \
            CMD_LIST(DECLARE_CMD_DATA) \
            {NULL, NULL, NULL} }

#endif // MKNIMAGE_H
