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

#include "xferthread.h"
#include "events.h"
#include "messages.h"

static void do_download(const struct XferItem *item)
{
    log_assert(item != NULL);
    msg_info("Should start downloading URL \"%s\", ID %u",
             item->url, item->item_id);
}

static struct
{
    struct EventFromUser *current_xfer_event;
}
xferthread_data;

static gpointer xferthread_main(gpointer data)
{
    while(1)
    {
        xferthread_data.current_xfer_event = events_from_user_receive(true);

        if(xferthread_data.current_xfer_event == NULL)
            continue;

        switch(xferthread_data.current_xfer_event->event_id)
        {
          case EVENT_FROM_USER_SHUTDOWN:
            events_from_user_free(xferthread_data.current_xfer_event);
            return NULL;

          case  EVENT_FROM_USER_START_DOWNLOAD:
            do_download(xferthread_data.current_xfer_event->d.item);
            break;

          case EVENT_FROM_USER_CANCEL:
            /* spurious cancel, doesn't make sense at this point */
            break;
        }

        events_from_user_free(xferthread_data.current_xfer_event);
    }
}

static GThread *thread;

void xferthread_init(void)
{
    log_assert(thread == NULL);
    thread = g_thread_new("Transfer thread", xferthread_main, NULL);
}

void xferthread_deinit(void)
{
    log_assert(thread != NULL);

    unsigned int tries = 10;

    while(1)
    {
        struct EventFromUser *shutdown_event = events_from_user_new_shutdown();

        if(shutdown_event == NULL)
        {
            if(--tries == 0)
                break;
            else
                g_thread_yield();
        }
        else
        {
            events_from_user_send(shutdown_event);
            break;
        }
    }

    if(tries > 0)
        g_thread_join(thread);
    else
        msg_error(0, LOG_ERR, "Failed joining thread, just quitting now.");

    g_thread_unref(thread);
    thread = NULL;
}
