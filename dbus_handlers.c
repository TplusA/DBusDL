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

#include "dbus_handlers.h"
#include "events.h"
#include "messages.h"

static void enter_handler(GDBusMethodInvocation *invocation)
{
    static const char iface_name[] = "de.tahifi.FileTransfer";

    msg_info("%s method invocation from '%s': %s",
             iface_name, g_dbus_method_invocation_get_sender(invocation),
             g_dbus_method_invocation_get_method_name(invocation));
}

gboolean dbusmethod_download_start(tdbusFileTransfer *object,
                                   GDBusMethodInvocation *invocation,
                                   const gchar *url, guint ticks)
{
    enter_handler(invocation);

    struct XferItem *item = xferitem_allocate(url, ticks);
    bool failed = true;

    if(item != NULL)
    {
        struct EventFromUser *event =
            events_from_user_new_start_download(item);

        if(event != NULL)
        {
            tdbus_file_transfer_complete_download(object, invocation,
                                                  item->item_id);
            msg_info("Queue download of \"%s\", ID %u, ticks resolution %u",
                     item->url, item->item_id, item->total_ticks);
            events_from_user_send(event);
            failed = false;
        }
        else
            xferitem_free(item);
    }

    if(failed)
        g_dbus_method_invocation_return_error(invocation,
                                              G_DBUS_ERROR, G_DBUS_ERROR_NO_MEMORY,
                                              "Failed queuing download of URL \"%s\"", url);

    return TRUE;
}

gboolean dbusmethod_transfer_cancel(tdbusFileTransfer *object,
                                    GDBusMethodInvocation *invocation,
                                    guint item_id)
{
    enter_handler(invocation);

    struct EventFromUser *event;

    if(item_id == 0)
        g_dbus_method_invocation_return_error(invocation,
                                              G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                                              "Item ID 0 is invalid");
    else if((event = events_from_user_new_cancel(item_id)) == NULL)
        g_dbus_method_invocation_return_error(invocation,
                                              G_DBUS_ERROR, G_DBUS_ERROR_NO_MEMORY,
                                              "Failed creating cancel event");
    else
    {
        tdbus_file_transfer_complete_cancel(object, invocation);
        msg_info("Cancel download of ID %u", item_id);
        events_from_user_send(event);
    }

    return TRUE;
}
