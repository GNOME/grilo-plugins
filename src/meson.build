#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

foreach p: plugins
    name = p[NAME].underscorify()
    name_enabled = name + '_enabled'
    if get_variable(name_enabled)
        subdir(p[NAME])
    endif
endforeach
