#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

test_plugins = [
    'chromaprint',
    # 'dleyna',
    'lua-factory',
    # 'thetvdb', See: https://gitlab.gnome.org/GNOME/grilo-plugins/issues/64
    'tmdb',
    'vimeo',
]

test_lua_sources = [
    'games',
    'spotify-cover',
    'steam-store',
]

# Plugins
foreach p: test_plugins
    name = p.underscorify()
    name_enabled = name + '_enabled'
    if get_variable(name_enabled)
        subdir(p)
    endif
endforeach

# Lua sources
if lua_factory_enabled
    foreach s: test_lua_sources
        subdir(s)
    endforeach
endif

# Special cases
if local_metadata_enabled and lua_factory_enabled
    subdir('local-metadata')
endif
