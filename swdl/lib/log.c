/*******************************************************************************
 * Copyright (C) 2018 Allen Wild <allenwild93@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 ******************************************************************************/

#include <stdio.h>
#include <stdarg.h>

#include "nImage.h"

log_level_e log_level = LOG_LEVEL_INFO;

static const char *log_level_str[] = {
    [LOG_LEVEL_ERROR]   = "Error: ",
    [LOG_LEVEL_WARN]    = "Warning: ",
    [LOG_LEVEL_INFO]    = "",
    [LOG_LEVEL_DEBUG]   = "Debug: ",
};

static void vlog(log_level_e level, const char *fmt, va_list args)
{
    if (level <= log_level)
    {
        fputs(log_level_str[level], stderr);
        vfprintf(stderr, fmt, args);
        putc('\n', stderr);
    }
}

void log_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vlog(LOG_LEVEL_ERROR, fmt, args);
    va_end(args);
}

void log_warn(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vlog(LOG_LEVEL_WARN, fmt, args);
    va_end(args);
}

void log_info(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vlog(LOG_LEVEL_INFO, fmt, args);
    va_end(args);
}

void log_debug(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vlog(LOG_LEVEL_DEBUG, fmt, args);
    va_end(args);
}
