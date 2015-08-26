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
#include "messages.h"
#include "versioninfo.h"

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
           "  --fg           Run in foreground, don't run as daemon.\n",
           program_name);
}

static int process_command_line(int argc, char *argv[],
                                bool *run_in_foreground)
{
    *run_in_foreground = false;

    for(int i = 1; i < argc; ++i)
    {
        if(strcmp(argv[i], "--help") == 0)
            return 1;
        else if(strcmp(argv[i], "--version") == 0)
            return 2;
        else if(strcmp(argv[i], "--fg") == 0)
            *run_in_foreground = true;
        else
        {
            fprintf(stderr, "Unknown option \"%s\". Please try --help.\n", argv[i]);
            return -1;
        }
    }

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
    bool run_in_foreground;
    int ret = process_command_line(argc, argv, &run_in_foreground);

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

    if(setup(run_in_foreground) < 0)
        return EXIT_FAILURE;

    GMainLoop *loop = create_glib_main_loop();

    connect_unix_signals(loop);
    g_main_loop_run(loop);

    msg_info("Shutting down");

    return EXIT_SUCCESS;
}
