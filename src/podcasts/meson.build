#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

podcasts_sources = [
    'grl-podcasts.c',
    'grl-podcasts.h',
]

configure_file(output: 'config.h',
    configuration: cdata)

shared_library('grlpodcasts',
    sources: podcasts_sources,
    install: true,
    install_dir: pluginsdir,
    dependencies: must_deps + plugins[podcasts_idx][REQ_DEPS] + plugins[podcasts_idx][OPT_DEPS],
    c_args: [
        '-DG_LOG_DOMAIN="GrlPodcasts"',
        '-DHAVE_CONFIG_H',
    ])
