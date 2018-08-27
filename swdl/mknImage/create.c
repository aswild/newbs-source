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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mknImage.h"

typedef struct {
    const char   *filename;
    nimg_ptype_e type;
} fileinfo_t;

static const char *img_filename = NULL;
static FILE *img_fp;
static bool create_success = false;

void cmd_help_create(void)
{
    static const char msg[] =
        "    Create an nImage.\n"
        "    usage: mknImage create IMAGE_FILE TYPE1:FILE1 [TYPE2:FILE2]...\n"
        "      FILE:  Output image file (can't use stdout)\n"
        "      TYPEn: Image type. Valid options are: kernel, boot, rootfs, rootfs_rw\n"
        "      FILEn: Input partition data filename\n"
        "    Type kernel is the bare Linux kernel image. Type boot is the full boot partition\n"
    "";
    fputs(msg, stdout);
}

static void cleanup(void)
{
    static bool cleaning_up = false;
    if (!cleaning_up)
    {
        cleaning_up = true;
        if (!create_success)
        {
            log_info("failed to create image, cleaning up...");
            if (img_fp != NULL)
                fclose(img_fp);

            if (img_filename != NULL)
                if (unlink(img_filename) < 0)
                    log_warn("failed to delete '%s' in cleanup handler: %s", img_filename, strerror(errno));

            img_fp = NULL;
            img_filename = NULL;
        }
    }
}

static void cleanup_sighand(int sig)
{
    cleanup();
    signal(sig, SIG_DFL);
    raise(sig);
}

static void register_cleanup(void)
{
    signal(SIGINT,  cleanup_sighand);
    signal(SIGTERM, cleanup_sighand);
    signal(SIGABRT, cleanup_sighand);
    atexit(cleanup);
}

static int init_fileinfo(fileinfo_t *f, const char *arg)
{
    char *colon = strchr(arg, ':');
    if (colon == NULL)
    {
        log_error("invalid format for argument '%s'", arg);
        return -1;
    }
    *colon = '\0';

    nimg_ptype_e type = part_type_from_name(arg);
    if (type == NIMG_PART_TYPE_INVALID)
    {
        log_error("invalid partition type '%s'", arg);
        return -1;
    }

    f->type = type;
    f->filename = colon + 1;
    return 0;
}

int cmd_create(int argc, char **argv)
{
    if (argc < 3)
        DIE_USAGE("create: not enough arguments");

    img_filename = argv[1];
    argc -= 2;
    argv += 2;

    if (argc > NIMG_MAX_PHDRS)
        DIE("too many image parts %d, max is %d", argc, NIMG_MAX_PHDRS);

    fileinfo_t *files = malloc(argc * sizeof(fileinfo_t));
    assert(files != NULL);

    for (int i = 0; i < argc; i++)
        if (init_fileinfo(&files[i], argv[i]) < 0)
            return 1;

    nimg_hdr_t hdr;
    nimg_hdr_init(&hdr);
    hdr.n_parts = argc;

    log_info("Creating image %s", img_filename);
    img_fp = fopen(img_filename, "w");
    if (img_fp == NULL)
        DIE_ERRNO("unable to open '%s' for writing", img_filename);
    register_cleanup();

    void *dummy_hdr = malloc(sizeof(nimg_hdr_t));
    assert(dummy_hdr != NULL);
    if (fwrite(dummy_hdr, sizeof(nimg_hdr_t), 1, img_fp) != 1)
        DIE_ERRNO("failed to write blank image header");
    free(dummy_hdr);

    uint64_t parts_bytes = 0;
    const size_t buf_size = 8192;
    uint8_t *buf = malloc(buf_size);
    assert(buf != NULL);
    for (int i = 0; i < argc; i++)
    {
        struct stat sb;
        if (stat(files[i].filename, &sb) < 0)
            DIE_ERRNO("failed to stat '%s'", files[i].filename);

        FILE *part_fp = fopen(files[i].filename, "r");
        if (part_fp == NULL)
            DIE_ERRNO("failed to open '%s' for reading", files[i].filename);

        uint32_t crc = 0;
        size_t count = file_copy_crc32(&crc, (long)sb.st_size, part_fp, img_fp);
        fclose(part_fp);
        if (count != (size_t)sb.st_size)
        {
            if (ferror(part_fp))
            {
                DIE_ERRNO("failed to read from '%s'", files[i].filename);
            }
            else if (ferror(img_fp))
            {
                DIE_ERRNO("failed to read from '%s'", files[i].filename);
            }
            else
            {
                DIE("expected to read %zu bytes but got only %zu from '%s'",
                    (size_t)sb.st_size, count, files[i].filename);
            }
        }

        hdr.parts[i].magic  = NIMG_PHDR_MAGIC;
        hdr.parts[i].size   = sb.st_size;
        hdr.parts[i].offset = parts_bytes;
        hdr.parts[i].type   = files[i].type;
        hdr.parts[i].crc32  = crc;
        parts_bytes += sb.st_size;

        if (log_level >= LOG_LEVEL_INFO)
        {
            fprintf(stderr, "Part %d\n  file:   %s\n", i, files[i].filename);
            print_part_info(&hdr.parts[i], "  ", stderr);

        }
    }
    free(buf);
    free(files);

    rewind(img_fp);
    if (fwrite(&hdr, NIMG_HDR_SIZE, 1, img_fp) != 1)
        DIE_ERRNO("failed to write final image header");

    create_success = true;
    fclose(img_fp);
    img_fp = NULL;
    img_filename = NULL;

    return 0;
}
