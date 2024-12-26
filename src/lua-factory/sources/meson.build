#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

sources = [
    'grl-acoustid',
    'grl-guardianvideos',
    'grl-iptv',
    'grl-itunes-podcast',
    'grl-lastfm-cover',
    'grl-musicbrainz-coverart',
    'grl-opensubtitles',
    'grl-radiofrance',
    'grl-steam-store',
    'grl-theaudiodb-cover',
    'grl-thegamesdb',
    'grl-video-title-parsing',
]

resources = [
    'grl-guardianvideos',
    'grl-iptv',
    'grl-radiofrance',
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
