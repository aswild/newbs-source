/*
 * switchroot.c - switch to new root directory and start init.
 *
 * Copyright 2002-2009 Red Hat, Inc.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *  Peter Jones <pjones@redhat.com>
 *  Jeremy Katz <katzj@redhat.com>
 */
/*
 * Modified for standalone use by Allen Wild
 */
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/param.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <getopt.h>

// BEGIN standalone compatibility
//#include "c.h"
//#include "nls.h"
//#include "closestream.h"
//#include "statfs_magic.h"
#define STATFS_TMPFS_MAGIC 0x01021994
#define STATFS_RAMFS_MAGIC 0x858458f6
#define F_TYPE_EQUAL(a, b) (a == (__typeof__(a)) b)

#define _(str) str
#define warn(fmt, args...) fprintf(stderr, fmt "\n", ##args)
#define warnx(fmt, args...) fprintf(stderr, fmt "\n", ##args)
// END standalone compatibility

#ifndef MS_MOVE
#define MS_MOVE 8192
#endif

#ifndef MNT_DETACH
#define MNT_DETACH       0x00000002 /* Just detach from the tree */
#endif

/* remove all files/directories below dirName -- don't cross mountpoints */
static int recursiveRemove(int fd)
{
    struct stat rb;
    DIR *dir;
    int rc = -1;
    int dfd;

    if (!(dir = fdopendir(fd))) {
        warn(_("failed to open directory"));
        goto done;
    }

    /* fdopendir() precludes us from continuing to use the input fd */
    dfd = dirfd(dir);

    if (fstat(dfd, &rb)) {
        warn(_("stat failed"));
        goto done;
    }

    while(1) {
        struct dirent *d;
        int isdir = 0;

        errno = 0;
        if (!(d = readdir(dir))) {
            if (errno) {
                warn(_("failed to read directory"));
                goto done;
            }
            break;  /* end of directory */
        }

        if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
            continue;
#ifdef _DIRENT_HAVE_D_TYPE
        if (d->d_type == DT_DIR || d->d_type == DT_UNKNOWN)
#endif
        {
            struct stat sb;

            if (fstatat(dfd, d->d_name, &sb, AT_SYMLINK_NOFOLLOW)) {
                warn(_("stat of %s failed"), d->d_name);
                continue;
            }

            /* skip if device is not the same */
            if (sb.st_dev != rb.st_dev)
                continue;

            /* remove subdirectories */
            if (S_ISDIR(sb.st_mode)) {
                int cfd;

                cfd = openat(dfd, d->d_name, O_RDONLY);
                if (cfd >= 0) {
                    recursiveRemove(cfd);
                    close(cfd);
                }
                isdir = 1;
            }
        }

        if (unlinkat(dfd, d->d_name, isdir ? AT_REMOVEDIR : 0))
            warn(_("failed to unlink %s"), d->d_name);
    }

    rc = 0; /* success */

done:
    if (dir)
        closedir(dir);
    return rc;
}

int switchroot(const char *newroot)
{
    /*  Don't try to unmount the old "/", there's no way to do it. */
    const char *umounts[] = { "/dev", "/proc", "/sys", "/run", NULL };
    int i;
    int cfd;
    pid_t pid;
    struct stat newroot_stat, sb;

    if (stat(newroot, &newroot_stat) != 0) {
        warn(_("stat of %s failed"), newroot);
        return -1;
    }

    for (i = 0; umounts[i] != NULL; i++) {
        char newmount[PATH_MAX];

        snprintf(newmount, sizeof(newmount), "%s%s", newroot, umounts[i]);

        if ((stat(newmount, &sb) != 0) || (sb.st_dev != newroot_stat.st_dev)) {
            /* mount point seems to be mounted already or stat failed */
            umount2(umounts[i], MNT_DETACH);
            continue;
        }

        if (mount(umounts[i], newmount, NULL, MS_MOVE, NULL) < 0) {
            warn(_("failed to mount moving %s to %s"),
                umounts[i], newmount);
            warnx(_("forcing unmount of %s"), umounts[i]);
            umount2(umounts[i], MNT_FORCE);
        }
    }

    if (chdir(newroot)) {
        warn(_("failed to change directory to %s"), newroot);
        return -1;
    }

    cfd = open("/", O_RDONLY);
    if (cfd < 0) {
        warn(_("cannot open %s"), "/");
        return -1;
    }

    if (mount(newroot, "/", NULL, MS_MOVE, NULL) < 0) {
        close(cfd);
        warn(_("failed to mount moving %s to /"), newroot);
        return -1;
    }

    if (chroot(".")) {
        close(cfd);
        warn(_("failed to change root"));
        return -1;
    }

    pid = fork();
    if (pid <= 0) {
        struct statfs stfs;

        if (fstatfs(cfd, &stfs) == 0 &&
            (F_TYPE_EQUAL(stfs.f_type, STATFS_RAMFS_MAGIC) ||
             F_TYPE_EQUAL(stfs.f_type, STATFS_TMPFS_MAGIC)))
            recursiveRemove(cfd);
        else
            warn(_("old root filesystem is not an initramfs"));
        if (pid == 0)
            exit(EXIT_SUCCESS);
    }

    close(cfd);
    return 0;
}