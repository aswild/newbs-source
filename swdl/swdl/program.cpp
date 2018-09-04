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

#include "newbs-swdl.h"

void program_part(CPipe& curl, const nimg_phdr_t *phdr)
{
    nimg_ptype_e type = static_cast<nimg_ptype_e>(phdr->type);
    if (type > NIMG_PTYPE_LAST)
        THROW_ERROR("invalid part type %u", type);

    // dummy for now
    (void)curl;
    log_info("program part type %s", part_name_from_type(type));
}
