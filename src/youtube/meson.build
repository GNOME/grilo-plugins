#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

youtube_resources = gnome.compile_resources('youtuberesources',
    'youtube.gresource.xml',
    c_name: '_grl_youtube')

youtube_sources = [
    'grl-youtube.c',
    'grl-youtube.h',
]

configure_file(output: 'config.h',
    configuration: cdata)

shared_library('grlyoutube',
    sources: youtube_sources + youtube_resources,
    install: true,
    install_dir: pluginsdir,
    dependencies: must_deps + plugins[youtube_idx][REQ_DEPS] + plugins[youtube_idx][OPT_DEPS],
    c_args: [
        '-DG_LOG_DOMAIN="GrlYoutube"',
        '-DHAVE_CONFIG_H',
    ])
