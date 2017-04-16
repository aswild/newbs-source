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

typedef struct {
    const char  *name;
    int (*handler)(int argc, char **argv);
} newbs_cmd_t;

int newbs_get_action(int argc, char **argv);
int newbs_reboot(int argc, char **argv);
int newbs_dump_config(int argc, char **argv);

#endif
