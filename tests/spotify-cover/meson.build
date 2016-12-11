#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

source_tests = [
    'test_spotify_cover',
]

foreach t: source_tests
    exe = executable(t, t + '.c',
        install: false,
        dependencies: must_deps,
        c_args: [
            '-DLUA_FACTORY_PLUGIN_PATH="@0@/src/lua-factory/"'.format(meson.build_root()),
            '-DLUA_SOURCES_PATH="@0@/src/lua-factory/sources/"'.format(meson.source_root()),
            '-DSPOTIFY_COVER_DATA_PATH="@0@/data/"'.format(meson.current_source_dir()),
        ])
    test(t, exe)
endforeach
