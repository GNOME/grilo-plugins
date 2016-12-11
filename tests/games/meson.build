#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

source_tests = [
    'test_games',
]

foreach t: source_tests
    exe = executable(t, t + '.c',
        install: false,
        dependencies: must_deps,
        c_args: [
            '-DGAMES_DATA_PATH="@0@/data/"'.format(meson.current_source_dir()),
            '-DLUA_FACTORY_PLUGIN_PATH="@0@/src/lua-factory/"'.format(meson.build_root()),
            '-DLUA_SOURCES_PATH="@0@/src/lua-factory/sources/"'.format(meson.source_root()),
        ])
    test(t, exe)
endforeach
