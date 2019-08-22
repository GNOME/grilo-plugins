#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

acoustid_resources = gnome.compile_resources('acoustidresources',
    'test_lua_acoustid.gresource.xml',
    c_name: '_grl_lua_acoustid')

source_tests = [
    'test_lua_theaudiodb'
]

if chromaprint_enabled
    source_tests += [
        'test_lua_acoustid',
    ]
endif

source_common = [
    'test_lua_factory_utils.c',
    'test_lua_factory_utils.h',
]

resources = acoustid_resources

foreach t: source_tests
    exe = executable(t, [t + '.c'] + source_common + resources,
        install: false,
        dependencies: must_deps,
        c_args: [
            '-DCHROMAPRINT_PLUGIN_PATH="@0@/src/chromaprint/"'.format(meson.build_root()),
            '-DLUA_FACTORY_PLUGIN_PATH="@0@/src/lua-factory/"'.format(meson.build_root()),
            '-DLUA_FACTORY_PLUGIN_TEST_DATA_PATH="@0@/"'.format(meson.current_build_dir()),
            '-DLUA_FACTORY_SOURCES_DATA_PATH="@0@/data/"'.format(meson.current_source_dir()),
            '-DLUA_FACTORY_SOURCES_PATH="@0@/src/lua-factory/sources/"'.format(meson.source_root()),
        ])
    test(t, exe)
endforeach
