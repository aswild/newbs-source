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
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include "newbs-util.h"

#define CMDLINE_ARG_SEARCH  "newbscmd="
#define MAX_LINE_LENGTH     256

#define DEFAULT_TIMEOUT      5
#define DEFAULT_ERROR_ACTION OPTION_TYPE_RECOVERY

typedef enum {
    OPTION_TYPE_INVALID = 0,
    OPTION_TYPE_CONTINUE,
    OPTION_TYPE_REBOOT,
    OPTION_TYPE_RECOVERY,
    OPTION_TYPE_CUSTOM
} option_type_e;

const char *option_type_strs[] = {
    "INVALID",
    "CONTINUE",
    "REBOOT",
    "RECOVERY",
    "CUSTOM"
};

typedef struct _newbs_option {
    char            *name;
    option_type_e   type;
    char            *root;
    int             reboot_part;
    char            *custom_command;
    struct _newbs_option *next;
} newbs_option_t;

typedef struct {
    int             timeout;
    option_type_e   error_action;
    char            *default_option_str;
    newbs_option_t  *default_option;

    int             option_count;
    newbs_option_t  *option_list;
} newbs_config_t;

typedef enum {
    LINE_TYPE_COMMENT,
    LINE_TYPE_OPTION_NAME,
    LINE_TYPE_OPTION_PARAM
} line_type_e;

typedef struct {
    line_type_e type;
    char *key;
    char *value;
} parsed_line_t;

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

/*
 * Find a pointer to the default option based on config->default_option_str
 */
static newbs_option_t* find_default_option(newbs_config_t *config)
{
    if (!config || !config->default_option_str || config->option_count < 1)
        return NULL;

    newbs_option_t *opt = NULL;

    // first see if default_option_str can be parsed as an integer
    errno = 0;
    long index = strtol(config->default_option_str, NULL, 0);
    if (!errno)
    {
        // yep, it's a number
        if (index < 0 || index >= config->option_count)
        {
            ERROR("Default option index %ld out of range", index);
            return NULL;
        }

        opt = config->option_list;
        for (int i = index; i > 0; i--, opt = opt->next);
        return opt;
    }
    else
    {
        // the default option value isn't a number, so it must be a name
        opt = config->option_list;
        for (int i = config->option_count; i > 0; i--, opt = opt->next)
        {
            if (strcasecmp(config->default_option_str, opt->name))
                return opt; // found a match
        }
    }

    // not found
    return NULL;
}

static int parse_line(const char *buf, parsed_line_t *parsed_line)
{
    if (!buf || !parsed_line)
        return -1;

    memset(parsed_line, 0, sizeof(*parsed_line));

    // ignore all leading whitespace
    while (isspace(*buf))
        buf++;

    switch (*buf)
    {
        case '#':
        case '\0':
            parsed_line->type = LINE_TYPE_COMMENT;
            break;

        case '[':
            {
                char *end_bracket = strchr(buf, ']');
                if (!end_bracket)
                {
                    ERROR("Missing ']'");
                    return -1;
                }
                *end_bracket = '\0';

                parsed_line->type = LINE_TYPE_OPTION_NAME;
                parsed_line->key = strdup(buf+1);
                if (!parsed_line->key)
                {
                    ERROR("strdup failed");
                    return -1;
                }
            }
            break;

        default:
            {
                char *value = strchr(buf, '=');
                if (!value)
                {
                    ERROR("Missing '=' for value assignment");
                    return -1;
                }

                *value = '\0';
                value++; // value starts after the =

                // strip trailing newline
                if (value[strlen(value)-1] == '\n')
                    value[strlen(value)-1] = '\0';

                parsed_line->type = LINE_TYPE_OPTION_PARAM;
                parsed_line->key = strdup(buf);
                if (!parsed_line->key)
                {
                    ERROR("strdup failed for key");
                    return -1;
                }

                parsed_line->value = strdup(value);
                if (!parsed_line->value)
                {
                    ERROR("strdup failed for value");
                    free(parsed_line->key);
                    return -1;
                }
            }
            break;
    }

    return 0;
}

static int add_option_param(newbs_option_t *opt, char *key, char *value, bool *can_free)
{
    if (!opt || !key || !value)
        return -1;

    // whether the caller can free key/value
    can_free[0] = true;
    can_free[1] = true;

    if (!strcasecmp(key, "type"))
    {
        if (!strcasecmp(value, "continue"))
            opt->type = OPTION_TYPE_CONTINUE;

        else if (!strcasecmp(value, "reboot"))
            opt->type = OPTION_TYPE_REBOOT;

        else if (!strcasecmp(value, "recovery"))
            opt->type = OPTION_TYPE_RECOVERY;

        else if (!strcasecmp(value, "custom"))
            opt->type = OPTION_TYPE_CUSTOM;

        else
        {
            ERROR("Invalid boot option type '%s'", value);
            return -1;
        }
    }
    else if (!strcasecmp(key, "root"))
    {
        opt->root = value;
        can_free[1] = false;
    }
    else if (!strcasecmp(key, "rebootpart"))
    {
        errno = 0;
        opt->reboot_part = (int)strtol(value, NULL, 0);
        if (errno || opt->reboot_part > 63)
        {
            ERROR("Invalid reboot partition '%s'", value);
            return -1;
        }
    }
    else if (!strcasecmp(key, "customcommand"))
    {
        opt->custom_command = value;
        can_free[1] = false;
    }
    else
    {
        ERROR("Invalid boot option param '%s'", key);
        return -1;
    }
    return 0;
}

