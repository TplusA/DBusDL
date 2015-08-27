/*
 * Copyright (C) 2015  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of D-Bus DL.
 *
 * D-Bus DL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3 as
 * published by the Free Software Foundation.
 *
 * D-Bus DL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with D-Bus DL.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef XFERITEM_H
#define XFERITEM_H

#include <stdint.h>
#include <stdbool.h>

struct XferItem
{
    uint32_t item_id;
    uint32_t total_ticks;
    char *url;
};

#ifdef __cplusplus
extern "C" {
#endif

void xferitem_init(const char *download_path, bool create_path);
void xferitem_deinit(void);

struct XferItem *xferitem_allocate(const char *url, uint32_t ticks);
void xferitem_free(struct XferItem *item);

#ifdef __cplusplus
}
#endif

#endif /* !XFERITEM_H */
