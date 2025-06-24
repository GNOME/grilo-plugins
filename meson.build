#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

project('grilo-plugins', 'c',
        version: '0.3.18',
        default_options: [
            'buildtype=debugoptimized',
            'c_std=gnu99',
            'warning_level=1'
        ],
        license: 'LGPL-2.1-or-later',
        meson_version: '>= 0.47.0')

plugin_version = meson.project_version()
version_array = plugin_version.split('.')
grl_major = version_array[0]
grl_minor = version_array[1]

prefix = get_option('prefix')
localedir = join_paths(prefix, get_option('localedir'))
libdir = get_option('libdir')
datadir = get_option('datadir')
pluginsdir =join_paths(libdir, 'grilo-@0@.@1@'.format(grl_major, grl_minor))

glib2_required = '2.66'
grilo_required = '0.3.8'
glib2_required_info = '>= @0@'.format(glib2_required)
grilo_required_info = '>= @0@'.format(grilo_required)

glib_dep = dependency('glib-2.0', version: glib2_required_info, required: true)
gmodule_dep = dependency('gmodule-2.0', version: glib2_required_info, required: true)
gobject_dep = dependency('gobject-2.0', version: glib2_required_info, required: true)
grilo_dep = dependency('grilo-0.3', version: grilo_required_info, required: true,
                       fallback: ['grilo', 'libgrl_dep'])

pkgconf = configuration_data()
pkgconf.set('VERSION', plugin_version)

pkgconfig_file = 'grilo-plugins-@0@.@1@'.format(grl_major, grl_minor)

configure_file(input: pkgconfig_file + '.pc.in',
               output: pkgconfig_file + '.pc',
               configuration: pkgconf,
               install_dir: '@0@/pkgconfig'.format(get_option('libdir')))

avahi_client_dep = dependency('avahi-client', required: false)
avahi_glib_dep = dependency('avahi-glib', required: false)
avahi_gobject_dep = dependency('avahi-gobject', required: false)
gio_dep = dependency('gio-2.0', required: false)
gio_unix_dep = dependency('gio-unix-2.0', required: false)
goa_dep = dependency('goa-1.0', version: '>= 3.17.91', required: get_option('goa'))
gom_dep = dependency('gom-1.0', version: '>= 0.4', required: false)
grilo_net_dep = dependency('grilo-net-0.3', version: '>= 0.3.0', required: false,
                           fallback: ['grilo', 'libgrlnet_dep'])
if grilo_net_dep.found()
  soup_api_version = grilo_net_dep.get_variable(
      pkgconfig: 'soupapiversion',
      internal: 'soupapiversion',
      default_value: '2.4',
  )
else
  soup_api_version = '2.4'
endif
grilo_pls_dep = dependency('grilo-pls-0.3', version: '>= 0.3.0', required: false,
                           fallback: ['grilo', 'libgrlpls_dep'])
gstreamer_dep = dependency('gstreamer-1.0', required: false)
gthread_dep = dependency('gthread-2.0', required: false)
json_glib_dep = dependency('json-glib-1.0', required: false)
libarchive_dep = dependency('libarchive', required: false)
libmediaart_dep = dependency('libmediaart-2.0', required: false)
if soup_api_version == '2.4'
  libsoup_dep = dependency('libsoup-2.4', required: false)
  libsoup24_dep = libsoup_dep
  libsoup30_dep = dependency('unresolvable-dep', required: false)
  libgdata_dep = dependency('libgdata', version: '>= 0.17.0', required: false)
else
  libsoup_dep = dependency('libsoup-3.0', required: false)
  libsoup30_dep = libsoup_dep
  libsoup24_dep = dependency('unresolvable-dep', required: false)
  libgdata_dep = dependency('unresolvable-dep', required: false)
endif

libdmapsharing4_dep = dependency('libdmapsharing-4.0', version: '>= 3.9.11', required: false)
if soup_api_version == '2.4'
  libdmapsharing_dep = dependency('libdmapsharing-3.0', version: '>= 2.9.12', required: false)
else
  libdmapsharing_dep = libdmapsharing4_dep
endif

librest_dep = dependency('rest-1.0', required: false)
libxml_dep = dependency('libxml-2.0', required: false)
oauth_dep = dependency('oauth', required: false)
sqlite3_dep = dependency('sqlite3', required: false)
totem_plparser_dep = dependency('totem-plparser', version: '>= 3.4.1', required: false)
totem_plparser_mini_dep = dependency('totem-plparser-mini', version: '>= 3.4.1', required: false)
tracker_sparql_dep = dependency('tracker-sparql-2.0', version: '>= 2.3.0', required: false)
tracker3_dep = dependency('tracker-sparql-3.0', required: false)
tracker3_testutils_dep = dependency('tracker-testutils-3.0', required: false)

lua_pkg = [
  'lua',
  'lua5.4',
  'lua-5.4',
  'lua5.3',
  'lua-5.3',
]
lua_dep = disabler()
lua_version = ''
foreach p: lua_pkg
    if not lua_dep.found()
        lua_dep = dependency(p, version: '>= 5.3.0', required: false)
        if lua_dep.found()
            lua_version = p
        endif
    endif
