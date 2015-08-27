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

#include <stdio.h>
#include <glib.h>
#include <curl/curl.h>
#include <errno.h>

#include "xferthread.h"
#include "events.h"
#include "messages.h"

static void send_progress_report(const struct XferItem *item, uint32_t tick)
{
    struct EventToUser *ev = events_to_user_new_report_progress(item, tick);

    if(ev != NULL)
        events_to_user_send(ev);
}

/*!
 * Send a Done event to user, pass ownership of XferItem to the event.
 *
 * In case the event could not be allocated, the \p item is freed. In any case,
 * \p item must not accessed by the caller anymore after calling this function.
 */
static void send_download_done(struct XferItem *item,
                               enum DBusListsErrorCode error_code)
{
    struct EventToUser *ev = events_to_user_new_done(item, error_code);

    if(ev != NULL)
        events_to_user_send(ev);
    else
    {
        msg_error(0, LOG_CRIT,
                  "Failed notifying main thread of end of download");
        xferitem_free(item);
    }
}

static bool download_must_be_canceled(struct EventFromUser *event,
                                      uint32_t current_item_id)
{
    if(event == NULL)
        return false;

    switch(event->event_id)
    {
      case EVENT_FROM_USER_START_DOWNLOAD:
      case EVENT_FROM_USER_SHUTDOWN:
        break;

      case EVENT_FROM_USER_CANCEL:
        if(event->d.item_id != current_item_id)
        {
            events_from_user_free(event);
            return false;
        }

        break;
    }

    return true;
}

struct ProgressCallbackData
{
    const struct XferItem *const current_xfer;
    struct EventFromUser *next_event;
    uint32_t previously_sent_tick;
};

static int progress_callback(void *clientp,
                             curl_off_t dltotal, curl_off_t dlnow,
                             curl_off_t ultotal, curl_off_t ulnow)
{
    struct ProgressCallbackData *data = clientp;
    struct EventFromUser *ev = events_from_user_receive(false);

    if(download_must_be_canceled(ev, data->current_xfer->item_id))
    {
        data->next_event = ev;
        return 1;
    }

    uint32_t tick = dltotal > 0
        ? (uint32_t)(data->current_xfer->total_ticks * ((double)dlnow / (double)dltotal))
        : 0;

    if((tick > data->previously_sent_tick ||
        data->previously_sent_tick == UINT32_MAX) &&
       tick <= data->current_xfer->total_ticks)
    {
        msg_info("Download progress %u/%u (%lu/%lu bytes)",
                 tick, data->current_xfer->total_ticks, dlnow, dltotal);
        send_progress_report(data->current_xfer, tick);
        data->previously_sent_tick = tick;
    }

    return 0;
}

static void remove_file(const char *filename)
{
    if(remove(filename) < 0)
        msg_error(errno, LOG_ERR, "Failed deleting file \"%s\"", filename);
}

static void close_and_remove(FILE *output_file, const char *filename)
{
    fclose(output_file);
    remove_file(filename);
}

/*!
 * Perform download of given URL, write to file with given name.
 *
 * In case of error, this function will remove any temporary files it may have
 * created.
 */
static enum DBusListsErrorCode do_download(const struct XferItem *const item,
                                           const char *const filename,
                                           struct EventFromUser **next_event)
{
    *next_event = NULL;

    FILE *output_file = fopen(filename, "wb");

    if(output_file == NULL)
    {
        msg_error(errno, LOG_ERR,
                  "Failed creating temporary file \"%s\"", filename);

        return LIST_ERROR_PHYSICAL_MEDIA_IO;
    }

    CURL *rx = curl_easy_init();
    if(rx == NULL)
    {
        msg_error(ENOENT, LOG_ERR, "Failed initializing cURL object");
        close_and_remove(output_file, filename);

        return LIST_ERROR_INTERNAL;
    }

    struct ProgressCallbackData data =
    {
        .current_xfer = item,
        .previously_sent_tick = UINT32_MAX,
    };

    char error_buffer[CURL_ERROR_SIZE] = "(details unknown)";

