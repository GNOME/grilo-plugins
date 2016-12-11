#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

examples = [
    'example-tmdb'
]

foreach e: examples
    source = e + '.c'
    executable(e,
        source,
        install: false,
        dependencies: must_deps)
endforeach
