#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

opensubtitles_sources = [
    'grl-opensubtitles.c',
    'grl-opensubtitles.h',
]

configure_file(output: 'config.h',
    configuration: cdata)

shared_library('grlopensubtitles',
    sources: opensubtitles_sources,
    install: true,
    install_dir: pluginsdir,
    dependencies: must_deps + plugins[opensubtitles_idx][REQ_DEPS] + plugins[opensubtitles_idx][OPT_DEPS],
    c_args: [
        '-DG_LOG_DOMAIN="GrlOpenSubtitles"',
        '-DHAVE_CONFIG_H',
    ])
