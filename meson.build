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

project('DBusDL', ['cpp', 'c'],
    version: '2.0',
    default_options: ['warning_level=3', 'cpp_std=c++14', 'buildtype=plain'])

PACKAGE = 'dbusdl'

config_data = configuration_data()
config_data.set_quoted('PACKAGE', PACKAGE)
config_data.set_quoted('PACKAGE_NAME', meson.project_name())
config_data.set_quoted('PACKAGE_STRING', meson.project_name() + ' ' + meson.project_version())
config_data.set_quoted('PACKAGE_VERSION', meson.project_version())
config_data.set('abs_srcdir', meson.current_source_dir())
config_data.set('abs_builddir', meson.build_root())
config_data.set('bindir', get_option('prefix') / get_option('bindir'))

add_project_arguments('-DHAVE_CONFIG_H', language: ['cpp', 'c'])

relaxed_dbus_warnings = ['-Wno-bad-function-cast']

glib_deps = [
    dependency('gmodule-2.0'),
    dependency('gio-2.0'),
    dependency('gio-unix-2.0'),
    dependency('gthread-2.0'),
]

libcurl_deps = dependency('libcurl')

autorevision = find_program('autorevision')
markdown = find_program('markdown')
extract_docs = find_program('dbus_interfaces/extract_documentation.py')
doxygen = find_program('doxygen', required: false)

doxyfile = configure_file(input: 'Doxyfile.in', output: 'Doxyfile',
                          configuration: config_data)

version_info = custom_target(
    'version information file',
    input: configure_file(input: 'versioninfo.cache.in',
                          output: 'versioninfo.cache', copy: true),
    output: 'versioninfo.h',
    command: [
        autorevision, '-C', meson.current_source_dir(),
        '-t', 'h',
        '-o', 'versioninfo.cache'
    ],
    capture: true
)

config_h = declare_dependency(
    sources: configure_file(input: 'config.h.meson',
                            output: 'config.h',
                            configuration: config_data),
    include_directories: '.')

gnome = import('gnome')

dbus_iface_dir = 'dbus_interfaces'
dbus_iface_defs_includes = include_directories(dbus_iface_dir)

dbus_iface_data = [
    ['de_tahifi_filetransfer',   'de.tahifi.',                'tdbus',             'File Download'],
]

dbus_deps = []
dbus_docs = []

foreach d : dbus_iface_data
    dbus_deps += declare_dependency(
        link_with: static_library(
            d[0].split('_')[-1] + '_dbus',
            gnome.gdbus_codegen(d[0],
                                sources: dbus_iface_dir / d[0] + '.xml',
                                interface_prefix: d[1],
                                namespace: d[2]),
            dependencies: [glib_deps, config_h],
            c_args: relaxed_dbus_warnings),
        include_directories: dbus_iface_defs_includes
    )

    dbus_docs += custom_target(d[0] + '_docs',
        input: dbus_iface_dir / d[0] + '.xml',
        output: ['@BASENAME@-doc.md', '@BASENAME@-doc.h'],
        command: [
            extract_docs, '-i', '@INPUT@', '-o', '@OUTPUT0@', '-H', '@OUTPUT1@',
            '-c', d[2], '-s', d[1], '-n', d[3]
        ])
endforeach

events_lib = static_library('events',
    ['events.c', 'xferitem.c'],
    dependencies: [glib_deps, config_h],
    include_directories: dbus_iface_defs_includes,
)

subdir('tests')

custom_target('doxygen', output: 'doxygen.stamp',
              input: doxyfile, depends: dbus_docs, command: doxygen)

custom_target('documentation',
    output: 'README.html', input: 'README.md',
    command: [markdown, '-o', '@OUTPUT@', '@INPUT@'],
    install: true, install_dir: get_option('datadir') / 'doc' / PACKAGE
)

executable(
    'dbusdl',
    [
        'dbusdl.c', 'xferthread.c', 'messages.c', 'os.c', 'backtrace.c',
        'dbus_iface.c','dbus_handlers.c',
        version_info,
    ],
    dependencies: [dbus_deps, glib_deps, libcurl_deps, config_h],
    link_with: events_lib,
    install: true
)
