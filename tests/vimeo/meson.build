#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

source_tests = [
    'test_vimeo',
]

foreach t: source_tests
    exe = executable(t, t + '.c',
        install: false,
        dependencies: must_deps,
        c_args: [
            '-DVIMEO_DATA_PATH="@0@/data/"'.format(meson.current_source_dir()),
            '-DVIMEO_PLUGIN_PATH="@0@/src/vimeo/"'.format(meson.build_root()),
        ])
    test(t, exe)
endforeach
