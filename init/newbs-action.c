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

// to get strcasestr
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include "newbs-util.h"

#define CMDLINE_ARG_SEARCH  "newbscmd="
#define MAX_CMDLINE_SIZE    1024
#define MAX_INPUT_LEN       64

const char *action_type_strs[ACTION_TYPE_LAST] = {
    "INVALID",
    "CONTINUE",
    "REBOOT",
    "RECOVERY",
    "CUSTOM"
};

typedef struct {
    const char      *name;
    char            key;
    const char      *action_str;
} static_action_t;

static const static_action_t static_actions[] = {
    {"Continue to NEWBS Core",  'c', "continue"},
    {"Drop to init shell",      'd', "recoveryshell"},
    {"Custom command",          'e',  NULL}, // special case, user will be prompted
    {NULL, 0, NULL},
};

static int set_option_action_str(newbs_option_t *opt)
{
    char *buf = NULL;

    switch (opt->type)
    {
        case ACTION_TYPE_CONTINUE:
            {
                size_t alloc_size = strlen("continue") + 1;
                if (opt->root)
                    alloc_size += strlen(opt->root) + 1; // :rootpath
                buf = malloc(alloc_size);
                if (!buf)
                {
                    ERROR("malloc for %zu bytes failed", alloc_size);
                    return -1;
                }
                if (opt->root)
                    snprintf(buf, alloc_size, "continue:%s", opt->root);
                else
                    snprintf(buf, alloc_size, "continue");
                opt->action_str = buf;
            }
            break;

        case ACTION_TYPE_REBOOT:
            {
                size_t alloc_size = strlen("reboot:") + 3; // "reboot:63\0"
                buf = malloc(alloc_size);
                if (!buf)
                {
                    ERROR("malloc for %zu bytes failed", alloc_size);
                    return -1;
                }
                snprintf(buf, alloc_size, "reboot:%d", opt->reboot_part);
                opt->action_str = buf;
            }
            break;

        case ACTION_TYPE_RECOVERY:
            {
                size_t alloc_size = strlen("recoveryshell") + 1; // "reboot:63\0"
                buf = malloc(alloc_size);
                if (!buf)
                {
                    ERROR("malloc for %zu bytes failed", alloc_size);
                    return -1;
                }
                strncpy(buf, "recoveryshell", alloc_size);
                opt->action_str = buf;
            }
            break;

        case ACTION_TYPE_CUSTOM:
            // no-op, already set
            break;

        default:
            ERROR("Unknown action type: %d", opt->type);
            return -1;
    }
    return 0;
}

int newbs_run_action(int argc, char **argv)
{
    newbs_config_t *config = NULL;
    newbs_option_t *opt = NULL;
    char input_buf[MAX_INPUT_LEN] = {0};
    int err = 1;

    DEBUG("Enter");
    if (argc < 1)
    {
        ERROR("need to specify a config filename");
        return 1;
    }

    config = get_newbs_config(argv[0]);
    if (!config)
        return 1;

    // set up actions
    opt = config->option_list;
    for (; opt; opt=opt->next)
    {
        if (set_option_action_str(opt))
        {
            ERROR("set_option_action_str failed for option '%s'", opt->name);
            goto out;
        }
    }

    fprintf(stderr, "Options from %s:\n", argv[0]);
    opt = config->option_list;
    for (int i = 1; opt; i++, opt=opt->next)
        fprintf(stderr, "%2d) %s (%s)\n", i, opt->name, opt->action_str);

    fprintf(stderr, "\nBuilt-in Actions:\n");
    for (const static_action_t *sa = static_actions; sa->name; sa++)
        fprintf(stderr, "%2c) %s\n", sa->key, sa->name);
    fprintf(stderr, "\n");

    do
    {
        fprintf(stderr, "Select an option> ");
        if (fgets(input_buf, sizeof(input_buf), stdin) == NULL)
        {
            ERROR("Failed to read user input");
            goto out;
        }
        char *c = input_buf;
        while (*c && isspace(*c))
            c++;
        if (*c)
            break;
    } while (true);

    const char *command_to_print = NULL;
    long index;

    if (!check_strtol(input_buf, 10, &index))
    {
        // input is a number, so it's an option index
        if (index < 0 || index > config->option_count)
        {
            ERROR("Option index %ld out of range", index);
            goto out;
        }

        // find the right option
        opt = config->option_list;
        for (int i = 1; opt && i < index; i++, opt=opt->next);
        command_to_print = opt->action_str;
    }
    else
    {
        // input isn't a number so it must be a letter index of a static option
        char c = tolower(*input_buf);
        const static_action_t *sa = static_actions;

        for (; sa->name; sa++)
            if (sa->key == c)
                break;
        if (!sa->name)
        {
            ERROR("Invalid option %s", input_buf);
            goto out;
        }
        if (sa->action_str)
            command_to_print = sa->action_str;
        else
        {
            // user wants to input a custom command
            do
            {
                fprintf(stderr, "Enter custom command> ");
                if (fgets(input_buf, sizeof(input_buf), stdin) == NULL)
                {
                    ERROR("Failed to read user input");
                    goto out;
                }
                char *c = input_buf;
                while (*c && isspace(*c))
                    c++;
                if (*c)
                    break;
            } while (true);

            if (input_buf[strlen(input_buf)-1] == '\n')
                input_buf[strlen(input_buf)-1] = '\0';

            command_to_print = strdup(input_buf);
            if (!command_to_print)
            {
                ERROR("strdup failed");
                goto out;
            }
        }
    }

    // print and be done
    fprintf(stdout, "%s\n", command_to_print);
    err = 0;

out:
    newbs_config_cleanup(config);
    free(config);
    return err;
}
