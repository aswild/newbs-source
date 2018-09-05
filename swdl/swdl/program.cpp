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

#include <cstdlib>

#include "newbs-swdl.h"

// TODO: these should all be configurable instead of hard-coded
static inline string get_boot_dev(void)
{
#ifdef SWDL_TEST
    return "/dev/null";
#else
    return "/dev/mmcblk0p1";
#endif
}

static inline string _get_inactive_dev(const stringvec& cmdline)
{
#ifdef SWDL_TEST
    (void)cmdline;
    return "/dev/null";
#else
    return get_inactive_dev(cmdline);
#endif
}

static inline string get_boot_dir(void)
{
#ifdef SWDL_TEST
    char test_boot_dir[] = "./boot.XXXXXX"; // modified by mkdtemp, must be non-const
    if (mkdtemp(test_boot_dir) == NULL)
        THROW_ERRNO("mkdtemp failed for test boot directory");
    log_info("SWDL_TEST: using temporary directory %s as boot dir", test_boot_dir);
    return test_boot_dir;
#else
    return "/boot";
#endif
}

static void program_raw(CPipe& curl, const nimg_phdr_t *p, const string& dev)
{
    // dummy for now
    (void)curl;
    log_info("program raw part type %d to %s", p->type, dev.c_str());
}

static void program_boot_tar(CPipe& curl, const nimg_phdr_t *p, const string& bootdir)
{
    // dummy for now
    (void)curl;
    log_info("program boot tar part type %d to %s", p->type, bootdir.c_str());
}

// program a partition with the given header and check the CRC
// throw an exception if anything goes wrong
void program_part(CPipe& curl, const nimg_phdr_t *p, const stringvec& cmdline)
{
    nimg_ptype_e type = static_cast<nimg_ptype_e>(p->type);
    if (type > NIMG_PTYPE_LAST)
        THROW_ERROR("invalid part type %u", type);

    log_info("program part type %s", part_name_from_type(type));
    switch (type)
    {
        case NIMG_PTYPE_BOOT_IMG:
            program_raw(curl, p, get_boot_dev());
            break;

        case NIMG_PTYPE_ROOTFS:
        case NIMG_PTYPE_ROOTFS_RW:
            program_raw(curl, p, _get_inactive_dev(cmdline));
            break;

        case NIMG_PTYPE_BOOT_TAR:
        case NIMG_PTYPE_BOOT_TARGZ:
        case NIMG_PTYPE_BOOT_TARXZ:
            program_boot_tar(curl, p, get_boot_dir());
            break;

        default:
            // shouldn't actually get here because we checked the part type
            // earlier, but adding these cases satisfies "enumeration values
            // not handled in switch" warnings
            THROW_ERROR("invalid part type in switch");
    }
}
