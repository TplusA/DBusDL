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
                 tick, data->current_xfer->total_ticks,
                 (unsigned long)dlnow, (unsigned long)dltotal);
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

static enum DBusListsErrorCode map_curl_error_to_list_error(CURLcode error)
{
    switch(error)
    {
      case CURLE_OK:
        return LIST_ERROR_OK;

      case CURLE_ABORTED_BY_CALLBACK:
        return LIST_ERROR_INTERRUPTED;

      case CURLE_WRITE_ERROR:
      case CURLE_READ_ERROR:
      case CURLE_FILE_COULDNT_READ_FILE:
      case CURLE_SSL_CACERT_BADFILE:
      case CURLE_SSL_CRL_BADFILE:
        return LIST_ERROR_PHYSICAL_MEDIA_IO;

      case CURLE_COULDNT_RESOLVE_PROXY:
      case CURLE_COULDNT_RESOLVE_HOST:
      case CURLE_COULDNT_CONNECT:
      case CURLE_FTP_ACCEPT_TIMEOUT:
      case CURLE_OPERATION_TIMEDOUT:
      case CURLE_LDAP_CANNOT_BIND:
      case CURLE_INTERFACE_FAILED:
      case CURLE_SEND_ERROR:
      case CURLE_RECV_ERROR:
      case CURLE_TFTP_NOTFOUND:
      case CURLE_REMOTE_DISK_FULL:
      case CURLE_REMOTE_FILE_EXISTS:
      case CURLE_REMOTE_FILE_NOT_FOUND:
      case CURLE_SSH:
        return LIST_ERROR_NET_IO;

      case CURLE_UNSUPPORTED_PROTOCOL:
      case CURLE_URL_MALFORMAT:
      case CURLE_NOT_BUILT_IN:
      case CURLE_FTP_WEIRD_SERVER_REPLY:
      case CURLE_FTP_ACCEPT_FAILED:
      case CURLE_FTP_WEIRD_PASS_REPLY:
      case CURLE_FTP_WEIRD_PASV_REPLY:
      case CURLE_FTP_WEIRD_227_FORMAT:
      case CURLE_FTP_COULDNT_SET_TYPE:
      case CURLE_PARTIAL_FILE:
      case CURLE_FTP_COULDNT_RETR_FILE:
      case CURLE_QUOTE_ERROR:
      case CURLE_HTTP_RETURNED_ERROR:
      case CURLE_FTP_PORT_FAILED:
      case CURLE_FTP_COULDNT_USE_REST:
      case CURLE_RANGE_ERROR:
      case CURLE_HTTP_POST_ERROR:
      case CURLE_BAD_DOWNLOAD_RESUME:
      case CURLE_LDAP_SEARCH_FAILED:
      case CURLE_TOO_MANY_REDIRECTS:
      case CURLE_TELNET_OPTION_SYNTAX:
      case CURLE_GOT_NOTHING:
      case CURLE_BAD_CONTENT_ENCODING:
      case CURLE_LDAP_INVALID_URL:
      case CURLE_FILESIZE_EXCEEDED:
      case CURLE_SEND_FAIL_REWIND:
      case CURLE_TFTP_ILLEGAL:
      case CURLE_TFTP_UNKNOWNID:
      case CURLE_SSL_SHUTDOWN_FAILED:
      case CURLE_FTP_PRET_FAILED:
      case CURLE_RTSP_CSEQ_ERROR:
      case CURLE_RTSP_SESSION_ERROR:
      case CURLE_FTP_BAD_FILE_LIST:
#if LIBCURL_VERSION_NUM >= 0x073100
      case CURLE_HTTP2_STREAM:
#endif /* version 7.49.0 and up */
        return LIST_ERROR_PROTOCOL;

      case CURLE_REMOTE_ACCESS_DENIED:
      case CURLE_UPLOAD_FAILED:
      case CURLE_SSL_CONNECT_ERROR:
      case CURLE_PEER_FAILED_VERIFICATION:
      case CURLE_SSL_CERTPROBLEM:
      case CURLE_SSL_CIPHER:
#if LIBCURL_VERSION_NUM < 0x073E00
      case CURLE_SSL_CACERT:
#endif /* removed in version 7.62.0 */
      case CURLE_USE_SSL_FAILED:
      case CURLE_LOGIN_DENIED:
      case CURLE_TFTP_PERM:
      case CURLE_TFTP_NOSUCHUSER:
      case CURLE_SSL_ISSUER_ERROR:
#if LIBCURL_VERSION_NUM >= 0x072700
      case CURLE_SSL_PINNEDPUBKEYNOTMATCH:
#endif /* version 7.39.0 and up */
#if LIBCURL_VERSION_NUM >= 0x072900
      case CURLE_SSL_INVALIDCERTSTATUS:
#endif /* version 7.41.0 and up */
        return LIST_ERROR_AUTHENTICATION;

      case CURLE_FAILED_INIT:
      case CURLE_FTP_CANT_GET_HOST:
      case CURLE_OUT_OF_MEMORY:
      case CURLE_FUNCTION_NOT_FOUND:
      case CURLE_BAD_FUNCTION_ARGUMENT:
      case CURLE_UNKNOWN_OPTION:
      case CURLE_SSL_ENGINE_NOTFOUND:
      case CURLE_SSL_ENGINE_SETFAILED:
      case CURLE_SSL_ENGINE_INITFAILED:
      case CURLE_CONV_FAILED:
      case CURLE_CONV_REQD:
      case CURLE_AGAIN:
      case CURLE_CHUNK_FAILED:
      case CURLE_NO_CONNECTION_AVAILABLE:
      case CURLE_OBSOLETE16:
      case CURLE_OBSOLETE20:
      case CURLE_OBSOLETE24:
      case CURLE_OBSOLETE29:
      case CURLE_OBSOLETE32:
      case CURLE_OBSOLETE40:
      case CURLE_OBSOLETE44:
      case CURLE_OBSOLETE46:
      case CURLE_OBSOLETE50:
#if LIBCURL_VERSION_NUM >= 0x073E00
      case CURLE_OBSOLETE51:
#endif /* version 7.62.0 and up */
      case CURLE_OBSOLETE57:
#if LIBCURL_VERSION_NUM >= 0x073B00
      case CURLE_RECURSIVE_API_CALL:
#endif /* version 7.59.0 and up */
      case CURL_LAST:
        break;
    }

    return LIST_ERROR_INTERNAL;
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

    char error_buffer[CURL_ERROR_SIZE] = "[details unknown]";

    curl_easy_setopt(rx, CURLOPT_URL, item->url);
    curl_easy_setopt(rx, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(rx, CURLOPT_MAXREDIRS, 5L);
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
    enum DBusListsErrorCode error = map_curl_error_to_list_error(rx_result);

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
            msg_error(0, LOG_ERR, "Failed downloading file: %s (%s)",
                      error_buffer, curl_easy_strerror(rx_result));
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