static int add_main_config_param(newbs_config_t *config, char *key, char *value, bool *can_free)
{
    if (!config || !key || !value)
        return -1;

    // whether the caller can free key/value
    can_free[0] = true;
    can_free[1] = true;

    if (!strcasecmp(key, "default"))
    {
        config->default_option_str = value;
        can_free[1] = false;
    }
    else if (!strcasecmp(key, "timeout"))
    {
        errno = 0;
        config->timeout = (int)strtol(value, NULL, 0);
        if (errno)
        {
            ERROR("Invalid timeout value '%s'", value);
            return -1;
        }
    }
    else if (!strcasecmp(key, "onerror"))
    {
        if (!strcasecmp(value, "continue"))
            config->error_action = OPTION_TYPE_CONTINUE;
        else if (!strcasecmp(value, "recovery"))
            config->error_action = OPTION_TYPE_RECOVERY;
        else
        {
            ERROR("Invalid OnError action '%s'", value);
            return -1;
        }
    }
    else
    {
        ERROR("Invalid main config option '%s'", key);
        return -1;
    }
    return 0;
}

static int parse_config_file(const char *filename, newbs_config_t *config)
{
    FILE *fp = NULL;
    char buf[MAX_LINE_LENGTH] = {0};
    int ret = -1;

    DEBUG("filename is '%s'", filename);

    if (!filename || !config)
        return -1;

    if (!(fp = fopen(filename, "r")))
    {
        ERROR("Unable to open '%s'", filename);
        return -1;
    }

    bool parsing_main_config = false;
    parsed_line_t parsed_line = {0};
    newbs_option_t *current_opt = NULL;
    int line_num = 1;

    for (; fgets(buf, sizeof(buf), fp); line_num++)
    {
        if (buf[strlen(buf)-1] != '\n')
        {
            // if no newline, then we didn't read a full line
            ERROR("Newbs config file line %d is too long (limit is %d)", line_num, MAX_LINE_LENGTH-1);
            goto out;
        }

        // strip trailing newline
        buf[strlen(buf)-1] = '\0';
        DEBUG("parsing line %d '%s'", line_num, buf);
        if (parse_line(buf, &parsed_line))
            goto out;

        switch (parsed_line.type)
        {
            case LINE_TYPE_COMMENT:
                continue;

            case LINE_TYPE_OPTION_NAME:
                if (!strcasecmp(parsed_line.key, "newbs"))
                {
                    parsing_main_config = true;
                    free(parsed_line.key);
                }
                else
                {
                    parsing_main_config = false;
                    newbs_option_t *new_opt = calloc(sizeof(*new_opt), 1);
                    if (!new_opt)
                    {
                        ERROR("calloc failed");
                        goto out;
                    }

                    new_opt->name = parsed_line.key;
                    if (current_opt)
                        current_opt->next = new_opt;
                    else
                        config->option_list = new_opt;
                    config->option_count++;
                    current_opt = new_opt;
                }
                free(parsed_line.value);
                break;

            case LINE_TYPE_OPTION_PARAM:
                {
                    bool can_free[2] = {0};
                    int err = 0;
                    if (parsing_main_config)
                    {
                        err = add_main_config_param(config, parsed_line.key, parsed_line.value, can_free);
                    }
                    else
                    {
                        err = add_option_param(current_opt, parsed_line.key, parsed_line.value, can_free);
                    }

                    if (err || can_free[0])
                        free(parsed_line.key);
                    if (err || can_free[1])
                        free(parsed_line.value);
                    if (err)
                        goto out;
                }
                break;

            default:
                ERROR("you dun goofed");
                goto out;
        }
    }

    config->default_option = find_default_option(config);
    if (!config->default_option)
        WARNING("No default option found");

    // Success!
    ret = 0;

out:
    fclose(fp);
    if (ret)
        ERROR("Can't parse line number %d: '%s'", line_num, buf);
    return ret;
}

static void newbs_config_init(newbs_config_t *config)
{
    memset(config, 0, sizeof(*config));
    config->timeout = DEFAULT_TIMEOUT;
    config->error_action = DEFAULT_ERROR_ACTION;
}

static void newbs_config_cleanup(newbs_config_t *config)
{
    newbs_option_t *opt = config->option_list;
    newbs_option_t *next = NULL;

    while (opt)
    {
        free(opt->name);
        free(opt->root);
        free(opt->custom_command);

        next = opt->next;
        free(opt);
        opt = next;
    }

    config->option_count = 0;
    config->option_list = NULL;
    config->default_option = NULL;
    free(config->default_option_str);
}

int newbs_get_action(int argc, char **argv)
{
    INFO("Enter");
    printf("cmdline check: %s\n", check_cmdline());
    return 0;
}

int newbs_dump_config(int argc, char **argv)
{
    if (argc < 1)
    {
        ERROR("Need to specify a filename");
        return 1;
    }

    newbs_config_t config;
    newbs_config_init(&config);
    if (parse_config_file(argv[0], &config))
    {
        ERROR("Failed to parse config");
        return 1;
    }

    printf("[NEWBS]\n");
    if (config.default_option)
        printf("Default=%s [%s=%s]\n", config.default_option_str, config.default_option->name,
               option_type_strs[config.default_option->type]);
    else
        printf("Default=(null)\n");
    printf("Timeout=%d\n", config.timeout);
    printf("OnError=%s\n", option_type_strs[config.error_action]);
    printf("\n");

    int i;
    newbs_option_t *opt = config.option_list;
    for (i = 0; i < config.option_count && opt; i++, opt = opt->next)
    {
        printf("[%s]\n", opt->name);
        printf("Type=%s\n", option_type_strs[opt->type]);
        printf("Root=%s\n", opt->root);
        printf("RebootPart=%d\n", opt->reboot_part);
        printf("CustomCommand=%s\n", opt->custom_command);
        printf("\n");
    }

    newbs_config_cleanup(&config);
    return 0;
}
