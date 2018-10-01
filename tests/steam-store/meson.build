#
# meson.build
#
# Author: Tony Crisci <tony@dubstepdish.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

source_tests = [
    'test_steam_store',
]

foreach t: source_tests
    exe = executable(t, t + '.c',
        install: false,
        dependencies: must_deps,
        c_args: [
            '-DSTEAM_STORE_DATA_PATH="@0@/data/"'.format(meson.current_source_dir()),
            '-DLUA_FACTORY_PLUGIN_PATH="@0@/src/lua-factory/"'.format(meson.build_root()),
            '-DLUA_SOURCES_PATH="@0@/src/lua-factory/sources/"'.format(meson.source_root()),
        ])
    test(t, exe)
endforeach
