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
#include <string.h>
#include "newbs-util.h"

extern int newbs_parse_conf(int argc, char **argv);
extern int newbs_reboot(int argc, char **argv);

const newbs_cmd_t commands[] = {
    {"parse",   &newbs_parse_conf},
    {"reboot",  &newbs_reboot},
    {NULL, NULL}
};

static const newbs_cmd_t * get_cmd(const newbs_cmd_t *cmd_list, const char *str)
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
    fprintf(fp, "Usage: newbs-util.c COMMAND [ARGS...]\nAvailable Commands:\n");
    print_commands(fp, commands);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        ERROR("No command");
        usage(stderr);
        return 1;
    }

    const newbs_cmd_t *cmd = get_cmd(commands, argv[1]);
    if (!cmd)
    {
        ERROR("Invalid command");
        usage(stderr);
        return 1;
    }

    //return ((*int)(int,char**)(cmd->handler))(argc - 1, &argv[1]);
    return cmd->handler(argc-1, &argv[1]);
}
