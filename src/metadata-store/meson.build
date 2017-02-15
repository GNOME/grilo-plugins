#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

metadata_store_sources = [
    'grl-metadata-store.c',
    'grl-metadata-store.h',
]

configure_file(output: 'config.h',
    configuration: cdata)

shared_library('grlmetadatastore',
    sources: metadata_store_sources,
    install: true,
    install_dir: pluginsdir,
    dependencies: must_deps + plugins[metadata_store_idx][REQ_DEPS] + plugins[metadata_store_idx][OPT_DEPS],
    c_args: [
        '-DG_LOG_DOMAIN="GrlMetadataStore"',
        '-DHAVE_CONFIG_H',
    ])
