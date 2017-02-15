#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

lua_factory_resources = gnome.compile_resources('luafactoryresources',
    'grl-lua-factory.gresource.xml',
    c_name: '_grl_lua_factory')

lua_factory_sources = [
    'grl-lua-common.h',
    'grl-lua-factory.c',
    'grl-lua-factory.h',
    'grl-lua-library-operations.c',
    'grl-lua-library-operations.h',
    'grl-lua-library.c',
    'grl-lua-library.h',
]

lua_library_sources = [
    'lua-library/lua-json.c',
    'lua-library/lua-xml.c',
    'lua-library/lua-libraries.h',
]

gperf_gen = generator(gperf,
    output: '@BASENAME@.c',
    arguments: [
        '--struct-type',
        '-l',
        '-N', 'html_entity_hash',
        '-L', 'ANSI-C',
        '--output-file', '@OUTPUT@',
        '@INPUT@',
    ])

htmlentity_c = gperf_gen.process(
    'lua-library/htmlentity.gperf')

htmlentity_h = configure_file(input: 'lua-library/htmlentity.h',
    output: 'htmlentity.h',
    configuration: configuration_data())

lua_library_sources += [
    htmlentity_c,
    htmlentity_h,
]

configure_file(output: 'config.h',
    configuration: cdata)

luafactorydir = join_paths(get_option('datadir'), 'grilo-plugins', 'grl-lua-factory')

shared_library('grlluafactory',
    sources: lua_factory_sources + lua_factory_resources + lua_library_sources,
    install: true,
    install_dir: pluginsdir,
    dependencies: must_deps + plugins[lua_factory_idx][REQ_DEPS] + plugins[lua_factory_idx][OPT_DEPS],
    c_args: [
        '-DG_LOG_DOMAIN="GrlLuaFactory"',
        '-DHAVE_CONFIG_H',
        '-DLUA_FACTORY_SOURCE_LOCATION="@0@"'.format(luafactorydir),
    ])

subdir('sources')
