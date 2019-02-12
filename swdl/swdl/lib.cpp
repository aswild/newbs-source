/*******************************************************************************
 * Copyright (C) 2018-2019 Allen Wild <allenwild93@gmail.com>
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

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <iterator>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <mntent.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "newbs-swdl.h"

// split a file into words using magic C++ i(n)f(ile)stream iterator
// roughtly equivalent to Python open(filename).read().split()
stringvec split_words_in_file(const string& filename)
{
    std::ifstream ifs(filename);
    if (ifs.fail())
        THROW_ERRNO("Unable to open %s as ifstream", filename.c_str());

    std::istream_iterator<string> start(ifs), end; // default constructor for end yields "end of stream"
    stringvec vec(start, end); // initialize vector from start/end iterator pair
    ifs.close();
    return vec;
}

string join_words(const stringvec& vec, const string& sep)
{
    std::ostringstream ss;
    for (auto it = vec.begin(); it != vec.end(); it++)
    {
        if (it != vec.begin())
            ss << sep;
        ss << *it;
    }
    return ss.str();
}

CPipe open_curl(const string& url_)
{
    string url;

    if (url_ == "-")
    {
        log_info("reading image from stdin");
        return { .pid = -1, .fd = STDIN_FILENO, .running = false };
    }

    struct stat sb;
    if ((stat(url_.c_str(), &sb) == 0) && ((sb.st_mode & S_IFMT) != S_IFDIR))
    {
        log_debug("using local file %s", url_.c_str());
        char *fullpath = realpath(url_.c_str(), NULL);
        if (fullpath == NULL)
            THROW_ERRNO("Failed to expand local file path %s", url_.c_str());
        url = string("file://") + fullpath;
        free(fullpath);
    }
    else url = url_;

    log_info("Flashing image '%s'", url.c_str());

    // opening some sort of URI, open a pipe and fork off to curl
    int pfd[2];
    if (pipe(pfd) == -1)
        THROW_ERRNO("pipe() failed");

    pid_t cpid = fork();
    if (cpid < 0)
        THROW_ERRNO("fork() failed");
    else if (cpid == 0)
    {
        // child process
        close(pfd[0]);   // close read end of the pipe
        dup2(pfd[1], 1); // redirect stdout to write end of pipe
        close(pfd[1]);   // close old pipe fd that was just dup'd

        // -s (be quiet), -S (still print errors)
        // -L (follow redirects), -f (report HTTP errors)
        execlp("curl", "curl", "-sSLf", "--", url.c_str(), NULL);
        THROW_ERRNO("execlp() failed");
    }

    // parent process
    close(pfd[1]); // close write end of pipe

    // return child PID and read end of the pipe
    log_debug("started child process %d", cpid);
    return { .pid = cpid, .fd = pfd[0], .running = true };
}

// wait for a cpipe process. throw an exception if it returns nonzero or was killed by a signal
void cpipe_wait(CPipe& cp, bool block)
{
    int waitflags = block ? 0 : WNOHANG;

    if (!cp.running)
        return;

    int wstatus;
    pid_t waitret = waitpid(cp.pid, &wstatus, waitflags);
    if (waitret > 0)
    {
        cp.running = false;
        if (WIFEXITED(wstatus))
        {
            int status = WEXITSTATUS(wstatus);
            if (status == 0)
                log_debug("child process %d exited successfully", cp.pid);
            else
                throw PError("child process %d exited non-zero (%d)", cp.pid, status);
        }
        else if (WIFSIGNALED(wstatus))
            throw PError("child process %d killed by signal %d", cp.pid, WTERMSIG(wstatus));
        else
            log_warn("child process %d exited, but not normally or by signal???", cp.pid);
    }
}

// read from a forked child process, then check if it exited using cpipe_wait.
// continues reading from the pipe after exit, cpipe_wait will throw an exception
// if the process exits non-zero or was killed by a signal.
size_t cpipe_read(CPipe& cp, void *buf, size_t count)
{
    size_t nread = 0;
    if ((cp.fd != -1) && (count != 0))
    {
        nread = read_n(cp.fd, buf, count);
        if (nread < count)
        {
            close(cp.fd);
            cp.fd = -1;
            if (errno)
                THROW_ERRNO("read error on pipe");
            else
                THROW_ERROR("pipe closed after reading only %zd/%zu bytes", nread, count);
        }
    }
    return nread;
}

// put the first found mount entry info into *ment, returns whether a mount was found.
// there can be multiple mount points for the same device, an exception will be thrown
// if that happens
bool find_mntent(const string& dev, struct mntent *ment)
{
    assert(ment != NULL);
    const char *_dev = dev.c_str();

    FILE *fp = fopen("/etc/mtab", "r");
    if (fp == NULL)
        THROW_ERRNO("Failed to open /etc/mtab for reading");

    bool found = false;
    struct mntent *m;
    while ((m = getmntent(fp)) != NULL)
    {
        if (!strcmp(_dev, m->mnt_fsname))
        {
            if (found)
            {
                fclose(fp);
                throw PError("boot device %s is mounted in multiple places, aborting", _dev);
            }

            // deep copy the mntent
            memset(ment, 0, sizeof(*ment));
            ment->mnt_fsname = strdup(m->mnt_fsname);
            ment->mnt_dir    = strdup(m->mnt_dir);
            ment->mnt_type   = strdup(m->mnt_type);
            ment->mnt_opts   = strdup(m->mnt_opts);
            if (!(ment->mnt_fsname && ment->mnt_dir && ment->mnt_type && ment->mnt_opts))
            {
                fclose(fp);
                THROW_ERROR("failed to copy mntent, strdup failed");
            }
            found = true;
        }
    }

    fclose(fp);
    return found;
}

// re-mount the filesystem mount described in m.
// Do this by forking a mount(8) process rather than using mount(2)
// because it'd be tedious and unreliable to convert the string mnt_opts
// to a set of integer flags needed by mount(2).
void mount_mntent(const struct mntent *m)
{
    assert(m != NULL);
    pid_t cpid = fork();
    if (cpid < 0)
        THROW_ERRNO("fork() failed");
    else if (cpid == 0)
    {
        execlp("mount", "mount", "-t", m->mnt_type, "-o", m->mnt_opts,
               m->mnt_fsname, m->mnt_dir, NULL);
        fprintf(stderr, "%s: execlp failed: %s\n", __func__, strerror(errno));
        _exit(200);
    }
    else
    {
        log_debug("spawned mount process PID %d", cpid);
        int wstatus = 0;
        pid_t waitret = waitpid(cpid, &wstatus, 0);
        if (waitret < 0)
            THROW_ERRNO("failed to wait for mount process");

        if (WIFEXITED(wstatus))
        {
            if (WEXITSTATUS(wstatus) == 0)
                log_debug("mount process exited successfully");
            else
                throw PError("Failed to mount %s, mount returned %d",
                             m->mnt_fsname, WEXITSTATUS(wstatus));
        }
        else if (WIFSIGNALED(wstatus))
            throw PError("Failed to mount %s, mount killed by signal %d",
                         m->mnt_fsname, WTERMSIG(wstatus));
        else
            throw PError("Failed to mount %s, mount exited in an unknown manner", m->mnt_fsname);
    }
}
