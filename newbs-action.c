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
#include <string.h>
#include <unistd.h>
#include "newbs-util.h"

#define CMDLINE_ARG_SEARCH  "newbscmd="

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
    action_type_e   action;
} static_action_t;

static const static_action_t static_actions[] = {
    {"Continue to NEWBS Core",  'c', ACTION_TYPE_CONTINUE},
    {"Drop to init shell",      'd', ACTION_TYPE_RECOVERY},
    {"Custom command",          'e', ACTION_TYPE_CUSTOM},
    {NULL, 0, ACTION_TYPE_INVALID},
};

static inline void print_actions(FILE *fp, newbs_config_t *config)
{
    newbs_option_t *opt = config->option_list;
    for (int i = 1; opt; i++, opt=opt->next)
        fprintf(fp, "%2d) %s (%s)\n", i, opt->name, opt->action_str);

    for (const static_action_t *sa = static_actions; sa->name; sa++)
        fprintf(fp, "%2c) %s\n", sa->key, sa->name);
}

static char* check_cmdline(void)
{
    FILE *fp = fopen("/proc/cmdline", "r");
    if (!fp)
        return NULL;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    char *buf = malloc(size+1); // extra nullbyte
    if (!buf)
    {
        ERROR("malloc failed");
        fclose(fp);
        return NULL;
    }

    if (!fgets(buf, sizeof(buf), fp))
    {
        ERROR("fgets failed");
        free(buf);
        fclose(fp);
        return NULL;
    }
    fclose(fp);

    char *retstr = NULL;
    char *startptr = strcasestr(buf, CMDLINE_ARG_SEARCH);
    if (startptr)
    {
        startptr += strlen(CMDLINE_ARG_SEARCH);
        char *endptr = strchr(startptr, ' ');
        if (endptr)
            *endptr = '\0';
        retstr = strdup(startptr);
    }

    free(buf);
    return retstr;
}

int newbs_run_action(int argc, char **argv)
{
    DEBUG("Enter");

    char *cmdline_action = check_cmdline();
    if (cmdline_action)
    {
        printf("%s\n", cmdline_action);
        free(cmdline_action);
        return 0;
    }

    if (argc < 1)
    {
        ERROR("need to specify a config filename");
        return 1;
    }

    newbs_config_t *config = get_newbs_config(argv[0]);
    if (!config)
        return 1;

    fprintf(stderr, "Available Actions:\n");
    print_actions(stderr, config);

    return 0;
}
