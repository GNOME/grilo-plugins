#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

bookmarks_sources = [
    'bookmarks-resource.c',
    'bookmarks-resource.h',
    'grl-bookmarks.c',
    'grl-bookmarks.h',
]

configure_file(output: 'config.h',
    configuration: cdata)

shared_library('grlbookmarks',
    sources: bookmarks_sources,
    install: true,
    install_dir: pluginsdir,
    dependencies: must_deps + plugins[bookmarks_idx][DEPS],
    c_args: [
        '-DG_LOG_DOMAIN="GrlBookmarks"',
        '-DHAVE_CONFIG_H',
    ],
    link_args: [
        '-Wl,--no-undefined',
    ])
