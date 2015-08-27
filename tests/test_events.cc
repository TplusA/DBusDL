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

#include <cppcutter.h>
#include <ios>
#include <iomanip>

#include "events.h"

static struct XferItem *mk_xferitem(const char *url, uint32_t ticks,
                                    uint32_t expected_item_id)
{
    struct XferItem *item = xferitem_allocate(url, ticks);

    cppcut_assert_not_null(item);
    cppcut_assert_equal(expected_item_id, item->item_id);
    cppcut_assert_equal(ticks, item->total_ticks);
    cppcut_assert_equal(url, item->url);

    std::ostringstream os;
    os << "/tmp/downloads/"
       << std::dec << std::right << std::setw(10) << std::setfill('0')
       << expected_item_id << ".dbusdl";
    cppcut_assert_equal(os.str().c_str(), item->destfile_path);

    return item;
}

namespace events_from_user_tests
{

static struct EventFromUser *event;

void cut_setup()
{
    event = nullptr;
    xferitem_init("/tmp/downloads", false);
    events_init();
}

void cut_teardown()
{
    events_from_user_free(event);
    events_deinit();
    xferitem_deinit();
}

void test_new_shutdown()
{
    event = events_from_user_new_shutdown();

    cppcut_assert_not_null(event);
    cppcut_assert_equal(EVENT_FROM_USER_SHUTDOWN, event->event_id);
    cppcut_assert_null(event->d.item);
}

void test_new_start_download()
{
    struct XferItem *item = mk_xferitem("http://foo.bar/file.bin", 100, 1);
    event = events_from_user_new_start_download(item);

    cppcut_assert_not_null(event);
    cppcut_assert_equal(EVENT_FROM_USER_START_DOWNLOAD, event->event_id);
    cppcut_assert_equal(item, event->d.item);

    xferitem_free(event->d.item);
    event->d.item = NULL;
}

void test_new_cancel()
{
    event = events_from_user_new_cancel(42);

    cppcut_assert_not_null(event);
    cppcut_assert_equal(EVENT_FROM_USER_CANCEL, event->event_id);
    cppcut_assert_equal(42U, event->d.item_id);
}

void test_send_one_event()
{
    event = events_from_user_new_cancel(23);
    cppcut_assert_not_null(event);

    events_from_user_send(event);
    auto received = events_from_user_receive(false);

    cppcut_assert_equal(event, received);
}

void test_send_some_events()
{
    struct XferItem *item = mk_xferitem("foo", 400, 1);
    struct EventFromUser *events[] =
    {
        events_from_user_new_start_download(item),
        events_from_user_new_cancel(80),
        events_from_user_new_shutdown(),
    };

    for(const auto &ev : events)
    {
        cppcut_assert_not_null(ev);
        events_from_user_send(ev);
    }

    for(const auto &ev : events)
    {
        auto received = events_from_user_receive(false);

        cppcut_assert_equal(ev, received);

        switch(ev->event_id)
        {
          case EVENT_FROM_USER_START_DOWNLOAD:
            /* receiver of this event is expected to take ownership of the
             * attached #XferItem */
            xferitem_free(ev->d.item);
            ev->d.item = NULL;
            break;

          case EVENT_FROM_USER_SHUTDOWN:
          case EVENT_FROM_USER_CANCEL:
            break;
        }

        /* we own the event now, so we must free it */
        events_from_user_free(received);
    }
}

}

namespace events_to_user_tests
{

static struct EventToUser *event;

void cut_setup()
{
    event = nullptr;
    xferitem_init("/tmp/downloads", false);
    events_init();
}

void cut_teardown()
{
    events_to_user_free(event, true);
    events_deinit();
    xferitem_deinit();
}

void test_new_report_progress()
{
    struct XferItem *item = mk_xferitem("foo", 1000, 1);
    event = events_to_user_new_report_progress(item, 456);

    cppcut_assert_not_null(event);
    cppcut_assert_equal(EVENT_TO_USER_REPORT_PROGRESS, event->event_id);
    cppcut_assert_equal(item, event->xi.const_item);
}

void test_new_done()
{
    struct XferItem *item = mk_xferitem("bar", 10, 1);
    event = events_to_user_new_done(item, LIST_ERROR_OK);

    cppcut_assert_not_null(event);
    cppcut_assert_equal(EVENT_TO_USER_DONE, event->event_id);
    cppcut_assert_equal(item, event->xi.item);
}

void test_send_one_event()
{
    struct XferItem *item = mk_xferitem("foobar", 50, 1);
    event = events_to_user_new_done(item, LIST_ERROR_OK);

    cppcut_assert_not_null(event);

    events_to_user_send(event);
    auto received = events_to_user_receive(false);

    cppcut_assert_equal(event, received);
}

void test_send_some_events()
{
    struct XferItem *item = mk_xferitem("barfoo", 100, 1);
    struct EventToUser *events[] =
    {
        events_to_user_new_report_progress(item, 0),
        events_to_user_new_report_progress(item, 50),
        events_to_user_new_report_progress(item, 100),
        events_to_user_new_done(item, LIST_ERROR_OK),
    };

    for(const auto &ev : events)
    {
        cppcut_assert_not_null(ev);
        events_to_user_send(ev);
    }

    for(const auto &ev : events)
    {
        auto received = events_to_user_receive(false);

        cppcut_assert_equal(ev, received);

        /* we own the event now, so we must free it */
        events_to_user_free(received, false);
    }
}

}
