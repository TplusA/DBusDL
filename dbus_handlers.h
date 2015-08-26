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

#ifndef DBUS_HANDLERS_H
#define DBUS_HANDLERS_H

#include "filetransfer_dbus.h"

#ifdef __cplusplus
extern "C" {
#endif

gboolean dbusmethod_download_start(tdbusFileTransfer *object,
                                   GDBusMethodInvocation *invocation,
                                   const gchar *url, guint ticks);
gboolean dbusmethod_transfer_cancel(tdbusFileTransfer *object,
                                    GDBusMethodInvocation *invocation,
                                    guint item_id);

#ifdef __cplusplus
}
#endif

#endif /* !DBUS_HANDLERS_H */
