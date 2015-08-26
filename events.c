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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <glib.h>

#include "events.h"
#include "messages.h"

static struct
{
    GAsyncQueue *from_user_to_thread_queue;
    GAsyncQueue *from_thread_to_user_queue;
}
events_data;

void events_init(void)
{
    events_data.from_user_to_thread_queue = g_async_queue_new();
    events_data.from_thread_to_user_queue = g_async_queue_new();
}

void events_deinit(void)
{
    g_async_queue_unref(events_data.from_user_to_thread_queue);
    g_async_queue_unref(events_data.from_thread_to_user_queue);
}

static struct EventFromUser *alloc_from_user(enum EventFromUserID id)
{
    struct EventFromUser *ev = g_try_malloc0(sizeof(*ev));

    if(ev != NULL)
        ev->event_id = id;

    return ev;
}

static struct EventToUser *alloc_to_user(enum EventToUserID id,
                                         struct XferItem *item)
{
    struct EventToUser *ev = g_try_malloc0(sizeof(*ev));

    if(ev != NULL)
    {
        ev->event_id = id;
        ev->item = item;
    }

    return ev;
}

struct EventFromUser *events_from_user_new_shutdown(void)
{
    return alloc_from_user(EVENT_FROM_USER_SHUTDOWN);
}

struct EventFromUser *events_from_user_new_start_download(struct XferItem *item)
{
    log_assert(item != NULL);
    log_assert(item->item_id > 0);
    log_assert(item->url != NULL);

    struct EventFromUser *ev = alloc_from_user(EVENT_FROM_USER_START_DOWNLOAD);

    if(ev != NULL)
        ev->d.item = item;

    return ev;
}

struct EventFromUser *events_from_user_new_cancel(uint32_t item_id)
{
    struct EventFromUser *ev = alloc_from_user(EVENT_FROM_USER_CANCEL);

    if(ev != NULL)
        ev->d.item_id = item_id;

    return ev;
}

void events_from_user_send(struct EventFromUser *event)
{
    log_assert(event != NULL);

    g_async_queue_push(events_data.from_user_to_thread_queue, event);
}

struct EventFromUser *events_from_user_receive(bool blocking)
{
    struct EventFromUser *ev = blocking
        ? g_async_queue_pop(events_data.from_user_to_thread_queue)
        : g_async_queue_try_pop(events_data.from_user_to_thread_queue);

    return ev;
}

void events_from_user_free(struct EventFromUser *event)
{
    if(event == NULL)
        return;

    switch(event->event_id)
    {
      case EVENT_FROM_USER_SHUTDOWN:
      case EVENT_FROM_USER_CANCEL:
        break;

      case EVENT_FROM_USER_START_DOWNLOAD:
        xferitem_free(event->d.item);
        break;

    }

    g_free(event);
}

struct EventToUser *events_to_user_new_report_progress(struct XferItem *item,
                                                       uint32_t tick)
{
    struct EventToUser *ev = alloc_to_user(EVENT_TO_USER_REPORT_PROGRESS, item);

    if(ev != NULL)
        ev->d.tick = tick;

    return ev;
}

struct EventToUser *events_to_user_new_done(struct XferItem *item,
                                            enum DBusListsErrorCode error_code)
{
    struct EventToUser *ev = alloc_to_user(EVENT_TO_USER_DONE, item);

    if(ev != NULL)
        ev->d.error_code = error_code;

    return ev;
}

void events_to_user_send(struct EventToUser *event)
{
    log_assert(event != NULL);

    g_async_queue_push(events_data.from_thread_to_user_queue, event);
}

struct EventToUser *events_to_user_receive(bool blocking)
{
    struct EventToUser *ev = blocking
        ? g_async_queue_pop(events_data.from_thread_to_user_queue)
        : g_async_queue_try_pop(events_data.from_thread_to_user_queue);

    return ev;
}

void events_to_user_free(struct EventToUser *event, bool force_free_xferitem)
{
    if(event == NULL)
       return;

    if(force_free_xferitem)
        xferitem_free(event->item);
    else
    {
        switch(event->event_id)
        {
          case EVENT_TO_USER_REPORT_PROGRESS:
            break;

          case EVENT_TO_USER_DONE:
            xferitem_free(event->item);
            break;
        }
    }

    g_free(event);
}
