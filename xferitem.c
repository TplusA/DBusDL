/*
 * Copyright (C) 2015, 2019  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of D-Bus DL.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <glib.h>
#include <errno.h>

#include "xferitem.h"
#include "messages.h"

static struct
{
    const char *download_path;
    char *tempfile_path;
    uint32_t next_free_id;
}
xferitem_data;

static char *construct_path(const char *prefix, uint32_t id)
{
    char buffer[32];
    g_snprintf(buffer, sizeof(buffer), "%010u.dbusdl", id);

    return g_build_filename(prefix, buffer, NULL);
}

static uint32_t next_id(void)
{
    uint32_t id = xferitem_data.next_free_id++;

    if(xferitem_data.next_free_id == 0)
        ++xferitem_data.next_free_id;

    return id;
}

void xferitem_init(const char *download_path, bool create_path)
{
    log_assert(download_path != NULL);

    xferitem_data.download_path = download_path;
    xferitem_data.tempfile_path = construct_path(download_path, 0);
    xferitem_data.next_free_id = 1;

    if(create_path &&
       g_mkdir_with_parents(xferitem_data.download_path, 0770) < 0)
    {
        msg_error(errno, LOG_ERR, "Failed creating directory \"%s\".",
                  xferitem_data.download_path);
    }
}

void xferitem_deinit(void)
{
    g_free(xferitem_data.tempfile_path);
    xferitem_data.tempfile_path = NULL;
}

struct XferItem *xferitem_allocate(const char *url, uint32_t ticks)
{
    log_assert(url != NULL);

    struct XferItem *const item = g_try_malloc(sizeof(*item));

    if(item == NULL)
    {
        msg_out_of_memory("XferItem");
        return NULL;
    }

    item->item_id = next_id();
    item->total_ticks = ticks;
    item->url = g_strdup(url);
    item->destfile_path =
        construct_path(xferitem_data.download_path, item->item_id);

    if(item->url == NULL || item->destfile_path == NULL)
    {
        xferitem_free(item);
        return NULL;
    }
    else
        return item;
}

void xferitem_free(struct XferItem *item)
{
    if(item == NULL)
        return;

    g_free(item->url);
    g_free(item->destfile_path);
    g_free(item);
}

const char *xferitem_get_tempfile_path(void)
{
    log_assert(xferitem_data.tempfile_path != NULL);
    return xferitem_data.tempfile_path;
}
