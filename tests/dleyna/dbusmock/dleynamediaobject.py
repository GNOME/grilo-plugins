# -*- coding: utf-8 -*-
'''dLeyna Media Object mock template'''

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
from dbusmock import MOCK_IFACE, get_object
from items import find_item, find_root, CHANGE_TYPES

MAIN_IFACE = 'org.gnome.UPnP.MediaObject2'

def load(mock, parameters):
    mock.AddMethods(MAIN_IFACE, [
        ('GetMetaData', '', 's', ''),
      ])
    mock.AddProperties(MAIN_IFACE, dbus.Dictionary(parameters, signature='sv'))

@dbus.service.method(MAIN_IFACE,
                     in_signature='a{sv}as',
                     out_signature='')
def Update(self, to_add_update, to_delete):
    item = find_item(self.items, self.__dbus_object_path__)
    for prop in to_delete:
        del item[prop]
    for prop, val in to_add_update.items():
        item[prop] = val

    device = get_object(find_root(self.items, self.__dbus_object_path__)['Path'])
    device.queue_change ({
        'ChangeType': CHANGE_TYPES['Mod'],
        'Path': self.__dbus_object_path__
    })

@dbus.service.method(MAIN_IFACE,
                     in_signature='',
                     out_signature='')
def Delete(self):
    device = get_object(find_root(self.items, self.__dbus_object_path__)['Path'])
    device.queue_change ({
        'ChangeType': CHANGE_TYPES['Del'],
        'Path': self.__dbus_object_path__
    })

    item = find_item(self.items, self.__dbus_object_path__)
    self.items.remove(item)

    parent = find_item(self.items, item['Parent'])
    parent['ChildCount'] -= 1
