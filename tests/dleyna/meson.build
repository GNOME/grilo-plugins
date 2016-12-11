#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

mock_conf = configuration_data()
mock_conf.set('abs_top_srcdir', meson.source_root())

configure_file(input: 'dbusmock/dleyna-server-mock.service.in',
    output: 'dleyna-server-mock.service',
    configuration: mock_conf)

source_tests = [
    'test_dleyna',
]

source_common = [
    'test_dleyna_utils.c',
    'test_dleyna_utils.h',
]

foreach t: source_tests
    exe = executable(t, [t + '.c'] + source_common,
        install: false,
        dependencies: must_deps,
        c_args: [
            '-DGRILO_PLUGINS_TESTS_DLEYNA_DATA_PATH="@0@/data/"'.format(meson.current_source_dir()),
            '-DGRILO_PLUGINS_TESTS_DLEYNA_PLUGIN_PATH="@0@/src/dleyna/"'.format(meson.build_root()),
            '-DGRILO_PLUGINS_TESTS_DLEYNA_SERVICES_PATH="@0@/"'.format(meson.current_build_dir()),
        ])
    test(t, exe)
endforeach
