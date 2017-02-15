#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

tmdb_sources = [
    'grl-tmdb-request.c',
    'grl-tmdb-request.h',
    'grl-tmdb.c',
    'grl-tmdb.h',
]

configure_file(output: 'config.h',
    configuration: cdata)

shared_library('grltmdb',
    sources: tmdb_sources,
    install: true,
    install_dir: pluginsdir,
    dependencies: must_deps + plugins[tmdb_idx][REQ_DEPS] + plugins[tmdb_idx][OPT_DEPS],
    c_args: [
        '-DG_LOG_DOMAIN="GrlTmdb"',
        '-DHAVE_CONFIG_H',
    ])