    curl_easy_setopt(rx, CURLOPT_URL, item->url);
    curl_easy_setopt(rx, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(rx, CURLOPT_MAXREDIRS, 0L);
    curl_easy_setopt(rx, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(rx, CURLOPT_WRITEDATA, output_file);
    curl_easy_setopt(rx, CURLOPT_XFERINFOFUNCTION, progress_callback);
    curl_easy_setopt(rx, CURLOPT_XFERINFODATA, &data);
    curl_easy_setopt(rx, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(rx, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(rx, CURLOPT_CONNECTTIMEOUT, 45L);
    curl_easy_setopt(rx, CURLOPT_ACCEPTTIMEOUT_MS, 45000L);
    curl_easy_setopt(rx, CURLOPT_ERRORBUFFER, error_buffer);

    const CURLcode rx_result = curl_easy_perform(rx);
    enum DBusListsErrorCode error =
        (rx_result == CURLE_OK) ? LIST_ERROR_OK : LIST_ERROR_NET_IO;

    curl_easy_cleanup(rx);
    rx = NULL;

    *next_event = data.next_event;

    if(error == LIST_ERROR_OK)
    {
        fclose(output_file);

        /* in case 100% completion has not been sent from the progress
         * callback for any reason, do it now for the sake of UX */
        if(data.previously_sent_tick != item->total_ticks)
            send_progress_report(item, item->total_ticks);
    }
    else
    {
        if(rx_result != CURLE_ABORTED_BY_CALLBACK)
            msg_error(0, LOG_ERR, "Failed downloading file: %s", error_buffer);
        else
        {
            msg_info("Download canceled as requested");
            error = LIST_ERROR_INTERRUPTED;
        }

        close_and_remove(output_file, filename);
    }

    return error;
}

/*!
 * Take ownership of #XferItem and attempt to download it.
 *
 * While the download proceeds, the event queue is regularly checked for new
 * messages. In case a cancellation request for the current transfer is
 * received, or in case a new download or application shutdown is requested,
 * the download is interrupted and any temporary files are removed.
 *
 * \returns
 *     The event that was read from the incoming queue and that caused
 *     interuption of the download, or \c NULL in case the download was not
 *     interrupted by an event. Note that \c NULL does not indicate a
 *     successful download.
 */
static struct EventFromUser *download(struct XferItem *item)
{
    log_assert(item != NULL);

    msg_info("Start downloading URL \"%s\", ID %u", item->url, item->item_id);

    struct EventFromUser *next_event;
    enum DBusListsErrorCode error =
        do_download(item, xferitem_get_tempfile_path(), &next_event);

    if(error == LIST_ERROR_OK)
    {
        if(rename(xferitem_get_tempfile_path(), item->destfile_path) < 0)
        {
            error = LIST_ERROR_PHYSICAL_MEDIA_IO;
            remove_file(xferitem_get_tempfile_path());
        }
        else
            msg_info("Finished downloading \"%s\" to \"%s\"",
                     item->url, item->destfile_path);
    }

    send_download_done(item, error);

    return next_event;
}

static struct
{
    struct EventFromUser *current_xfer_event;
}
xferthread_data;

static gpointer xferthread_main(gpointer data)
{
    struct EventFromUser *next_event = NULL;

    while(1)
    {
        xferthread_data.current_xfer_event =
            (next_event != NULL) ? next_event : events_from_user_receive(true);

        next_event = NULL;

        if(xferthread_data.current_xfer_event == NULL)
            continue;

        switch(xferthread_data.current_xfer_event->event_id)
        {
          case EVENT_FROM_USER_SHUTDOWN:
            events_from_user_free(xferthread_data.current_xfer_event);
            return NULL;

          case EVENT_FROM_USER_START_DOWNLOAD:
            next_event = download(xferthread_data.current_xfer_event->d.item);
            xferthread_data.current_xfer_event->d.item = NULL;
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

    curl_global_init(CURL_GLOBAL_DEFAULT);
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

    curl_global_cleanup();
}
