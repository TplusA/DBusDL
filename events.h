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

#ifndef EVENTS_H
#define EVENTS_H

#include <stdbool.h>

#include "xferitem.h"
#include "de_tahifi_lists_errors.h"

enum EventFromUserID
{
    EVENT_FROM_USER_SHUTDOWN,
    EVENT_FROM_USER_START_DOWNLOAD,
    EVENT_FROM_USER_CANCEL,
};

struct EventFromUser
{
    enum EventFromUserID event_id;

    union
    {
        struct XferItem *item;
        uint32_t item_id;
    }
    d;
};

enum EventToUserID
{
    EVENT_TO_USER_REPORT_PROGRESS,
    EVENT_TO_USER_DONE,
};

struct EventToUser
{
    enum EventToUserID event_id;

    union
    {
        struct XferItem *item;
        const struct XferItem *const_item;
    }
    xi;

    union
    {
        uint32_t tick;
        enum DBusListsErrorCode error_code;
    }
    d;
};

#ifdef __cplusplus
extern "C" {
#endif

void events_init(int (*to_user_queue_notification)(void *user_data));
void events_deinit(void);

struct EventFromUser *events_from_user_new_shutdown(void);
struct EventFromUser *events_from_user_new_start_download(struct XferItem *item);
struct EventFromUser *events_from_user_new_cancel(uint32_t item_id);
void events_from_user_send(struct EventFromUser *event);
struct EventFromUser *events_from_user_receive(bool blocking);
void events_from_user_free(struct EventFromUser *event);

struct EventToUser *events_to_user_new_report_progress(const struct XferItem *item,
                                                       uint32_t tick);
struct EventToUser *events_to_user_new_done(struct XferItem *item,
                                            enum DBusListsErrorCode error_code);
void events_to_user_send(struct EventToUser *event);
struct EventToUser *events_to_user_receive(bool blocking);
void events_to_user_free(struct EventToUser *event, bool force_free_xferitem);

#ifdef __cplusplus
}
#endif

#endif /* !EVENTS_H */
