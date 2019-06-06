/*
 * Tiny initramfs program for raspberry pi
 *
 * Copyright 2019 Allen Wild <allenwild93@gmail.com>
 * SPDX-License-Identifier: GPL-2.0
 */

#include <exception>
#include <string>
#include <vector>

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "PError.h"

using std::vector;
using std::string;

using stringvec = vector<string>;

static const char rootfs_mountpoint[] = "/rootfs";

// from switch_root.c
extern "C" int switchroot(const char *newroot);

static void make_dir(const char *path)
{
    if ((mkdir(path, 0777) < 0) && (errno != EEXIST))
        THROW_ERRNO("Failed to mkdir %s", path);
}

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
}

static void mount_rootfs(void)
{
    const char *rootfs_dev = "/dev/mmcblk0p2"; // TODO: find this in /proc/cmdline
    if ((mkdir(rootfs_mountpoint, 0777) < 0) && (errno != EEXIST))
        THROW_ERRNO("Failed to mkdir %s", rootfs_mountpoint);

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

    const stringvec rootfs_types{"squashfs", "ext4"}; // TODO: find this from /proc/filesystems
    for (const auto& type : rootfs_types)
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
    for (const auto& type : rootfs_types)
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
