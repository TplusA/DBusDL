#
# Copyright (C) 2015, 2018, 2019, 2020  T+A elektroakustik GmbH & Co. KG
#
# This file is part of D-Bus DL.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA  02110-1301, USA.
#

SUBDIRS = . dist tests

ACLOCAL_AMFLAGS = -I m4

DBUS_IFACES = $(top_srcdir)/dbus_interfaces

AM_CPPFLAGS = -I$(DBUS_IFACES)
AM_CPPFLAGS += $(DBUSDL_DEPENDENCIES_CFLAGS)

AM_CFLAGS = $(CWARNINGS)
AM_CFLAGS += $(DBUSDL_DEPENDENCIES_CFLAGS)

AM_CXXFLAGS = $(CXXWARNINGS)

bin_PROGRAMS = dbusdl

noinst_LTLIBRARIES = libfiletransfer_dbus.la libevents.la

dbusdl_SOURCES = \
    dbusdl.c \
    events.h xferitem.h xferthread.c xferthread.h \
    messages.h messages.c \
    backtrace.h backtrace.c \
    os.h os.c \
    dbus_interfaces/de_tahifi_lists_errors.h \
    dbus_iface.c dbus_iface.h dbus_handlers.c dbus_handlers.h

dbusdl_LDADD = $(noinst_LTLIBRARIES) $(DBUSDL_DEPENDENCIES_LIBS)

nodist_libfiletransfer_dbus_la_SOURCES = filetransfer_dbus.c filetransfer_dbus.h
libfiletransfer_dbus_la_CFLAGS = $(CRELAXEDWARNINGS)

libevents_la_SOURCES = events.c events.h xferitem.c xferitem.h

if WITH_MARKDOWN
html_DATA = README.html
endif

EXTRA_DIST = README.md

CLEANFILES = README.html

EXTRA_DIST += \
    dbus_interfaces/extract_documentation.py \
    dbus_interfaces/de_tahifi_filetransfer.xml

BUILT_SOURCES = \
    $(nodist_libfiletransfer_dbus_la_SOURCES) \
    de_tahifi_filetransfer.h \
    de_tahifi_filetransfer.md \
    filetransfer_dbus.stamp \
    versioninfo.cache \
    versioninfo.h

CLEANFILES += $(BUILT_SOURCES)

versioninfo.h: versioninfo.cache
	(cd $(top_srcdir) && $(AUTOREVISION) -t h -o $(abs_top_builddir)/versioninfo.cache >$(abs_top_builddir)/$@)

.md.html:
	$(MARKDOWN) $< >$@


de_tahifi_filetransfer.h: filetransfer_dbus.stamp
de_tahifi_filetransfer.md: filetransfer_dbus.stamp
filetransfer_dbus.c: filetransfer_dbus.stamp
filetransfer_dbus.h: filetransfer_dbus.stamp
filetransfer_dbus.stamp: $(DBUS_IFACES)/de_tahifi_filetransfer.xml
	$(GDBUS_CODEGEN) --generate-c-code=filetransfer_dbus --c-namespace tdbus --interface-prefix de.tahifi. $<
	$(DBUS_IFACES)/extract_documentation.py -i $< -o de_tahifi_filetransfer.md -H de_tahifi_filetransfer.h -c tdbus -s de.tahifi. -n "$(PACKAGE_NAME) (transferring files across the internet)"
	touch $@
