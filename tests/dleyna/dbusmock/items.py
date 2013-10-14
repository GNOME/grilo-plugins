# -*- coding: utf-8 -*-
'''dLeyna DMS fake content hierarchy'''

# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License as published by the Free
# Software Foundation; either version 3 of the License, or (at your option) any
# later version.  See http://www.gnu.org/copyleft/lgpl.html for the full text
# of the license.

__author__ = 'Emanuele Aina'
__email__ = 'emanuele.aina@collabora.com'
__copyright__ = 'Copyright © 2013 Intel Corp. All rights reserved.'
__license__ = 'LGPL 3+'

import dbus

def filter_properties(obj, prop_filter):
    return dict(i for i in obj.items() if i[0] in prop_filter)

def find_item(items, path):
    item = [i for i in items if i['Path'] == path]
    if item:
        return item[0]
    return None

def find_root(items, path):
    item = find_item(items, path)
    while item and item['Parent'] != path:
        path = item['Parent']
        item = find_item(items, path)
    return item

MEDIA_DEVICE_PROPERTIES = ['DeviceType', 'UDN', 'FriendlyName', 'IconURL', 'Manufacturer', 'ManufacturerUrl',
                           'ModelDescription', 'ModelName', 'ModelNumber', 'SerialNumber', 'PresentationURL',
                           'DLNACaps', 'SearchCaps', 'SortCaps', 'SortExtCaps', 'FeatureList', 'ServiceResetToken',
                           'SystemUpdateID' ]
MEDIA_OBJECT2_PROPERTIES = [ 'Parent', 'Type', 'TypeEx', 'Path', 'DisplayName' ]
MEDIA_CONTAINER2_PROPERTIES = [ 'ChildCount', 'ItemCount', 'ContainerCount', 'Searchable', 'Resources', 'URLs' ]

CHANGE_TYPES = {
    'Add': dbus.UInt32(1),
    'Mod': dbus.UInt32(2),
    'Del': dbus.UInt32(3),
    'Done': dbus.UInt32(4),
    'Container': dbus.UInt32(5) }

ITEMS = [
    { 'DisplayName': 'The Root',
      'SearchCaps': ['*'],
      'SystemUpdateID': dbus.UInt32(0),
      'Path':   '{root}',
      'Parent': '{root}',
      'Type':   'container',
      'TypeEx': 'container',
      'URLs': [ 'http://127.0.0.1:4242/root/DIDL_S.xml' ] },
    { 'DisplayName': 'Music',
      'Path':   '{root}/1',
      'Parent': '{root}',
      'Type':   'container',
      'TypeEx': 'container' },
    { 'DisplayName': 'A song',
      'Path':   '{root}/11',
      'Parent': '{root}/1',
      'Type':   'music',
      'TypeEx': 'music' },
    { 'DisplayName': 'Another song, with funny chars: < >\'"{1}?#%s❤;',
      'Path':   '{root}/12',
      'Parent': '{root}/1',
      'Type':   'music',
      'TypeEx': 'music' },
    { 'DisplayName': 'Video',
      'Path':   '{root}/2',
      'Parent': '{root}',
      'Type':   'container',
      'TypeEx': 'container' },
    { 'DisplayName': 'This is a movie clip',
      'Path':   '{root}/21',
      'Parent': '{root}/2',
      'Type':   'video',
      'TypeEx': 'video' },
    { 'DisplayName': 'Stuff',
      'Path':   '{root}/3',
      'Parent': '{root}',
      'Type':   'container',
      'TypeEx': 'container',
      'URLs': [ 'http://127.0.0.1:4242/stuff/DIDL_S.xml' ] },
    { 'DisplayName': 'A non-media file',
      'Path':   '{root}/31',
      'Parent': '{root}/3',
      'Type':   'item.unclassified',
      'TypeEx': 'item' },
    { 'DisplayName': 'An empty sub container',
      'Path':   '{root}/32',
      'Parent': '{root}/3',
      'Type':   'container',
      'TypeEx': 'container' },
    { 'DisplayName': 'A picture.jpg',
      'Path':   '{root}/33',
      'Parent': '{root}/3',
      'Type':   'image.photo',
      'TypeEx': 'image.photo',
      'URLs': [ 'http://127.0.0.1:4242/stuff/picture.jpg' ] },
    { 'DisplayName': 'Another picture.jpg',
      'Path':   '{root}/34',
      'Parent': '{root}/3',
      'Type':   'image.photo',
      'TypeEx': 'image.photo',
      'URLs': [ 'http://127.0.0.1:4242/stuff/picture2.jpg' ] },
    { 'DisplayName': 'Another sub container',
      'Path':   '{root}/35',
      'Parent': '{root}/3',
      'Type':   'container',
      'TypeEx': 'container' },
    { 'DisplayName': 'A nested file',
      'Path':   '{root}/351',
      'Parent': '{root}/35',
      'Type':   'item.unclassified',
      'TypeEx': 'item' },
]

# Populate initial ChildCounts
for item in ITEMS:
    if item['Type'].startswith('container'):
        item['ChildCount'] = dbus.UInt32(len([i for i in ITEMS if i != item
            and i['Parent'] == item['Path']]))
