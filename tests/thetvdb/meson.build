#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

source_tests = [
    'test_thetvdb_resolve_episodes',
    'test_thetvdb_resolve_shows',
    'test_thetvdb_resolve_stress',
]

source_common = [
    'test_thetvdb_utils.c',
    'test_thetvdb_utils.h',
]

foreach t: source_tests
    exe = executable(t, [t + '.c'] + source_common,
        install: false,
        dependencies: must_deps,
        c_args: [
            '-DTHETVDB_PLUGIN_PATH="@0@/src/thetvdb/"'.format(meson.build_root()),
            '-DTHETVDB_PLUGIN_TEST_DATA_PATH="@0@/data/"'.format(meson.current_source_dir()),
        ])
    test(t, exe)
endforeach
