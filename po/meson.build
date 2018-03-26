#
# meson.build
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2016 Igalia S.L. All rights reserved.

i18n = import('i18n')

langs = [
    'as',
    'bg',
    'bs',
    'ca',
    'ca@valencia',
    'cs',
    'da',
    'de',
    'el',
    'en_GB',
    'eo',
    'es',
    'eu',
    'fr',
    'fur',
    'gl',
    'he',
    'hu',
    'id',
    'it',
    'ja',
    'ko',
    'lt',
    'lv',
    'ml',
    'nb',
    'oc',
    'pl',
    'pt',
    'pt_BR',
    'ru',
    'sk',
    'sl',
    'sr',
    'sr@latin',
    'sv',
    'tr',
    'uk',
    'zh_CN',
    'zh_HK',
    'zh_TW',
]

i18n.gettext(meson.project_name(),
    languages: langs, preset: 'glib')
