#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

gravatar_sources = [
    'grl-gravatar.c',
    'grl-gravatar.h',
]

configure_file(output: 'config.h',
    configuration: cdata)

shared_library('grlgravatar',
    sources: gravatar_sources,
    install: true,
    install_dir: pluginsdir,
    dependencies: must_deps + plugins[gravatar_idx][REQ_DEPS] + plugins[gravatar_idx][OPT_DEPS],
    c_args: [
        '-DG_LOG_DOMAIN="GrlGravatar"',
        '-DHAVE_CONFIG_H',
    ])
