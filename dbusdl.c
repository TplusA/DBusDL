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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <glib-unix.h>

#include "dbus_iface.h"
#include "events.h"
#include "xferthread.h"
#include "messages.h"
#include "versioninfo.h"

ssize_t (*os_read)(int fd, void *dest, size_t count) = read;
ssize_t (*os_write)(int fd, const void *buf, size_t count) = write;

static GMainLoop *create_glib_main_loop(void)
{
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    if(loop == NULL)
        msg_error(ENOMEM, LOG_EMERG, "Failed creating GLib main loop");

    return loop;
}

static void show_version_info(void)
{
    printf("%s\n"
           "Revision %s%s\n"
           "         %s+%d, %s\n",
           PACKAGE_STRING,
           VCS_FULL_HASH, VCS_WC_MODIFIED ? " (tainted)" : "",
           VCS_TAG, VCS_TICK, VCS_DATE);
}

static void log_version_info(void)
{
    msg_info("Rev %s%s, %s+%d, %s",
             VCS_FULL_HASH, VCS_WC_MODIFIED ? " (tainted)" : "",
             VCS_TAG, VCS_TICK, VCS_DATE);
}

static int setup(bool run_in_foreground)
{
    msg_enable_syslog(!run_in_foreground);

    if(!run_in_foreground)
        openlog("dbusdl", LOG_PID, LOG_DAEMON);

    if(!run_in_foreground)
    {
        if(daemon(0, 0) < 0)
        {
            msg_error(errno, LOG_EMERG, "Failed to run as daemon");
            return -1;
        }
    }

    log_version_info();

    return 0;
}

static void usage(const char *program_name)
{
    printf("Usage: %s [options]\n"
           "\n"
           "Options:\n"
           "  --help         Show this help.\n"
           "  --version      Print version information to stdout.\n"
           "  --fg           Run in foreground, don't run as daemon.\n"
           "  --tmpdir PATH  Download files to directory PATH.\n",
           program_name);
}

struct Parameters
{
    bool run_in_foreground;
    const char *download_path;
};

static int process_command_line(int argc, char *argv[],
                                struct Parameters *parameters)
{
    parameters->run_in_foreground = false;
    parameters->download_path = "/tmp/downloads";

#define CHECK_ARGUMENT() \
    do \
    { \
        if(i + 1 >= argc) \
        { \
            fprintf(stderr, "Option %s requires an argument.\n", argv[i]); \
            return -1; \
        } \
        ++i; \
    } \
    while(0)

    for(int i = 1; i < argc; ++i)
    {
        if(strcmp(argv[i], "--help") == 0)
            return 1;
        else if(strcmp(argv[i], "--version") == 0)
            return 2;
        else if(strcmp(argv[i], "--fg") == 0)
            parameters->run_in_foreground = true;
        else if(strcmp(argv[i], "--tmpdir") == 0)
        {
            CHECK_ARGUMENT();
            parameters->download_path = argv[i];
        }
        else
        {
            fprintf(stderr, "Unknown option \"%s\". Please try --help.\n", argv[i]);
            return -1;
        }
    }

#undef CHECK_ARGUMENT

    return 0;
}

static gboolean signal_handler(gpointer user_data)
{
    g_main_loop_quit((GMainLoop *)user_data);
    return G_SOURCE_REMOVE;
}

static void connect_unix_signals(GMainLoop *loop)
{
    g_unix_signal_add(SIGINT, signal_handler, loop);
    g_unix_signal_add(SIGTERM, signal_handler, loop);
}

int main(int argc, char *argv[])
{
    static struct Parameters parameters;
    int ret = process_command_line(argc, argv, &parameters);

    if(ret == -1)
        return EXIT_FAILURE;
    else if(ret == 1)
    {
        usage(argv[0]);
        return EXIT_SUCCESS;
    }
    else if(ret == 2)
    {
        show_version_info();
        return EXIT_SUCCESS;
    }

    if(setup(parameters.run_in_foreground) < 0)
        return EXIT_FAILURE;

    xferitem_init(parameters.download_path, true);
    events_init(dbus_poll_event_queue);
    xferthread_init();

    GMainLoop *loop = create_glib_main_loop();

    dbus_setup(loop, "de.tahifi.DBusDL");

    connect_unix_signals(loop);
    g_main_loop_run(loop);

    msg_info("Shutting down");
    dbus_shutdown(loop);

    xferthread_deinit();
    events_deinit();
    xferitem_deinit();

    return EXIT_SUCCESS;
}
