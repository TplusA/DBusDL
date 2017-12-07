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

#include <string.h>

#include "dbus_iface.h"
#include "dbus_handlers.h"
#include "filetransfer_dbus.h"
#include "events.h"
#include "messages.h"

struct dbus_data
{
    guint owner_id;
    int name_acquired;

    tdbusFileTransfer *filetransfer_iface;
};

static int handle_dbus_error(GError **error)
{
    if(*error == NULL)
        return 0;

    if((*error)->message != NULL)
        msg_error(0, LOG_EMERG, "Got D-Bus error: %s", (*error)->message);
    else
        msg_error(0, LOG_EMERG, "Got D-Bus error without any message");

    g_error_free(*error);
    *error = NULL;

    return -1;
}

static void try_export_iface(GDBusConnection *connection,
                             GDBusInterfaceSkeleton *iface)
{
    GError *error = NULL;

    g_dbus_interface_skeleton_export(iface, connection, "/de/tahifi/DBusDL", &error);

    (void)handle_dbus_error(&error);
}

static void bus_acquired(GDBusConnection *connection,
                         const gchar *name, gpointer user_data)
{
    struct dbus_data *data = user_data;

    msg_info("D-Bus \"%s\" acquired", name);

    data->filetransfer_iface = tdbus_file_transfer_skeleton_new();

    g_signal_connect(data->filetransfer_iface, "handle-download",
                     G_CALLBACK(dbusmethod_download_start), NULL);
    g_signal_connect(data->filetransfer_iface, "handle-cancel",
                     G_CALLBACK(dbusmethod_transfer_cancel), NULL);

    try_export_iface(connection, G_DBUS_INTERFACE_SKELETON(data->filetransfer_iface));
}

static void name_acquired(GDBusConnection *connection,
                          const gchar *name, gpointer user_data)
{
    struct dbus_data *data = user_data;

    msg_info("D-Bus name \"%s\" acquired", name);
    data->name_acquired = 1;
}

static void name_lost(GDBusConnection *connection,
                      const gchar *name, gpointer user_data)
{
    struct dbus_data *data = user_data;

    msg_info("D-Bus name \"%s\" lost", name);
    data->name_acquired = -1;
}

static void destroy_notification(gpointer user_data)
{
    msg_info("Bus destroyed.");
}

static struct dbus_data dbus_data;

gboolean dbus_poll_event_queue(gpointer user_data)
{
    while(1)
    {
        struct EventToUser *event = events_to_user_receive(false);

        if(event == NULL)
            return G_SOURCE_REMOVE;

        switch(event->event_id)
        {
          case EVENT_TO_USER_REPORT_PROGRESS:
            {
                const struct XferItem *item = event->xi.const_item;

                tdbus_file_transfer_emit_progress(dbus_data.filetransfer_iface,
                                                  item->item_id,
                                                  event->d.tick,
                                                  item->total_ticks);
            }

            break;

          case EVENT_TO_USER_DONE:
            {
                const struct XferItem *item = event->xi.const_item;
                const char *path = event->d.error_code == LIST_ERROR_OK
                    ? item->destfile_path
                    : "";

                tdbus_file_transfer_emit_done(dbus_data.filetransfer_iface,
                                              item->item_id,
                                              event->d.error_code, path);
            }

            break;
        }

        events_to_user_free(event, false);
    }
}

int dbus_setup(GMainLoop *loop, const char *bus_name)
{
    memset(&dbus_data, 0, sizeof(dbus_data));

    dbus_data.owner_id =
        g_bus_own_name(G_BUS_TYPE_SESSION, bus_name,
                       G_BUS_NAME_OWNER_FLAGS_NONE,
                       bus_acquired, name_acquired, name_lost, &dbus_data,
                       destroy_notification);

    log_assert(dbus_data.owner_id != 0);

    while(dbus_data.owner_id == 0 || dbus_data.name_acquired == 0)
    {
        /* do whatever has to be done behind the scenes until one of the
         * guaranteed callbacks gets called for each bus */
        g_main_context_iteration(NULL, TRUE);
    }

    if(dbus_data.owner_id > 0 && dbus_data.name_acquired < 0)
    {
        msg_error(0, LOG_EMERG, "Failed acquiring D-Bus name on session bus");
        return -1;
    }

    g_main_loop_ref(loop);

    return 0;
}

void dbus_shutdown(GMainLoop *loop)
{
    if(loop == NULL)
        return;

    if(dbus_data.owner_id > 0)
        g_bus_unown_name(dbus_data.owner_id);

    g_main_loop_unref(loop);
}
