#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

optical_media_sources = [
    'grl-optical-media.c',
    'grl-optical-media.h',
]

configure_file(output: 'config.h',
    configuration: cdata)

shared_library('grlopticalmedia',
    sources: optical_media_sources,
    install: true,
    install_dir: pluginsdir,
    dependencies: must_deps + plugins[optical_media_idx][REQ_DEPS] + plugins[optical_media_idx][OPT_DEPS],
    c_args: [
        '-DG_LOG_DOMAIN="GrlOpticalMedia"',
        '-DHAVE_CONFIG_H',
    ])
