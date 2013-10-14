# -*- coding: utf-8 -*-
'''dLeyna Media Device mock template'''

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
from items import find_item, find_root, filter_properties, MEDIA_OBJECT2_PROPERTIES, MEDIA_CONTAINER2_PROPERTIES, CHANGE_TYPES
from gi.repository import GLib

MAIN_IFACE = 'com.intel.dLeynaServer.MediaDevice'

def queue_change(self, change):
    self.changes_id += 1

    if self.changes is None:
        return

    # return only the parent if we want to emulate ContainerUpdateID-generated
    # change notifications
    if not self.changes_detailed:
        parent = find_item(self.items, change['Path'])
        change = filter_properties(parent, 'Path Type TypeEx'.split())
        change['ChangeType'] = CHANGE_TYPES['Container']
        change['UpdateID'] = self.changes_id

    self.changes += (change,)

def load(mock, parameters):
    mock.AddMethods(MAIN_IFACE, [
        ('Cancel', '', '', ''),
        ('CancelUpload', 'u', '', ''),
        ('CreatePlaylistInAnyContainer', 'ssssao', 'uo', ''),
        ('GetUploadIDs', '', 'au', ''),
        ('GetUploadStatus', 'u', 'stt', ''),
      ])
    mock.AddProperties(MAIN_IFACE, dbus.Dictionary(parameters, signature='sv'))
    mock.next_upload_id = 0
    mock.changes = None
    mock.changes_id = 0
    mock.changes_detailed = True
    # make queue_change() a real method of the mock object
    setattr(mock, 'queue_change', queue_change.__get__(mock, mock.__class__))

@dbus.service.method(MAIN_IFACE,
                     in_signature='aoas',
                     out_signature='aa{sv}')
def BrowseObjects(self, object_paths, props):
    results = []
    for object_path in object_paths:
        item = find_item(self.items, object_path)
        if item:
            result = filter_properties(item, props)
        else:
            result = {
                'Error': dbus.Dictionary({
                    'ID': dbus.Int32(42),
                    'Message': 'Object not found'}, 'sv') }
        results.append(result)
    return results

@dbus.service.method(MAIN_IFACE,
                     in_signature='ssas',
                     out_signature='o')
def CreateContainerInAnyContainer(self, display_name, container_type, child_types):
    upload_id = self.next_upload_id
    self.next_upload_id += 1

    path = '{0}/any{1:03}'.format(self.__dbus_object_path__, upload_id)
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
    obj.AddTemplate('dleynamediaobject.py', filter_properties(upload, MEDIA_OBJECT2_PROPERTIES))
    obj.AddTemplate('dleynamediacontainer.py', filter_properties(upload, MEDIA_CONTAINER2_PROPERTIES))

    self.queue_change ({
        'ChangeType': CHANGE_TYPES['Add'],
        'Path': path
    })

    return (path)

@dbus.service.method(MOCK_IFACE,
                     in_signature='', out_signature='')
def FlushChanges(self):
    """Emit the currently queued changes"""
    if not self.changes:
        return
    self.EmitSignal(MAIN_IFACE, 'Changed', 'aa{sv}', [self.changes])
    self.changes = []

@dbus.service.method(MOCK_IFACE,
                     in_signature='bb', out_signature='')
def QueueChanges(self, enabled, detailed):
    """Queue a change to be later emitted on FlushChanges()"""
    self.changes_detailed = detailed
    if not enabled:
        self.changes = None
    if enabled and self.changes is None:
        self.changes = []

@dbus.service.method(MAIN_IFACE,
                     in_signature='ss',
                     out_signature='uo')
def UploadToAnyContainer(self, display_name, filename):
    upload_id = self.next_upload_id
    self.next_upload_id += 1

    path = '{0}/any{1:03}'.format(self.__dbus_object_path__, upload_id)
    size = os.stat(filename).st_size
    upload = {
        'DisplayName': display_name,
        'Parent': self.__dbus_object_path__,
        'Path': path,
        'Type': 'item.unclassified',
        'UploadFilename': filename,
        'UploadId': upload_id,
        'UploadSize': size,
      }
    self.items.append(upload)

    self.AddObject(path, 'org.gnome.UPnP.MediaObject2', {}, [])
    obj = get_object(path)
    obj.items = self.items
    obj.AddTemplate('dleynamediaobject.py', filter_properties(upload, MEDIA_OBJECT2_PROPERTIES))

    device = get_object(find_root(self.items, self.__dbus_object_path__)['Path'])

    def upload_completed():
        device.EmitSignal(MAIN_IFACE, 'UploadUpdate', 'ustt', (upload_id, 'COMPLETED', size, size))
        self.queue_change ({
            'ChangeType': CHANGE_TYPES['Add'],
            'Path': path
        })
    GLib.idle_add(upload_completed)

    return (upload_id, path)
