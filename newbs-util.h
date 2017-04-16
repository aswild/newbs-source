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

#define DEBUG(fmt, args...) do { \
    if (debug_enabled) { \
        fprintf(stderr, "%s:%d: DEBUG: " fmt "\n", __func__, __LINE__, ##args); \
    } } while (0)

#define INFO(fmt, args...) fprintf(stderr, "%s:%d: INFO: " fmt "\n", __func__, __LINE__, ##args)
#define ERROR(fmt, args...) fprintf(stderr, "%s:%d: ERROR: " fmt "\n", __func__, __LINE__, ##args)

//typedef int (*newbs_util_handler_t)(int argc, char **argv);

typedef struct {
    const char  *name;
    int (*handler)(int argc, char **argv);
} newbs_cmd_t;

#endif
