/**********************************************************************
 * Tiny initramfs program for raspberry pi
 *
 * Copyright 2019 Allen Wild <allenwild93@gmail.com>
 * SPDX-License-Identifier: GPL-2.0
 **********************************************************************/

/**********************************************************************
 * INCLUDES
 **********************************************************************/
#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "PError.h"

/**********************************************************************
 * EXTERNS
 **********************************************************************/
// from switch_root.c
extern "C" int switchroot(const char *newroot);

// from blkid.c
extern "C" const char* get_fstype(const char *device);

/**********************************************************************
 * DEFINES, TYPES, AND USINGS
 **********************************************************************/
using std::getline;
using std::ifstream;
using std::ios;
using std::map;
using std::string;
using std::vector;

/**********************************************************************
 * GLOBAL VARIABLES
 **********************************************************************/
static const char boot_device[] = "/dev/mmcblk0p1";
static const char boot_mountpoint[] = "/boot";
static const char rootfs_mountpoint[] = "/rootfs";

// map of arguments in /proc/cmdline. Each space-separated word is split on the
// first '=' to get key/value for the map. If there is no '=', the value will
// be an empty string.  If multiple keys are specified, the last one will take
// precedence.
static map<string, string> cmdline_params;

// list of filesystem types to attempt mounting, populated by reading /proc/filesystems
// and collecting everything that isn't marked "nodev"
static vector<string> filesystems;

/**********************************************************************
 * FUNCTIONS
 **********************************************************************/
// make a directory, throw an exception if it didn't work
static inline void make_dir(const char *path)
{
    if ((mkdir(path, 0777) < 0) && (errno != EEXIST))
        THROW_ERRNO("Failed to mkdir %s", path);
}

// populates the cmdline_params global map
static void parse_cmdline(void)
{
    errno = 0;
    ifstream ifs("/proc/cmdline", ios::in);
    if (!ifs.good())
        THROW_ERRNO("Failed to open /proc/cmdline for reading");

    for (string word; getline(ifs, word, ' ');)
    {
        if (word.empty())
            continue;

        size_t eqpos = word.find('=');
        if (eqpos != string::npos)
        {
            const string key = word.substr(0, eqpos);
            const string val = word.substr(eqpos+1);
            cmdline_params[key] = val;
        }
        else
        {
            cmdline_params[word] = string();
        }
    }
    ifs.close();
}

// mount early filesystems and such
static void early_init(void)
{
    make_dir("/proc");
    if (mount("none", "/proc", "proc", 0, NULL) < 0)
        THROW_ERRNO("Failed to mount /proc");

    make_dir("/sys");
    if (mount("none", "/sys", "sysfs", 0, NULL) < 0)
        THROW_ERRNO("Failed to mount /sys");

    make_dir("/dev");
    if (mount("devtmpfs", "/dev", "devtmpfs", 0, NULL) < 0)
        THROW_ERRNO("Failed to mount /dev");

    make_dir("/run");
    if (mount("tmpfs", "/run", "tmpfs", MS_NOSUID | MS_NODEV, "mode=0755") < 0)
        THROW_ERRNO("Failed to mount /run");

    parse_cmdline();
}

// populate the filesystems vector from /proc/filesystems
static void parse_filesystems(void)
{
    errno = 0;
    ifstream ifs("/proc/filesystems", ios::in);
    if (!ifs.good())
        THROW_ERRNO("Failed to open /proc/filesystems for reading");

    for (string line; getline(ifs, line);)
    {
        if (line.empty() || (line.substr(0, 5) == "nodev"))
            continue;

        size_t pos = line.find_first_not_of(" \t");
        filesystems.push_back(line.substr(pos));
    }
    ifs.close();
}

// mount the root filesystem
static void mount_rootfs(void)
{
    if ((mkdir(rootfs_mountpoint, 0777) < 0) && (errno != EEXIST))
        THROW_ERRNO("Failed to mkdir %s", rootfs_mountpoint);

    const char *rootfs_dev = NULL;
    string& rootfs_dev_str = cmdline_params["root"];
    if (rootfs_dev_str.empty())
    {
        printf("WARNING: no root= found in /proc/cmdline, using default /dev/mmcblk0p2\n");
        rootfs_dev_str = string("/dev/mmcblk0p2");
    }
    rootfs_dev = rootfs_dev_str.c_str();

    // wait for root device to become ready, the kernel is usually still setting up
    // the sdcard when the initramfs starts
    constexpr int rootfs_wait_time = 15; // seconds
    constexpr useconds_t retry_delay = 10000; // 10ms = 10000us
    int retry_count = (rootfs_wait_time * 1000000) / retry_delay;

    printf("Waiting for root device %s (max %d seconds)\n", rootfs_dev, rootfs_wait_time);
    while (retry_count > 0)
    {
        if (access(rootfs_dev, R_OK) == 0)
            break;
        usleep(retry_delay);
        retry_count--;
    }
    if (access(rootfs_dev, R_OK) != 0)
        THROW_ERROR("Unable to find root device %s\n", rootfs_dev);

    const char *fstype = get_fstype(rootfs_dev);
    if (fstype != NULL)
    {
        if (mount(rootfs_dev, rootfs_mountpoint, fstype, MS_RDONLY, NULL) == 0)
            return; // success!
    }

    // didn't find a known filesystem magic or the mount above failed,
    // try everything from /proc/filesystems
    parse_filesystems();
    for (const auto& type : filesystems)
    {
        int r = mount(rootfs_dev, rootfs_mountpoint, type.c_str(), MS_RDONLY, NULL);
        if (r == 0)
            return; // success, we're done

        if (errno != -EINVAL)
        {
            // in our case, EINVAL means bad superblock, which we ignore and try the next type
            printf("warning: failed to mount %s as type %s: %s\n",
                   rootfs_dev, type.c_str(), strerror(errno));
        }
    }

    printf("Didn't mount root! Tried fs types: ");
    for (const auto& type : filesystems)
        printf("%s ", type.c_str());
    putchar('\n');
    THROW_ERROR("unable to mount root filesystem");
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    try
    {
        early_init();
        mount_rootfs();
        if (switchroot(rootfs_mountpoint) != 0)
            THROW_ERROR("switchroot failed");
    }
    catch (std::exception& e)
    {
        printf("ERROR: %s\n", e.what());
        return 1;
    }

    if (access("/sbin/init", X_OK))
        printf("WARNING: /sbin/init doesn't appear to exist or isn't executable\n");

    printf("Leaving initramfs...\n");
    execl("/sbin/init", "/sbin/init", NULL);

    // NOTE: see util_linux c.h errexec definition for standard return codes if exec fails
    printf("Failed to exec new init: %s\n", strerror(errno));
    return 1;
}
