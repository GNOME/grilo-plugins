#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

shoutcast_sources = [
    'grl-shoutcast.c',
    'grl-shoutcast.h',
]

configure_file(output: 'config.h',
    configuration: cdata)

shared_library('grlshoutcast',
    sources: shoutcast_sources,
    install: true,
    install_dir: pluginsdir,
    dependencies: must_deps + plugins[shoutcast_idx][REQ_DEPS] + plugins[shoutcast_idx][OPT_DEPS],
    c_args: [
        '-DG_LOG_DOMAIN="GrlShoutcast"',
        '-DHAVE_CONFIG_H',
    ])
