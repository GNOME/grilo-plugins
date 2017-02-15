#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

freebox_resources = gnome.compile_resources('freeboxresources',
    'freebox.gresource.xml',
    c_name: '_grl_freebox')

freebox_sources = [
    'freebox-monitor.c',
    'freebox-monitor.h',
    'grl-freebox.c',
    'grl-freebox.h',
]

configure_file(output: 'config.h',
    configuration: cdata)

shared_library('grlfreebox',
    sources: freebox_sources + freebox_resources,
    install: true,
    install_dir: pluginsdir,
    dependencies: must_deps + plugins[freebox_idx][REQ_DEPS] + plugins[freebox_idx][OPT_DEPS],
    c_args: [
        '-DG_LOG_DOMAIN="GrlFreebox"',
        '-DHAVE_CONFIG_H',
    ])
