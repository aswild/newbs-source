/**********************************************************************
 * Tiny initramfs program for raspberry pi
 *
 * Copyright 2019 Allen Wild <allenwild93@gmail.com>
 * SPDX-License-Identifier: GPL-2.0
 **********************************************************************/

/**********************************************************************
 * INCLUDES
 **********************************************************************/
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "newbs_init.h"

/**********************************************************************
 * DEFINES, TYPES, LOCALS, USING
 **********************************************************************/
#ifndef LASTBOOT_STAMP_FILE
#define LASTBOOT_STAMP_FILE "/boot/lastboot_timestamp"
#endif

using std::getline;
using std::ifstream;
using std::ios;
using std::map;
using std::string;
using std::vector;

#ifdef ENABLE_TESTS
static int run_test(int argc, char **argv);
#endif

/**********************************************************************
 * GLOBAL VARIABLES
 **********************************************************************/
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
// make a directory, fatal error if it didn't work
static inline void make_dir(const char *path)
{
    if ((mkdir(path, 0777) < 0) && (errno != EEXIST))
        FATAL_ERRNO("failed to mkdir %s", path);
}

// populates the cmdline_params global map
static void parse_cmdline(const char *cmdline_file=NULL)
{
    if (cmdline_file == NULL)
        cmdline_file = "/proc/cmdline";
    errno = 0;
    ifstream ifs(cmdline_file, ios::in);
    if (!ifs.good())
        FATAL_ERRNO("failed to open %s for reading", cmdline_file);

    for (string word; getline(ifs, word, ' ');)
    {
        if (word.empty())
            continue;

        size_t eqpos = word.find('=');
        if (eqpos != string::npos)
        {
            string key = word.substr(0, eqpos);
            string val = word.substr(eqpos+1);

            // strip trailing whitespace from value
            size_t wpos = val.find_first_of(" \r\n\t");
            if (wpos != string::npos)
                val.erase(val.begin() + wpos, val.end());

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
    if (mount("proc", "/proc", "proc", 0, NULL) < 0)
        FATAL_ERRNO("failed to mount /proc");

    make_dir("/sys");
    if (mount("sysfs", "/sys", "sysfs", 0, NULL) < 0)
        FATAL_ERRNO("failed to mount /sys");

    make_dir("/dev");
    if (mount("devtmpfs", "/dev", "devtmpfs", 0, NULL) < 0)
        FATAL_ERRNO("failed to mount /dev");

    make_dir("/run");
    if (mount("tmpfs", "/run", "tmpfs", MS_NOSUID | MS_NODEV, "mode=0755") < 0)
        FATAL_ERRNO("failed to mount /run");

    parse_cmdline();
}

// populate the filesystems vector from /proc/filesystems
static bool parse_filesystems(void)
{
    errno = 0;
    ifstream ifs("/proc/filesystems", ios::in);
    if (!ifs.good())
    {
        log_error_errno("failed to open /proc/filesystems for reading");
        return false;
    }

    for (string line; getline(ifs, line);)
    {
        if (line.empty() || (line.substr(0, 5) == "nodev"))
            continue;

        size_t pos = line.find_first_not_of(" \t");
        filesystems.push_back(line.substr(pos));
    }
    ifs.close();
    return filesystems.size() > 0;
}

static void wait_for_device(const char *dev)
{
    constexpr int wait_time = 15; // seconds
    constexpr useconds_t retry_delay = 10000; // 10ms = 10000us
    int retry_count = (wait_time * 1000000) / retry_delay;

    log_info("waiting for device %s (max %d seconds)", dev, wait_time);
    while (retry_count > 0)
    {
        if (access(dev, R_OK) == 0)
            break;
        usleep(retry_delay);
        retry_count--;
    }
}

// mount /dev/mmcblk0p1 on /boot and check the timestamp of /boot/lastboot_timestamp
// If that file is newer than the current time, advance the clock
static void update_clock(void)
{
    static const char stampfile[] = LASTBOOT_STAMP_FILE;

    make_dir("/boot");
    wait_for_device("/dev/mmcblk0p1");
    if (mount("/dev/mmcblk0p1", "/boot", "vfat", MS_RDONLY, NULL) != 0)
    {
        log_warning_errno("failed to mount /dev/mmcblk0p1 on /boot");
        return;
    }
    log_info("mounted /dev/mmcblk0p1 on /boot");

    struct stat sb;
    if (stat(stampfile, &sb) != 0)
    {
        log_warning_errno("failed to stat %s", stampfile);
        goto out;
    }

    struct timespec cur_time;
    clock_gettime(CLOCK_REALTIME, &cur_time);

    // the whole point of this initramfs is to set the clock to *after* the mtime of the
    // most recent systemd journal file. Because it's hard to get the shutdown script
    // ordering precise, the journal's timestamp is probably newer than the stamp file
    // in /boot. Add an arbitrary amount to account for that difference.
    sb.st_mtim.tv_sec += 15;

    if (cur_time.tv_sec < sb.st_mtim.tv_sec)
    {

        char timebuf[32] = {0};
        ctime_r(&sb.st_mtim.tv_sec, timebuf);
        timebuf[strlen(timebuf)-1] = '\0'; // remove \n

        log_info("advancing clock to %s", timebuf);
        if (clock_settime(CLOCK_REALTIME, &sb.st_mtim) != 0)
            log_warning_errno("failed to set time");
    }

out:
    if (umount("/boot") != 0)
    {
        if (umount2("/boot", MNT_DETACH) != 0)
            log_warning_errno("failed to unmount /boot");
    }
}

// mount the root filesystem
static void mount_rootfs(void)
{
    const char *rootfs_dev = NULL;
    string& rootfs_dev_str = cmdline_params["root"];
    if (rootfs_dev_str.empty())
    {
        log_warning("no root= found in /proc/cmdline, using default /dev/mmcblk0p2");
        rootfs_dev_str = string("/dev/mmcblk0p2");
    }
    rootfs_dev = rootfs_dev_str.c_str();

    // wait for root device to become ready, the kernel is usually still setting up
    // the sdcard when the initramfs starts
    wait_for_device(rootfs_dev);
    if (access(rootfs_dev, R_OK) != 0)
        FATAL_ERRNO("unable to find root device %s", rootfs_dev);

    make_dir(rootfs_mountpoint);

    const char *fstype = get_fstype(rootfs_dev);
    if (fstype != NULL)
    {
        if (mount(rootfs_dev, rootfs_mountpoint, fstype, MS_RDONLY, NULL) == 0)
            return; // success!
    }

    // didn't find a known filesystem magic or the mount above failed,
    // try everything from /proc/filesystems
    if (parse_filesystems())
    {
        for (const auto& type : filesystems)
        {
            int r = mount(rootfs_dev, rootfs_mountpoint, type.c_str(), MS_RDONLY, NULL);
            if (r == 0)
                return; // success, we're done

            if (errno != -EINVAL)
            {
                // in our case, EINVAL means bad superblock, which we ignore and try the next type
                log_warning_errno("failed to mount %s as type %s",
                                  rootfs_dev, type.c_str());
            }
        }

        log_raw("FATAL: Didn't mount root! Tried fs types: ");
        for (const auto& type : filesystems)
            log_raw("%s ", type.c_str());
        log_raw("\n");
    }
    FATAL("unable to mount root filesystem");
}

int main(int argc, char *argv[])
{
#ifdef ENABLE_TESTS
    if (argc > 1 && !strcmp(argv[1], "--test"))
    {
        return run_test(argc-2, argv+2);
    }
#else
    // suppress -Werror=unused-parameter
    (void)argc; (void)argv;
#endif

    if (getpid() != 1)
        FATAL("this program must be run as PID 1 (except for test modes)");

    early_init();
    log_init();
    atexit(log_deinit);
    update_clock();
    mount_rootfs();
    if (switchroot(rootfs_mountpoint) != 0)
        FATAL("switchroot failed");

    if (access("/sbin/init", X_OK))
        log_warning("/sbin/init doesn't appear to exist or isn't executable");

    log_info("leaving initramfs...");
    execl("/sbin/init", "/sbin/init", NULL);

    // NOTE: see util_linux c.h errexec definition for standard return codes if exec fails
    int ret = (errno == ENOENT) ? 127 : 126;
    log_fatal_errno("failed to exec new init");
    return ret;
}

#ifdef ENABLE_TESTS
static int run_test(int argc, char **argv)
{
    const char *test = argv[0];
    if (!test)
    {
        printf("No test specified\n");
        return 1;
    }

    printf("Running test: %s\n", test);
    if (!strcmp(test, "filesystems"))
    {
        parse_filesystems();
        printf("Found in /proc/filesystems:\n");
        for (const string& fs : filesystems)
        {
            printf("%s\n", fs.c_str());
        }
    }
    else if (!strcmp(test, "fstype"))
    {
        if (argc < 2)
        {
            printf("ERROR: missing argument for fstype test: <device...>\n");
            return 1;
        }
        for (int i = 1; i < argc; i++)
        {
            const char *fstype = get_fstype(argv[i]);
            printf("%s:\t%s\n", argv[i], fstype ? fstype : "(null)");
        }
    }
    else if (!strcmp(test, "cmdline"))
    {
        if (argc > 1)
            parse_cmdline(argv[1]);
        else
            parse_cmdline();

        printf("cmdline root='%s'\n", cmdline_params["root"].c_str());
        printf("cmdline args:\n");
        for (const auto& p : cmdline_params)
        {
            if (p.second.empty())
                printf("'%s'\n", p.first.c_str());
            else
                printf("'%s'='%s'\n", p.first.c_str(), p.second.c_str());
        }

    }
    else
    {
        printf("ERROR: unknown test\n");
        return 1;
    }
    return 0;
}
#endif // ENABLE_TESTS
