#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

source_tests = [
    'test_chromaprint_resolve',
]

source_common = [
    'test_chromaprint_utils.c',
    'test_chromaprint_utils.h',
]

foreach t: source_tests
    exe = executable(t, [t + '.c'] + source_common,
        install: false,
        dependencies: [ must_deps, gstreamer_dep ],
        c_args: [
            '-DCHROMAPRINT_PLUGIN_PATH="@0@/src/chromaprint/"'.format(meson.build_root()),
            '-DCHROMAPRINT_PLUGIN_TEST_DATA_PATH="@0@/data/"'.format(meson.current_source_dir()),
        ])
    test(t, exe)
endforeach
