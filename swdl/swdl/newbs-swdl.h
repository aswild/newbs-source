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

#ifndef NEWBS_SWDL_H
#define NEWBS_SWDL_H

#include <exception>
#include <iostream>
#include <vector>

#include "nImage.h"
#include "PError.h"

using std::exception;
using std::string;
using std::vector;

typedef std::vector<std::string> stringvec;

struct SwdlOptions
{
    bool    toggle = true;
    bool    reboot = false;
    string  cmdline_txt = string("/boot/cmdline.txt");
};
extern SwdlOptions g_opts;

// struct for a pipe fed by a child process
struct CPipe
{
    pid_t pid = -1; // child PID owning the pipe
    int fd = -1;    // our end of the pipe
    bool running = false;
};

// lib.cpp functions
stringvec split_words_in_file(const string& filename);
string join_words(const stringvec& vec, const string& sep);
CPipe open_curl(const string& url_);
void cpipe_wait(CPipe& cp, bool block);
size_t cpipe_read(CPipe& cp, void *buf, size_t count);

// flashbanks.cpp functions
int get_bank(const string& dev);
int get_active_bank(const stringvec& cmdline);
int get_inactive_bank(const stringvec& cmdline);
string get_inactive_dev(const stringvec& cmdline);
void cmdline_flip_bank(stringvec& cmdline, bool rw);

// program.cpp functions
void program_part(CPipe& curl, const nimg_phdr_t *p, const stringvec& cmdline);


#endif // NEWBS_SWDL_H
