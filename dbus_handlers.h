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
