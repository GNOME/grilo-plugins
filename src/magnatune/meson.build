#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

magnatune_sources = [
    'grl-magnatune.c',
    'grl-magnatune.h',
]

configure_file(output: 'config.h',
    configuration: cdata)

shared_library('grlmagnatune',
    sources: magnatune_sources,
    install: true,
    install_dir: pluginsdir,
    dependencies: must_deps + plugins[magnatune_idx][REQ_DEPS] + plugins[magnatune_idx][OPT_DEPS],
    c_args: [
        '-DG_LOG_DOMAIN="GrlMagnatune"',
        '-DHAVE_CONFIG_H',
    ])
