#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

source_tests = [
    'test_tmdb_fast_resolution',
    'test_tmdb_fast_resolution_by_id',
    'test_tmdb_full_resolution',
    'test_tmdb_missing_configuration',
    'test_tmdb_preconditions',
]

source_common = [
    'test_tmdb_utils.c',
    'test_tmdb_utils.h',
]

foreach t: source_tests
    exe = executable(t, [t + '.c'] + source_common,
        install: false,
        dependencies: must_deps,
        c_args: [
            '-DGRILO_PLUGINS_TESTS_TMDB_DATA_PATH="@0@/data/"'.format(meson.current_source_dir()),
            '-DGRILO_PLUGINS_TESTS_TMDB_PLUGIN_PATH="@0@/src/tmdb/"'.format(meson.build_root()),
        ])
    test(t, exe)
endforeach
