# -*- coding: utf-8 -*-
'''dLeyna Media Container mock template'''

# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License as published by the Free
# Software Foundation; either version 3 of the License, or (at your option) any
# later version.  See http://www.gnu.org/copyleft/lgpl.html for the full text
# of the license.

__author__ = 'Emanuele Aina'
__email__ = 'emanuele.aina@collabora.com'
__copyright__ = 'Copyright © 2013 Intel Corp. All rights reserved.'
__license__ = 'LGPL 3+'

import dbus, os
from dbusmock import MOCK_IFACE, get_object
from items import ITEMS, filter_properties, find_item, find_root, MEDIA_OBJECT2_PROPERTIES, MEDIA_CONTAINER2_PROPERTIES, CHANGE_TYPES
from gi.repository import GLib
from dleynamediadevice import MAIN_IFACE as MEDIA_DEVICE_IFACE

MAIN_IFACE = 'org.gnome.UPnP.MediaContainer2'

def load(mock, parameters):
    mock.AddMethods(MAIN_IFACE, [
        ('CreatePlaylist', 'ssssao', '', ''),
        ('GetCompatibleResource', 'sas', 'a{sv}', ''),
        ('ListChildrenEx', 'uuass', 'aa{sv}', 'ret = []'),
        ('ListContainersEx', 'uuass', 'aa{sv}', 'ret = []'),
        ('ListContainers', 'uuas', 'aa{sv}', 'ret = []'),
        ('ListItemsEx', 'uuass', 'aa{sv}', 'ret = []'),
        ('ListItems', 'uuas', 'aa{sv}', 'ret = []'),
        ('SearchObjectsEx', 'suuass', 'aa{sv}u', 'ret = []'),
        ('SearchObjects', 'suuas', 'aa{sv}u', 'ret = []'),
      ])
    mock.AddProperties(MAIN_IFACE, dbus.Dictionary(parameters, signature='sv'))
    mock.next_upload_id = 0

@dbus.service.method(MAIN_IFACE,
                     in_signature='ssas',
                     out_signature='o')
def CreateContainer(self, display_name, container_type, child_types):
    upload_id = self.next_upload_id
    self.next_upload_id += 1

    path = "{0}/up{1:03}".format(self.__dbus_object_path__, upload_id)
    upload = {
        'ChildCount': dbus.UInt32(0),
        'DisplayName': display_name,
        'Parent': self.__dbus_object_path__,
        'Path': path,
        'Type': 'container',
        'TypeEx': container_type,
      }
    self.items.append(upload)

    self.AddObject(path, 'org.gnome.UPnP.MediaObject2', {}, [])
    obj = get_object(path)
    obj.items = self.items
    obj.AddTemplate("dleynamediaobject.py", filter_properties(upload, MEDIA_OBJECT2_PROPERTIES))
    obj.AddTemplate("dleynamediacontainer.py", filter_properties(upload, MEDIA_CONTAINER2_PROPERTIES))

    device = get_object(find_root(self.items, self.__dbus_object_path__)['Path'])
    device.queue_change ({
        'ChangeType': CHANGE_TYPES['Add'],
        'Path': path
    })

    return (path)

@dbus.service.method(MAIN_IFACE,
                     in_signature='uuas',
                     out_signature='aa{sv}')
def ListChildren(self, offset, count, prop_filter):
    children = [i for i in self.items if i['Parent'] == self.__dbus_object_path__]
    if count:
        children = children[offset:offset+count]
    else:
        children = children[offset:]
    return [filter_properties(i, prop_filter) for i in children]


@dbus.service.method(MAIN_IFACE,
                     in_signature='ss',
                     out_signature='uo')
def Upload(self, display_name, filename):
    upload_id = self.next_upload_id
    self.next_upload_id += 1

    path = "{0}/up{1:03}".format(self.__dbus_object_path__, upload_id)
    size = os.stat(filename).st_size
    upload = {
        'DisplayName': display_name,
        'Parent': self.__dbus_object_path__,
        'Path': path,
        'UploadFilename': filename,
        'UploadId': upload_id,
        'UploadSize': size,
      }
    self.items.append(upload)

    self.AddObject(path, 'org.gnome.UPnP.MediaObject2', {}, [])
    obj = get_object(path)
    obj.items = self.items
    obj.AddTemplate("dleynamediaobject.py", filter_properties(upload, MEDIA_OBJECT2_PROPERTIES))

    device = get_object(find_root(self.items, self.__dbus_object_path__)['Path'])

    def upload_completed():
        device.EmitSignal(MEDIA_DEVICE_IFACE, 'UploadUpdate', 'ustt', (upload_id, 'COMPLETED', size, size))
        device.queue_change ({
            'ChangeType': CHANGE_TYPES['Add'],
            'Path': path
        })
    GLib.idle_add(upload_completed)

    item = find_item(self.items, self.__dbus_object_path__)
    item['ChildCount'] += 1
    return (upload_id, path)
