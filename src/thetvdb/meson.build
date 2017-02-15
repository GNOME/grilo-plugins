#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

thetvdb_sources = [
    'grl-thetvdb.c',
    'grl-thetvdb.h',
    'thetvdb-resources-episodes.c',
    'thetvdb-resources-fuzzy-names.c',
    'thetvdb-resources-series.c',
    'thetvdb-resources.h',
]

configure_file(output: 'config.h',
    configuration: cdata)

shared_library('grlthetvdb',
    sources: thetvdb_sources,
    install: true,
    install_dir: pluginsdir,
    dependencies: must_deps + plugins[thetvdb_idx][REQ_DEPS] + plugins[thetvdb_idx][OPT_DEPS],
    c_args: [
        '-DG_LOG_DOMAIN="GrlTheTVDB"',
        '-DHAVE_CONFIG_H',
    ])
