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

#ifndef MKNIMAGE_H
#define MKNIMAGE_H

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "mknImage requires a little-endian machine"
#endif

// make sure we have static_assert available
// apparently parsing doesn't stop at the first #error,
// so define an empty macro to avoid extra errors when
// we use it below
#if !defined(__USE_ISOC11) || !defined(static_assert)
#define static_assert(cond, msg)
#error "mknImage requires a C11 compiler and static_assert"
#endif

typedef enum {
    LOG_LEVEL_NONE,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
} log_level_e;
extern log_level_e log_level;

extern void log_error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
extern void log_warn (const char *fmt, ...) __attribute__((format(printf, 1, 2)));
extern void log_info (const char *fmt, ...) __attribute__((format(printf, 1, 2)));
extern void log_debug(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#define DIE_USAGE(fmt, args...) do { \
        fprintf(stderr, "Error: " fmt "\nRun `mknImage -h` for usage information\n", ##args); \
        exit(2); \
    } while(0)

#define DIE(fmt, args...) do { \
        fprintf(stderr, "Error: " fmt "\n", ##args); \
        exit(1); \
    } while(0)

#define DIE_ERRNO(fmt, args...) do { \
        fprintf(stderr, "Error: " fmt ": %s\n", ##args, strerror(errno)); \
        exit(2); \
    } while(0)

#define NIMG_HDR_MAGIC  0x474d49534257454eULL /* "NEWBSIMG" */
#define NIMG_PHDR_MAGIC 0x54524150474d494eULL /* "NIMGPART" */

#define NIMG_HDR_SIZE   1024
#define NIMG_PHDR_SIZE  32
#define NIMG_MAX_PHDRS  31

typedef enum {
    NIMG_PART_TYPE_INVALID,
    NIMG_PART_TYPE_KERNEL,
    NIMG_PART_TYPE_BOOT,
    NIMG_PART_TYPE_ROOTFS,
    NIMG_PART_TYPE_ROOTFS_RW,

    NIMG_PART_TYPE_LAST,
    NIMG_PART_TYPE_LAST_VALID = NIMG_PART_TYPE_LAST - 1
} nimg_ptype_e;
static_assert(NIMG_PART_TYPE_LAST < 256, "Too many partition types defined");

typedef struct __attribute__((packed, aligned(8))) {
    uint64_t magic;
    uint64_t size;
    uint64_t offset;
    uint32_t flags; // bits 31-7: unused, bit 8: read/write, bits 0-7: type (nimg_ptype_e)
    uint32_t crc32;
} nimg_phdr_t;
static_assert(sizeof(nimg_phdr_t) == NIMG_PHDR_SIZE, "wrong size for nimg_phdr_t");

typedef struct __attribute__((packed, aligned(8))) {
    uint64_t    magic;
    uint8_t     ver_major;
    uint8_t     ver_minor;
    uint8_t     n_parts;
    uint8_t     unused1;
    uint32_t    unused2;
    nimg_phdr_t phdrs[NIMG_MAX_PHDRS];
    uint8_t     padding[16];
} nimg_hdr_t;
static_assert(sizeof(nimg_hdr_t) == NIMG_HDR_SIZE, "wrong size for nimg_hdr_t");

typedef struct {
    const char    *name;
    int(*handler)(int argc, char **argv);
    void(*help_func)(void);
} cmd_t;

// from libiberty crc32.c
extern void xcrc32(uint32_t *_crc, const uint8_t *buf, ssize_t len);

// from common.c
nimg_ptype_e part_type_from_name(const char *name);
const char * part_name_from_type(nimg_ptype_e id);
int check_strtol(const char *str, int base, long *value);
extern FILE * open_file(const char *name, const char *mode);

#define MIN(a, b) \
  ({ typeof (a) _a = (a); \
     typeof (b) _b = (b); \
     _a < _b ? _a : _b; })

#define MAX(a, b) \
  ({ typeof (a) _a = (a); \
     typeof (b) _b = (b); \
     _a > _b ? _a : _b; })

// command handlers with a side of macro magic
#define CMD_LIST(xform) \
    xform(create) \
    xform(crc32)

#define DECLARE_CMD_HANDLERS(name) \
    extern int  cmd_##name(int argc, char **argv); \
    extern void cmd_help_##name(void);

CMD_LIST(DECLARE_CMD_HANDLERS)

#define DECLARE_CMD_DATA(name) {#name, cmd_##name, cmd_help_##name},
#define DECLARE_CMD_TABLE(table_name) \
        cmd_t table_name[] = { \
            CMD_LIST(DECLARE_CMD_DATA) \
            {NULL, NULL, NULL} \
        }

#endif // MKNIMAGE_H
