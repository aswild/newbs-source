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
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "mknImage.h"

#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "1.0-noautoconf"
#endif

static const DECLARE_CMD_TABLE(cmd_table);

static const char usage_text[] =
    "usage: mknImage [OPTIONS] COMMAND [ARGUMENTS]\n"
    "\n"
    "OPTIONS:\n"
    " -h  Show this help text\n"
    " -V  Show program version\n"
    " -D  Enable verbose debug outpus\n"
    " -q  Be more quiet\n"
"";

static void print_version(void)
{
    fputs("mknImage version " PACKAGE_VERSION "\n", stdout);
}

static void usage(void)
{
    print_version();
    printf("%s\nCOMMANDS:\n", usage_text);
    for (const cmd_t *cmd = cmd_table; cmd->name != NULL; cmd++)
    {
        printf("  %s:\n", cmd->name);
        cmd->help_func();
        if ((cmd+1)->name != NULL)
            putchar('\n');
    }
}

static const cmd_t * find_cmd(const char *name)
{
    for (const cmd_t *cmd = cmd_table; cmd->name != NULL; cmd++)
        if (!strcasecmp(name, cmd->name))
            return cmd;
    return NULL;
}

int main(int argc, char *argv[])
{
    int opt;
    // start the optstring with + to disable automatic argument re-ordering,
    // getopt stops as soon as it finds a non-option argument so that
    // commands can take options too.
    while ((opt = getopt(argc, argv, "+hVDq")) != -1)
    {
        switch (opt)
        {
            case 'h':
                usage();
                exit(0);
                break;
            case 'V':
                print_version();
                exit(0);
                break;
            case 'D':
                log_level = LOG_LEVEL_DEBUG;
                break;
            case 'q':
                log_level = LOG_LEVEL_ERROR;
                break;
            default:
                DIE_USAGE("unknown option '%c'", opt);
                break;
        }
    }

    if (optind >= argc)
    {
        usage();
        exit(0);
    }

    argc -= optind;
    argv += optind;

    const cmd_t *cmd = find_cmd(argv[0]);
    if (cmd == NULL)
        DIE_USAGE("Unknown command %s", argv[0]);

    return cmd->handler(argc, argv);
}
