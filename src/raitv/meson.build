#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

raitv_resources = gnome.compile_resources('raitvresources',
    'raitv.gresource.xml',
    c_name: '_grl_raitv')

raitv_sources = [
    'grl-raitv.c',
    'grl-raitv.h',
]

configure_file(output: 'config.h',
    configuration: cdata)

shared_library('grlraitv',
    sources: raitv_sources + raitv_resources,
    install: true,
    install_dir: pluginsdir,
    dependencies: must_deps + plugins[raitv_idx][REQ_DEPS] + plugins[raitv_idx][OPT_DEPS],
    c_args: [
        '-DG_LOG_DOMAIN="GrlRaitv"',
        '-DHAVE_CONFIG_H',
    ])
