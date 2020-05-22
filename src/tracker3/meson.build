#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

tracker_sources = [
    'grl-tracker-source-api.c',
    'grl-tracker-source-api.h',
    'grl-tracker-source-cache.c',
    'grl-tracker-source-cache.h',
    'grl-tracker-source-notif.c',
    'grl-tracker-source-notif.h',
    'grl-tracker-source-priv.h',
    'grl-tracker-source-statements.c',
    'grl-tracker-source-statements.h',
    'grl-tracker-source.c',
    'grl-tracker-source.h',
    'grl-tracker-utils.c',
    'grl-tracker-utils.h',
    'grl-tracker.c',
    'grl-tracker.h',
]

configure_file(output: 'config.h',
    configuration: cdata)

shared_library('grltracker3',
    sources: tracker_sources,
    install: true,
    install_dir: pluginsdir,
    dependencies: must_deps + plugins[tracker3_idx][REQ_DEPS] + plugins[tracker3_idx][OPT_DEPS],
    c_args: [
        '-DG_LOG_DOMAIN="GrlTracker3"',
        '-DHAVE_CONFIG_H',
    ])
