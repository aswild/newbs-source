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

#ifndef PERROR_H
#define PERROR_H

#include <exception>
#include <string>

// PError: an exception class with a variadic constructor for constructing
// error messages printf style
class PError : public std::exception
{
    private:
        char *msg = NULL;

    public:
        PError(void) { /* empty default constructor, probably won't be used */ }
        PError(const char *fmt, ...) __attribute__((format(printf, 2, 3)));
        PError(const std::string& str);
        ~PError(void);

        virtual const char* what(void) const noexcept;
};

#endif
