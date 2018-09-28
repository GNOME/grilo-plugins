#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

sources = [
    'grl-acoustid',
    'grl-appletrailers',
    'grl-euronews',
    'grl-guardianvideos',
    'grl-itunes-podcast',
    'grl-lastfm-cover',
    'grl-metrolyrics',
    'grl-musicbrainz',
    'grl-pocket',
    'grl-radio-browser',
    'grl-radiofrance',
    'grl-spotify-cover',
    'grl-theaudiodb-cover',
    'grl-thegamesdb',
    'grl-video-title-parsing',
]

resources = [
    'grl-euronews',
    'grl-guardianvideos',
    'grl-radio-browser',
    'grl-radiofrance',
    'grl-pocket',
    'grl-appletrailers',
    'grl-itunes-podcast',
]

foreach s: sources
    f = s + '.lua'
    install_data(f,
        install_dir: join_paths(datadir, luafactorydir))
endforeach

foreach r: resources
    input = r + '.gresource.xml'
    output = r + '.gresource'
    custom_target(output,
        output: output,
        input: input,
        install: true,
        install_dir: join_paths(datadir, luafactorydir),
        command: [glib_compile_resources,
            '--sourcedir', meson.current_source_dir(),
            '--target', '@OUTPUT@',
            '@INPUT@'
        ])
endforeach
