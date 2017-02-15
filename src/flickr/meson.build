#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

flickr_sources = [
    'flickr-oauth.c',
    'flickr-oauth.h',
    'gflickr.c',
    'gflickr.h',
    'grl-flickr.c',
    'grl-flickr.h',
]

configure_file(output: 'config.h',
    configuration: cdata)

shared_library('grlflickr',
    sources: flickr_sources,
    install: true,
    install_dir: pluginsdir,
    dependencies: must_deps + plugins[flickr_idx][REQ_DEPS] + plugins[flickr_idx][OPT_DEPS],
    c_args: [
        '-DG_LOG_DOMAIN="GrlFlickr"',
        '-DHAVE_CONFIG_H',
    ])
