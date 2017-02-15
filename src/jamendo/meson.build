#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

jamendo_sources = [
    'grl-jamendo.c',
    'grl-jamendo.h',
]

configure_file(output: 'config.h',
    configuration: cdata)

shared_library('grljamendo',
    sources: jamendo_sources,
    install: true,
    install_dir: pluginsdir,
    dependencies: must_deps + plugins[jamendo_idx][REQ_DEPS] + plugins[jamendo_idx][OPT_DEPS],
    c_args: [
        '-DG_LOG_DOMAIN="GrlJamendo"',
        '-DHAVE_CONFIG_H',
    ])