endforeach

gperf = find_program('gperf', required: false)
glib_compile_resources = find_program('glib-compile-resources', required: false)

must_deps = [glib_dep, gmodule_dep, gobject_dep, grilo_dep]

cdata = configuration_data()
cdata.set('GRL_MAJOR', grl_major)
cdata.set('GRL_MINOR', grl_minor)
cdata.set_quoted('GETTEXT_PACKAGE', meson.project_name())
cdata.set_quoted('LOCALEDIR', localedir)
cdata.set_quoted('VERSION', plugin_version)

# Defines for optional dependencies
cdata.set('GOA_ENABLED', goa_dep.found())
cdata.set('HAVE_TOTEM_PLPARSER_MINI', totem_plparser_mini_dep.found())

NAME = 0
REQ_DEPS = 1
OPT_DEPS = 2

plugins = [ # NAME, REQ_DEPS, OPT_DEPS
    ['bookmarks', [gio_dep, libxml_dep, gom_dep], []],
    ['chromaprint', [gstreamer_dep], []],
    ['dleyna', [gio_dep, gio_unix_dep], []],
    ['dmap', [libdmapsharing_dep, libxml_dep], []],
    ['filesystem', [grilo_pls_dep], []],
    ['flickr', [grilo_net_dep, libxml_dep, oauth_dep], []],
    ['freebox', [grilo_pls_dep, avahi_client_dep, avahi_glib_dep, avahi_gobject_dep], []],
    ['gravatar', [], []],
    ['local-metadata', [gio_dep, libmediaart_dep], []],
    ['lua-factory', [lua_dep, libarchive_dep, grilo_net_dep, json_glib_dep, libxml_dep, librest_dep], [goa_dep, totem_plparser_mini_dep]],
    ['magnatune', [sqlite3_dep, grilo_net_dep], []],
    ['metadata-store', [sqlite3_dep], []],
    ['optical-media', [totem_plparser_dep], []],
    ['podcasts', [grilo_net_dep, libxml_dep, sqlite3_dep, totem_plparser_dep], []],
    ['shoutcast', [grilo_net_dep, libxml_dep], []],
    ['thetvdb', [grilo_net_dep, libxml_dep, libarchive_dep, gom_dep], []],
    ['tmdb', [json_glib_dep, grilo_net_dep], []],
    ['tracker', [tracker_sparql_dep], []],
    ['tracker3', [tracker3_dep], []],
    ['youtube', [grilo_net_dep, libxml_dep, libgdata_dep, totem_plparser_dep], []],
]

idx = 0
foreach p: plugins
    varname = p[NAME].underscorify()
    varname_enabled = varname + '_enabled'
    varname_idx = varname + '_idx'

    current_enabled = get_option('enable-' + p[NAME]) == 'no' ? false : true
    current_deps = p[REQ_DEPS]
    if current_enabled
        current_auto = get_option('enable-'+ p[NAME]) == 'auto'
        foreach d: current_deps
            if current_auto
                current_enabled = current_enabled and d.found()
            elif not d.found()
                error('Missing dependencies for @0@ plugin'.format(p[NAME]))
            endif
        endforeach
    endif
    set_variable(varname_enabled, current_enabled)
    set_variable(varname_idx, idx)
    if p[NAME] != 'dmap'
        cdata.set_quoted(varname.to_upper() + '_PLUGIN_ID', 'grl-' + p[NAME])
    else
        cdata.set_quoted('DPAP_PLUGIN_ID', 'grl-dpap')
        cdata.set_quoted('DAAP_PLUGIN_ID', 'grl-daap')
    endif
    if p[NAME] == 'dleyna'
      cdata.set_quoted('DLEYNA_DBUS_NAME', get_option('dleyna-service-name'))
    endif
    idx = idx + 1
endforeach

# Special case: Lua Factory requires gperf program
if lua_factory_enabled
    if get_option('enable-lua-factory') == 'auto'
        lua_factory_enabled = lua_factory_enabled and gperf.found() and glib_compile_resources.found()
    elif not gperf.found() or not glib_compile_resources.found()
        error('Missing dependencies for lua-factory plugin')
    endif
endif

# Special case: tracker and tracker3 modules are mutually exclusive
if tracker3_enabled and tracker_enabled
    error('tracker3 and tracker modules cannot be enabled at the same time')
endif

message('Plugins:')
foreach p: plugins
    name = p[NAME].underscorify() + '_enabled'
    message('   @0@ : @1@'.format(p[NAME], get_variable(name).to_string('yes', 'no')))
endforeach

gnome = import('gnome')

rootdir = include_directories('.')

subdir('src')
if get_option('help') != 'no'
  subdir('help')
endif
subdir('po')
subdir('tests')

# This is a dummy dependency for use when grilo-plugins is built as a Meson
# subproject.
grilo_plugins_dep = declare_dependency()
