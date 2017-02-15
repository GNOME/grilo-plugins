#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

filesystem_sources = [
    'grl-filesystem.c',
    'grl-filesystem.h',
]

configure_file(output: 'config.h',
    configuration: cdata)

shared_library('grlfilesystem',
    sources: filesystem_sources,
    install: true,
    install_dir: pluginsdir,
    dependencies: must_deps + plugins[filesystem_idx][REQ_DEPS] + plugins[filesystem_idx][OPT_DEPS],
    c_args: [
        '-DG_LOG_DOMAIN="GrlFilesystem"',
        '-DHAVE_CONFIG_H',
    ])
