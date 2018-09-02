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

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>

#include "PError.h"

PError::PError(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    if (vasprintf(&msg, fmt, args) == -1)
    {
        fprintf(stderr, "PError: vasprintf failed for fmt='%s'\n", fmt);
        msg = NULL;
    }
    va_end(args);
}

PError::PError(const std::string& str)
{
    msg = strdup(str.c_str());
}

PError::~PError(void)
{
    free(msg);
    msg = NULL;
}

const char* PError::what(void) const noexcept
{
    return msg;
}
