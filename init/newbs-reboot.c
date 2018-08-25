/*******************************************************************************
 * Copyright 2017 Allen Wild <allenwild93@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *******************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include "newbs-util.h"

// Don't actually try to reboot if we're compiling natively to test
#ifdef __arm__
#include <linux/reboot.h>
#include <sys/reboot.h>
#include <sys/syscall.h>
#endif

int newbs_reboot(int argc, char **argv)
{
    long cmd = 0;
    char buf[3] = {0}; // 2 digits and a nullbyte

    if (argc > 0)
    {
        int err = check_strtol(argv[0], 0, &cmd);
        if (err || cmd < 0 || cmd > 63)
        {
            ERROR("Invalid reboot command: '%s'", argv[0]);
            return 1;
        }
    }

    snprintf(buf, sizeof(buf), "%ld", cmd);

#ifdef __arm__
    syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART2, buf);
#else
    INFO("Reboot with command %s", buf);
#endif
    return 0;
}
