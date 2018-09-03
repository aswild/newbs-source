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

#define NIMG_HDR_MAGIC   0x474d49534257454eULL /* "NEWBSIMG" */
#define NIMG_PHDR_MAGIC  0x54524150474d494eULL /* "NIMGPART" */
#define NIMG_HDR_VERSION 1

#define NIMG_HDR_SIZE   1024
#define NIMG_PHDR_SIZE  32
#define NIMG_NAME_LEN   128
#define NIMG_MAX_PARTS  27

// Important! Keep this enum and nimg_ptype_names in sync!
typedef enum {
    NIMG_PTYPE_INVALID,
    NIMG_PTYPE_BOOT_IMG,
    NIMG_PTYPE_BOOT_TAR,
    NIMG_PTYPE_BOOT_TARGZ,
    NIMG_PTYPE_BOOT_TARXZ,
    NIMG_PTYPE_ROOTFS,
    NIMG_PTYPE_ROOTFS_RW,

    NIMG_PTYPE_COUNT,
    NIMG_PTYPE_LAST = NIMG_PTYPE_COUNT - 1
} nimg_ptype_e;
static_assert(NIMG_PTYPE_COUNT < 256, "Too many partition types defined");

extern const char *nimg_ptype_names[];
#ifdef NIMG_DECLARE_PTYPE_NAMES
const char *nimg_ptype_names[] = {
    "invalid",
    "boot_img",
    "boot_tar",
    "boot_targz",
    "boot_tarxz",
    "rootfs",
    "rootfs_rw",
};
static_assert(sizeof(nimg_ptype_names) == (NIMG_PTYPE_COUNT * sizeof(char*)),
              "wrong number of elements  in nimg_ptype_names");
#endif

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
    uint8_t     version;
    uint8_t     n_parts;
    uint16_t    unused1;
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

typedef enum {
    NIMG_HDR_CHECK_SUCCESS = 0,
    NIMG_HDR_CHECK_BAD_MAGIC,
    NIMG_HDR_CHECK_BAD_VERSION,
    NIMG_HDR_CHECK_TOO_MANY_PARTS,
    NIMG_HDR_CHECK_BAD_CRC,
} nimg_hdr_check_e;

typedef enum {
    NIMG_PHDR_CHECK_SUCCESS = 0,
    NIMG_PHDR_CHECK_BAD_MAGIC,
    NIMG_PHDR_CHECK_BAD_TYPE,
} nimg_phdr_check_e;

BEGIN_DECLS
// from libiberty crc32.c
extern void xcrc32(uint32_t *_crc, const uint8_t *buf, ssize_t len);

// from common.c
nimg_ptype_e    part_type_from_name(const char *name);
const char*     part_name_from_type(nimg_ptype_e id);
void            nimg_hdr_init(nimg_hdr_t *h);
void            print_part_info(nimg_phdr_t *p, const char *prefix, FILE *fp);
nimg_hdr_check_e    nimg_hdr_check(const nimg_hdr_t *h);
nimg_phdr_check_e   nimg_phdr_check(const nimg_phdr_t *h);
const char*     nimg_hdr_check_str(nimg_hdr_check_e status);
const char*     nimg_phdr_check_str(nimg_phdr_check_e status);

ssize_t         file_copy_crc32(uint32_t *crc, long len, int fd_in, int fd_out);
int             check_strtol(const char *str, int base, long *value);
FILE*           open_file(const char *name, const char *mode);
size_t          read_n(int fd, void *buf, size_t count);
END_DECLS

#endif // NIMAGE_H
