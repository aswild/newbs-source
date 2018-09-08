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

#include <cstring>

#include "newbs-swdl.h"

#define N_BANKS 2

static const char *rootfs_devs[N_BANKS] = {
    // the index in this array is the bank number
    "/dev/mmcblk0p2",
    "/dev/mmcblk0p3",
};

int get_bank(const string& dev)
{
    for (int i = 0; i < N_BANKS; i++)
        if (dev == rootfs_devs[i])
            return i;
    return -1;
}

int get_active_bank(const stringvec& cmdline)
{
    for (const string& arg : cmdline)
        if (arg.find("root=") == 0)
            return get_bank(arg.substr(strlen("root=")));
    return -1;
}

int get_inactive_bank(const stringvec& cmdline)
{
    int active = get_active_bank(cmdline);
    if (active == -1)
        return -1;
    return (active + 1) % N_BANKS;
}

string get_inactive_dev(const stringvec& cmdline)
{
    int bank = get_inactive_bank(cmdline);
    if (bank == -1)
        return string();
    return string(rootfs_devs[bank]);
}

// update the root= entry in cmdline in-place
// add "ro" or "rw" after the root entry depending on the rw bool
void cmdline_flip_bank(stringvec& cmdline, bool rw)
{
    string rw_str = rw ? "rw" : "ro";
    string inactive_dev = get_inactive_dev(cmdline);
    if (inactive_dev.length() == 0)
        throw PError("couldn't find inactive rootfs bank from cmdline");
    log_info("flipping rootfs to %s %s", inactive_dev.c_str(), rw_str.c_str());

    // remove any existing ro/rw entry
    for (auto it = cmdline.begin(); it != cmdline.end(); /* manual increment below */)
    {
        if ((*it == "ro") || (*it == "rw"))
            it = cmdline.erase(it);
        else
            it++;
    }

    // now update the root and add ro/rw after it
    for (auto it = cmdline.begin(); it != cmdline.end(); it++)
    {
        if (it->find("root=") == 0)
        {
            *it = "root=" + inactive_dev;
            cmdline.insert(it+1, rw_str);
            return;
        }
    }

    // if we get here, no root element was found so add it to the end
    log_warn("no root device argument found in existing kernel cmdline");
    cmdline.push_back("root=" + inactive_dev);
    cmdline.push_back(rw_str);
}
