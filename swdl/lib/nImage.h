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

#ifndef NIMAGE_H
#define NIMAGE_H

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "mknImage requires a little-endian machine"
#endif

#ifdef __cplusplus
#define BEGIN_DECLS extern "C" {
#define END_DECLS   }

#else
#define BEGIN_DECLS
#define END_DECLS

// make sure we have static_assert available
// apparently parsing doesn't stop at the first #error,
// so define an empty macro to avoid extra errors when
// we use it below
#if !defined(__GNUC__) || !defined(__USE_ISOC11) || !defined(static_assert)
#define static_assert(cond, msg)
#error "nImage requires a GNU C11 compiler and static_assert"
#endif
#endif // __cplusplus

/*******************************************************************************
 * NIMAGE DEFINES
 ******************************************************************************/

#define NIMG_HDR_MAGIC  0x474d49534257454eULL /* "NEWBSIMG" */
#define NIMG_PHDR_MAGIC 0x54524150474d494eULL /* "NIMGPART" */
#define NIMG_VER_MAJOR  1
#define NIMG_VER_MINOR  0

#define NIMG_HDR_SIZE   1024
#define NIMG_PHDR_SIZE  32
#define NIMG_NAME_LEN   128
#define NIMG_MAX_PARTS  27

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

typedef struct __attribute__((packed)) {
    uint64_t magic;
    uint64_t size;
    uint64_t offset; // offset 0 is the first byte after the image header
    uint8_t  type;   // nimg_type_e
    uint8_t  unused[3];
    uint32_t crc32;
} nimg_phdr_t;
static_assert(sizeof(nimg_phdr_t) == NIMG_PHDR_SIZE, "wrong size for nimg_phdr_t");

typedef struct __attribute__((packed)) {
    uint64_t    magic;
    uint8_t     ver_major;
    uint8_t     ver_minor;
    uint8_t     n_parts;
    uint8_t     unused1;
    uint32_t    unused2;
    char        name[NIMG_NAME_LEN];
    nimg_phdr_t parts[NIMG_MAX_PARTS];
    uint8_t     unused3[12];
    uint32_t    hdr_crc32;
} nimg_hdr_t;
static_assert(sizeof(nimg_hdr_t) == NIMG_HDR_SIZE, "wrong size for nimg_hdr_t");

/*******************************************************************************
 * LOGGING
 ******************************************************************************/

typedef enum {
    LOG_LEVEL_NONE,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
} log_level_e;
extern log_level_e log_level;

BEGIN_DECLS
extern void log_error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
extern void log_warn (const char *fmt, ...) __attribute__((format(printf, 1, 2)));
extern void log_info (const char *fmt, ...) __attribute__((format(printf, 1, 2)));
extern void log_debug(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
END_DECLS

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

/*******************************************************************************
 * HELPERS AND COMMON FUNCTIONS
 ******************************************************************************/

#ifdef __cplusplus
// fancy C++ inline template functions for min/max
// We get sensible compiler warnings if trying to compare signed/unsigned
// values or other incompatible types.
template<typename Ta, typename Tb>
static inline auto min(Ta a, Tb b)
{ return a < b ? a : b; }

template<typename Ta, typename Tb>
static inline auto max(Ta a, Tb b)
{ return a > b ? a : b; }

#else
// C macros for min/max using GCC's typeof extension
#define min(a, b) \
  ({ typeof (a) _a = (a); \
     typeof (b) _b = (b); \
     _a < _b ? _a : _b; })

#define max(a, b) \
  ({ typeof (a) _a = (a); \
     typeof (b) _b = (b); \
     _a > _b ? _a : _b; })

#endif // __cplusplus

BEGIN_DECLS
// from libiberty crc32.c
extern void xcrc32(uint32_t *_crc, const uint8_t *buf, ssize_t len);

// from common.c
nimg_ptype_e part_type_from_name(const char *name);
const char * part_name_from_type(nimg_ptype_e id);
void nimg_hdr_init(nimg_hdr_t *h);
ssize_t file_copy_crc32(uint32_t *crc, long len, int fd_in, int fd_out);
void print_part_info(nimg_phdr_t *p, const char *prefix, FILE *fp);
int check_strtol(const char *str, int base, long *value);
extern FILE * open_file(const char *name, const char *mode);
END_DECLS

#endif // NIMAGE_H
