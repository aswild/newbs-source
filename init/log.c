/**********************************************************************
 * log.c - Logging utility functions
 *
 * Copyright 2019 Allen Wild <allenwild93@gmail.com>
 * SPDX-License-Identifier: GPL-2.0
 **********************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "newbs_init.h"

static const char *log_level_strings[] = {
    "FATAL",
    "Error",
    "Warning",
    NULL, // no level prefix for info
    "Debug",
};
static_assert(sizeof(log_level_strings)/sizeof(log_level_strings[0]) == LOG_LEVEL_COUNT,
              "Log level enums and strings do not match");

static log_level_t log_level = LOG_LEVEL_INFO;
static FILE *kmsg_fp = NULL;

void log_message(log_level_t level, const char *fmt, ...)
{
    if (level > log_level)
        return;

    FILE *stream = kmsg_fp ? kmsg_fp : stdout;
    fprintf(stream, "init: ");
    if (log_level_strings[level])
        fprintf(stream, "%s: ", log_level_strings[level]);

    va_list args;
    va_start(args, fmt);
    vfprintf(stream, fmt, args);
    va_end(args);

    putc('\n', stream);
    fflush(stream);
}

void log_raw(const char *fmt, ...)
{
    FILE *stream = kmsg_fp ? kmsg_fp : stdout;
    va_list args;
    va_start(args, fmt);
    vfprintf(stream, fmt, args);
    va_end(args);
    fflush(stream);
}

void log_set_level(log_level_t level)
{
    if (level >= 0 && level < LOG_LEVEL_COUNT)
        log_level = level;
}

// open (or re-open) /dev/kmsg
void log_init(void)
{
    if (kmsg_fp)
        log_deinit();

    // use low-level open because we don't want O_CREAT which fopen would do
    int fd = open("/dev/kmsg", O_WRONLY);
    if (fd == -1)
    {
        printf("WARNING: failed to open /dev/kmsg: %s\n", strerror(errno));
        return;
    }

    if ((kmsg_fp = fdopen(fd, "w")) != NULL)
    {
        setlinebuf(kmsg_fp);
    }
    else
    {
        printf("WARNING: failed to fdopen /dev/kmsg file descriptor %d: %s\n", fd, strerror(errno));
        close(fd);
    }
}

void log_deinit(void)
{
    if (kmsg_fp)
    {
        fclose(kmsg_fp);
        kmsg_fp = NULL;
    }
}
