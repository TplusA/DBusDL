#
# Copyright (C) 2020  T+A elektroakustik GmbH & Co. KG
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

cutter_dep = dependency('cppcutter', required: false)
compiler = meson.get_compiler('cpp')

if not(compiler.has_header('cutter.h', dependencies: cutter_dep) and \
       compiler.has_header('cppcutter.h', dependencies: cutter_dep))
    subdir_done()
endif

cutter_wrap = find_program('cutter_wrap.sh')

cutter_wrap_args = [
    find_program('cutter').path(), find_program('xsltproc').path(),
    meson.current_build_dir(), meson.current_source_dir()
]

events_tests = shared_module('test_events',
    'test_events.cc',
    include_directories: ['..', dbus_iface_defs_includes],
    dependencies: cutter_dep,
    link_with: events_lib,
)
test('Events',
    cutter_wrap, args: [cutter_wrap_args, events_tests.full_path()],
    depends: events_tests,
)
