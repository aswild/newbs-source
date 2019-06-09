/**********************************************************************
 * fsmagic.c - utilities to figure out the filesystem type to use
 * when mounting a device.
 * Currently just a limited home-made implementation, but could be
 * updated with util-linux's libblkid for more features.
 *
 * Copyright 2019 Allen Wild <allenwild93@gmail.com>
 * SPDX-License-Identifier: GPL-2.0
 **********************************************************************/

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "newbs_init.h"

struct fsmagic
{
    const char *name;
    const uint8_t magic[4]; // at least as big as the longest magic
    size_t magic_len;
    size_t magic_offset;
};

static const struct fsmagic magics[] = {
    {
        .name = "squashfs",
        .magic = {0x68, 0x73, 0x71, 0x73}, // 0x73717368 "sqsh", little-endian
        .magic_len = 4,
        .magic_offset = 0,
    },
    {
        .name = "ext4",
        .magic = {0x53, 0xef}, // 0xEF53, little-endian
        .magic_len = 2,
        .magic_offset = 1024 + 0x38,
    },
    {
        .name = "xfs",
        .magic = {0x58, 0x46, 0x53, 0x42}, // "XFSB"
        .magic_len = 4,
        .magic_offset = 0,
    },
    {NULL, {0}, 0, 0},
};

static bool check_magic(const void *buf, size_t buf_len, const struct fsmagic *magic)
{
    if (!buf || !buf_len || !magic)
    {
        log_error("check_magic called with invalid argument(s)");
        return false;
    }

    if ((magic->magic_offset + magic->magic_len) > buf_len)
        return false;

    return !memcmp(buf + magic->magic_offset, magic->magic, magic->magic_len);
}

const char* get_fstype(const char *device)
{
    if (!device)
    {
        log_error("get_fstype called with NULL argument");
        return NULL;
    }

    char buf[2048] = {0};
    int fd = open(device, O_RDONLY);
    if (fd == -1)
    {
        log_error_errno("get_fstype: failed to open device %s", device);
        return NULL;
    }

    ssize_t rd = read(fd, buf, sizeof(buf));
    if (rd < 0)
    {
        log_error_errno("get_fstype: failed to read from %s", device);
        close(fd);
        return NULL;
    }
    else if ((size_t)rd < sizeof(buf))
    {
        log_warning("get_fstype: read only %zd/%zu bytes from %s", rd, sizeof(buf), device);
    }
    close(fd);

    for (const struct fsmagic *magic = magics; magic->name != NULL; magic++)
    {
        if (check_magic(buf, sizeof(buf), magic))
        {
            log_info("Found filesystem type %s for %s", magic->name, device);
            return magic->name;
        }
    }
    return NULL;
}
