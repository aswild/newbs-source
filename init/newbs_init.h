/**********************************************************************
 * Tiny initramfs program for raspberry pi
 *
 * Copyright 2019 Allen Wild <allenwild93@gmail.com>
 * SPDX-License-Identifier: GPL-2.0
 **********************************************************************/

#ifndef NEWBS_INIT_H
#define NEWBS_INIT_H

enum log_level_t
{
    LOG_LEVEL_FATAL,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,

    LOG_LEVEL_COUNT
};
#ifndef __cplusplus
typedef enum log_level_t log_level_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

// switch_root.c
int switchroot(const char *newroot);

// blkid.c
const char* get_fstype(const char *device);

// log.c
void log_message(log_level_t level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void log_raw(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void log_set_level(log_level_t level);
void log_init(void);
void log_deinit(void);

#ifdef __cplusplus
} // extern "C"
#endif

#define log_fatal(fmt, args...)   log_message(LOG_LEVEL_FATAL, fmt, ##args)
#define log_error(fmt, args...)   log_message(LOG_LEVEL_ERROR, fmt, ##args)
#define log_warning(fmt, args...) log_message(LOG_LEVEL_WARNING, fmt, ##args)
#define log_info(fmt, args...)    log_message(LOG_LEVEL_INFO, fmt, ##args)
#define log_debug(fmt, args...)   log_message(LOG_LEVEL_DEBUG, fmt, ##args)

#define log_fatal_errno(fmt, args...)   log_fatal(fmt ": %s", ##args, strerror(errno))
#define log_error_errno(fmt, args...)   log_error(fmt ": %s", ##args, strerror(errno))
#define log_warning_errno(fmt, args...) log_warning(fmt ": %s", ##args, strerror(errno))

#define FATAL(fmt, args...) do { \
    log_fatal(fmt, ##args); \
    exit(1); \
} while (0)

#define FATAL_ERRNO(fmt, args...) do { \
    log_fatal_errno(fmt, ##args); \
    exit(1); \
} while (0)

#endif // NEWBS_INIT_H
