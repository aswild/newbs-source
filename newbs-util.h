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

#ifndef _NEWBS_UTIL_H_
#define _NEWBS_UTIL_H_

#include <stdbool.h>
extern bool debug_enabled;
#define DEBUG(fmt, args...) do { \
    if (debug_enabled) { \
        fprintf(stderr, "DEBUG %s:%d: " fmt "\n", __func__, __LINE__, ##args); \
    } } while (0)

#define INFO(fmt, args...)      fprintf(stderr, "INFO %s:%d: "    fmt "\n", __func__, __LINE__, ##args)
#define WARNING(fmt, args...)   fprintf(stderr, "WARNING %s:%d: " fmt "\n", __func__, __LINE__, ##args)
#define ERROR(fmt, args...)     fprintf(stderr, "ERROR %s:%d: "   fmt "\n", __func__, __LINE__, ##args)

#define DEFAULT_TIMEOUT      5
#define DEFAULT_ERROR_ACTION ACTION_TYPE_RECOVERY

typedef enum {
    ACTION_TYPE_INVALID = 0,
    ACTION_TYPE_CONTINUE,
    ACTION_TYPE_REBOOT,
    ACTION_TYPE_RECOVERY,
    ACTION_TYPE_CUSTOM,
    ACTION_TYPE_LAST
} action_type_e;

extern const char *action_type_strs[ACTION_TYPE_LAST];

typedef struct _newbs_option {
    char            *name;
    action_type_e   type;
    char            *root;
    int             reboot_part;
    char            *action_str;
    int             num;
    struct _newbs_option *next;
} newbs_option_t;

typedef struct {
    int             timeout;
    action_type_e   error_action;
    char            *default_option_str;
    newbs_option_t  *default_option;

    int             option_count;
    newbs_option_t  *option_list;
} newbs_config_t;

typedef struct {
    const char  *name;
    int (*handler)(int argc, char **argv);
} newbs_cmd_t;

int newbs_run_action(int argc, char **argv);
int newbs_reboot(int argc, char **argv);
int newbs_dump_config(int argc, char **argv);

void newbs_config_init(newbs_config_t *config);
void newbs_config_cleanup(newbs_config_t *config);
newbs_config_t* get_newbs_config(const char *filename);
int parse_config_file(const char *filename, newbs_config_t *config);

#endif
