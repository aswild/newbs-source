/*******************************************************************************
 * Copyright 2017 Allen Wild <allenwild93@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *******************************************************************************/

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "newbs-util.h"

bool debug_enabled = false;

const newbs_cmd_t commands[] = {
    {"act",     &newbs_run_action},
    {"reboot",  &newbs_reboot},
    {"dump",    &newbs_dump_config},
    {NULL, NULL}
};

static const newbs_cmd_t* get_cmd(const newbs_cmd_t *cmd_list, const char *str)
{
    if (!cmd_list || !str)
        return NULL;

    const newbs_cmd_t *cmd = cmd_list;
    const newbs_cmd_t *partial_match = NULL;
    bool found_match = false;
    while (!found_match && cmd->name)
    {
        if (!strcasecmp(str, cmd->name))
        {
            found_match = true;
            break;
        }
        else if (!strncasecmp(str, cmd->name, strlen(str)))
        {
            if (partial_match)
            {
                ERROR("Ambiguous command");
                return NULL;
            }
            partial_match = cmd;
        }
        cmd++;
    }

    if (found_match)
        return cmd;
    else if (partial_match)
        return partial_match;
    return NULL;
}

static inline void print_commands(FILE *fp, const newbs_cmd_t *cmd_list)
{
    for (; cmd_list && cmd_list->name; cmd_list++)
        fprintf(fp, "    %s\n", cmd_list->name);
}

static inline void usage(FILE *fp)
{
    fprintf(fp, "Usage: newbs-util.c [-d] COMMAND [ARGS...]\nAvailable Commands:\n");
    print_commands(fp, commands);
}

int main(int argc, char *argv[])
{
    int c;
    while ((c = getopt(argc, argv, "d")) != -1)
    {
        switch (c)
        {
            case 'd':
                debug_enabled = true;
                break;

            default:
                ERROR("Invalid option '-%c'", c);
                return 1;
        }
    }

    DEBUG("optind is %d", optind);

    if (argc < 1)
    {
        ERROR("No command");
        usage(stderr);
        return 1;
    }

    const newbs_cmd_t *cmd = get_cmd(commands, argv[optind]);
    if (!cmd)
    {
        ERROR("Invalid command");
        usage(stderr);
        return 1;
    }

    return cmd->handler(argc - optind - 1, &argv[optind+1]);
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
