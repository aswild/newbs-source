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
#include <fcntl.h>
#include <unistd.h>

#include "mknImage.h"

// padding/alignment between images
#define PART_ALIGN 16
static const char part_align_buf[PART_ALIGN] = {0};

typedef struct {
    const char   *filename;
    nimg_ptype_e type;
} fileinfo_t;

static const char *img_filename = NULL;
static fileinfo_t *files = NULL;
static int img_fd = -1;
static bool create_success = false;

void cmd_help_create(void)
{
    static const char msg[] =
        "    Create an nImage.\n"
        "    usage: mknImage create -o IMAGE_FILE [-n NAME] TYPE1:FILE1 [TYPE2:FILE2]...\n"
        "      -o FILE: Output image file (must be a seekable file, not a pipe like stdout)\n"
        "      -n NAME: Name to embed in the image header (max %d chars)\n"
        "      TYPEn:   Image type\n"
        "      FILEn:   Input partition data filename\n"
        "    Valid image types are:\n"
        "      "
    "";
    printf(msg, NIMG_NAME_LEN);
    for (int i = 1; i < NIMG_PTYPE_COUNT; i++)
        printf("%s%c", nimg_ptype_names[i], (i == NIMG_PTYPE_COUNT-1) ? '\n' : ' ');
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
            if (img_fd != -1)
                close(img_fd);

            if (img_filename != NULL)
                if (unlink(img_filename) < 0)
                    log_warn("failed to delete '%s' in cleanup handler: %s", img_filename, strerror(errno));

            img_fd = -1;
            img_filename = NULL;

            free(files);
            files = NULL;
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
        log_error("invalid partition filename format: '%s'", arg);
        return -1;
    }
    *colon = '\0';

    nimg_ptype_e type = part_type_from_name(arg);
    if (type == NIMG_PTYPE_INVALID)
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
    char *img_name = NULL;
    int opt;
    optind = 1; // reset getopt state after main options parsing
    while ((opt = getopt(argc, argv, "o:n:")) != -1)
    {
        switch (opt)
        {
            case 'o':
                img_filename = optarg;
                break;
            case 'n':
                if (strlen(optarg) > NIMG_NAME_LEN)
                    DIE_USAGE("image name too long");
                img_name = optarg;
                break;
            default:
                DIE_USAGE("unknown option '%c'", opt);
                break;
        }
    }
    argc -= optind;
    argv += optind;

    if (img_filename == NULL)
        DIE_USAGE("create: the -o options is required");

    if (argc < 1)
        DIE_USAGE("create: no partitions specified");
    else if (argc > NIMG_MAX_PARTS)
        DIE("too many image parts %d, max is %d", argc, NIMG_MAX_PARTS);

    files = malloc(argc * sizeof(fileinfo_t));
    assert(files != NULL);

    for (int i = 0; i < argc; i++)
        if (init_fileinfo(&files[i], argv[i]) < 0)
            return 1;

    nimg_hdr_t hdr;
    nimg_hdr_init(&hdr);
    hdr.n_parts = argc;
    if (img_name != NULL)
        strncpy(hdr.name, img_name, NIMG_NAME_LEN);

    log_info("Creating image %s", img_filename);
    if (img_name != NULL)
        log_info("Image name is '%s'", img_name);

    img_fd = open(img_filename, O_WRONLY | O_CREAT, 0666);
    if (img_fd == -1)
        DIE_ERRNO("unable to open '%s' for writing", img_filename);
    register_cleanup();

    void *dummy_hdr = malloc(NIMG_HDR_SIZE);
    assert(dummy_hdr != NULL);
    if (write(img_fd, dummy_hdr, NIMG_HDR_SIZE) != NIMG_HDR_SIZE)
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

        int part_fd = open(files[i].filename, O_RDONLY);
        if (part_fd == -1)
            DIE_ERRNO("failed to open '%s' for reading", files[i].filename);

        uint32_t crc = 0;
        ssize_t count = file_copy_crc32(&crc, (long)sb.st_size, part_fd, img_fd);
        close(part_fd);
        if (count != sb.st_size)
        {
            if (count == -1)
                DIE_ERRNO("failed to read from '%s'", files[i].filename);
            else if (count == -2)
                DIE_ERRNO("failed to read from '%s'", files[i].filename);
            else
                DIE("expected to read %zu bytes but got only %zu from '%s'",
                    (size_t)sb.st_size, count, files[i].filename);
        }

        hdr.parts[i].magic  = NIMG_PHDR_MAGIC;
        hdr.parts[i].size   = sb.st_size;
        hdr.parts[i].offset = parts_bytes;
        hdr.parts[i].type   = files[i].type;
        hdr.parts[i].crc32  = crc;

        if (log_level >= LOG_LEVEL_INFO)
        {
            fprintf(stderr, "Part %d\n  file:   %s\n", i, files[i].filename);
            print_part_info(&hdr.parts[i], "  ", stderr);
        }

        parts_bytes += sb.st_size;
        unsigned int padding = (16 - (parts_bytes % 16)) % 16;
        log_debug("adding %u bytes of padding", padding);
        if (padding > 0)
        {
            if (write(img_fd, part_align_buf, padding) != (ssize_t)padding)
                DIE_ERRNO("failed to write %u padding bytes between images", padding);
            parts_bytes += padding;
        }
    }
    free(buf);
    free(files);

    // compute header CRC
    // use a temp variable rather than passing &hdr.hdr_crc32 to suppress
    // clang's address-of-packed-member warning (could return an unaligned pointer in a packed struct)
    uint32_t crc = 0;
    xcrc32(&crc, (const uint8_t*)&hdr, NIMG_HDR_SIZE-4);
    hdr.hdr_crc32 = crc;

    // seek back to beginning and add the real header
    if (lseek(img_fd, 0, SEEK_SET) == (off_t)(-1))
        DIE_ERRNO("failed to lseek to begining of image");
    if (write(img_fd, &hdr, NIMG_HDR_SIZE) != NIMG_HDR_SIZE)
        DIE_ERRNO("failed to write final image header");

    create_success = true;
    close(img_fd);

    return 0;
}
