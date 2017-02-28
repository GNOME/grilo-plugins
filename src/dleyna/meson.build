#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

dleyna_proxy_mediaserver2 = gnome.gdbus_codegen('grl-dleyna-proxy-mediaserver2',
    'org.gnome.UPnP.MediaServer2.xml',
    namespace: 'GrlDleyna',
    interface_prefix: 'org.gnome.UPnP.')

dleyna_proxy_manager = gnome.gdbus_codegen('grl-dleyna-proxy-manager',
    'com.intel.dLeynaServer.Manager.xml',
    namespace: 'GrlDleyna',
    interface_prefix: 'com.intel.dLeynaServer.')

dleyna_proxy_mediadevice = gnome.gdbus_codegen('grl-dleyna-proxy-mediadevice',
    'com.intel.dLeynaServer.MediaDevice.xml',
    namespace: 'GrlDleyna',
    interface_prefix: 'com.intel.dLeynaServer.')

dleyna_sources = [
    'grl-dleyna-server.c',
    'grl-dleyna-server.h',
    'grl-dleyna-servers-manager.c',
    'grl-dleyna-servers-manager.h',
    'grl-dleyna-source.c',
    'grl-dleyna-source.h',
    'grl-dleyna-utils.c',
    'grl-dleyna-utils.h',
    'grl-dleyna.c',
]

configure_file(output: 'config.h',
    configuration: cdata)

shared_library('grldleyna',
    sources: dleyna_sources + [dleyna_proxy_mediaserver2,  dleyna_proxy_manager, dleyna_proxy_mediadevice],
    install: true,
    install_dir: pluginsdir,
    dependencies: must_deps + plugins[dleyna_idx][REQ_DEPS] + plugins[dleyna_idx][OPT_DEPS],
    include_directories: rootdir,
    c_args: [
        '-DG_LOG_DOMAIN="GrlDleyna"',
        '-DHAVE_CONFIG_H',
    ])
