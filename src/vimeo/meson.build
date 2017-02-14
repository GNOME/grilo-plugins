#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

vimeo_resources = gnome.compile_resources('vimeoresources',
    'vimeo.gresource.xml',
    c_name: '_grl_vimeo')

vimeo_sources = [
    'grl-vimeo.c',
    'grl-vimeo.h',
    'gvimeo.c',
    'gvimeo.h',
]

configure_file(output: 'config.h',
    configuration: cdata)

shared_library('grlvimeo',
    sources: vimeo_sources + vimeo_resources,
    install: true,
    install_dir: pluginsdir,
    dependencies: must_deps + plugins[vimeo_idx][REQ_DEPS] + plugins[vimeo_idx][OPT_DEPS],
    c_args: [
        '-DG_LOG_DOMAIN="GrlVimeo"',
        '-DHAVE_CONFIG_H',
    ],
    link_args: [
        '-Wl,--no-undefined',
    ])
