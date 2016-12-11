#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

resources = gnome.compile_resources('luafactorytestsresources',
    'test_lua_factory_tests.gresource.xml',
    c_name: '_grl_lua_factory_tests')

source_tests = [
    'test_lua_factory_grl_media',
    'test_lua_factory_source_errors',
    'test_lua_factory_xml_parser',
]

foreach t: source_tests
    exe = executable(t, [t + '.c'] + resources,
        install: false,
        dependencies: must_deps + [json_glib_dep],
        c_args: [
            '-DLUA_FACTORY_DATA_PATH="@0@/data/"'.format(meson.current_source_dir()),
            '-DLUA_FACTORY_PLUGIN_PATH="@0@/src/lua-factory/"'.format(meson.build_root()),
            '-DLUA_FACTORY_PLUGIN_TEST_DATA_PATH="@0@/sources/"'.format(meson.current_build_dir()),
            '-DLUA_FACTORY_SOURCES_PATH="@0@/src/lua-factory/sources/"'.format(meson.source_root()),
        ])
    test(t, exe)
endforeach

subdir('sources')
