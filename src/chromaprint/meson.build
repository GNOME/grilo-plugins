#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

chromaprint_sources = [
    'grl-chromaprint.c',
    'grl-chromaprint.h',
]

configure_file(output: 'config.h',
    configuration: cdata)

shared_library('grlchromaprint',
    sources: chromaprint_sources,
    install: true,
    install_dir: pluginsdir,
    dependencies: must_deps + plugins[chromaprint_idx][REQ_DEPS] + plugins[chromaprint_idx][OPT_DEPS],
    c_args: [
        '-DG_LOG_DOMAIN="GrlChromaprint"',
        '-DHAVE_CONFIG_H',
    ])
