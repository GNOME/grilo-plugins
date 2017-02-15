#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

local_metadata_sources = [
    'grl-local-metadata.c',
    'grl-local-metadata.h',
]

configure_file(output: 'config.h',
    configuration: cdata)

shared_library('grllocalmetadata',
    sources: local_metadata_sources,
    install: true,
    install_dir: pluginsdir,
    dependencies: must_deps + plugins[local_metadata_idx][REQ_DEPS] + plugins[local_metadata_idx][OPT_DEPS],
    c_args: [
        '-DG_LOG_DOMAIN="GrlLocalMetadata"',
        '-DHAVE_CONFIG_H',
    ])
