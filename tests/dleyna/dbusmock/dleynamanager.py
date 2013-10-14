# -*- coding: utf-8 -*-
'''dLeyna Server Manager mock template'''

# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License as published by the Free
# Software Foundation; either version 3 of the License, or (at your option) any
# later version.  See http://www.gnu.org/copyleft/lgpl.html for the full text
# of the license.

__author__ = 'Emanuele Aina'
__email__ = 'emanuele.aina@collabora.com'
__copyright__ = 'Copyright © 2013 Intel Corp. All rights reserved.'
__license__ = 'LGPL 3+'

import copy, dbus, uuid
from dbusmock import MOCK_IFACE, get_object
from items import ITEMS, filter_properties, MEDIA_DEVICE_PROPERTIES, MEDIA_OBJECT2_PROPERTIES, MEDIA_CONTAINER2_PROPERTIES

BUS_NAME = 'com.intel.dleyna-server'
MAIN_OBJ = '/com/intel/dLeynaServer'
MAIN_IFACE = 'com.intel.dLeynaServer.Manager'
SYSTEM_BUS = False

def load(mock, parameters):
    mock.AddMethods(MAIN_IFACE, [
        ('GetServers', '', 'ao', 'ret = self.servers'),
        ('GetVersion', '', 's', 'ret = "0.2.0"'),
        ('PreferLocalAddresses', 'b', '', ''),
        ('Release', '', '', ''),
        ('Rescan', '', '', ''),
        ('SetProtocolInfo', 's', '', ''),
      ])
    mock.AddProperties(MAIN_IFACE, dbus.Dictionary({
        'Foo': 'bar',
      }, signature='sv'))
    mock.next_server_id = 0
    mock.servers = []


@dbus.service.method(MOCK_IFACE,
                     in_signature='', out_signature='o')
def AddServer(self):
    root = '/com/intel/dLeynaServer/server/%d' % (self.next_server_id,)

    # Pre-process the items tree anchoring paths to the new root object
    items = copy.deepcopy(ITEMS)
    for item in items:
        item['Path'] = dbus.ObjectPath(item['Path'].replace('{root}', root))
        item['Parent'] = dbus.ObjectPath(item['Parent'].replace('{root}', root))

    for item in items:
        path = item['Path']
        self.AddObject(path, 'org.gnome.UPnP.MediaObject2', {}, [])
        obj = get_object(path)
        obj.items = items

        if path == root:
            item['FriendlyName'] = 'Mock Server <#{0}>'.format(self.next_server_id)
            item['UDN'] = str(uuid.uuid5(uuid.UUID('9123ef5c-f083-11e2-8000-000000000000'), str(self.next_server_id)))
            obj.AddTemplate("dleynamediadevice.py", filter_properties (item, MEDIA_DEVICE_PROPERTIES))
        obj.AddTemplate("dleynamediaobject.py", filter_properties(item, MEDIA_OBJECT2_PROPERTIES))
        obj.AddTemplate("dleynamediacontainer.py", filter_properties(item, MEDIA_CONTAINER2_PROPERTIES))

    self.servers.append(root)
    self.EmitSignal(MAIN_IFACE, 'FoundServer', 'o', [root])

    self.next_server_id += 1
    return path


@dbus.service.method(MOCK_IFACE,
                     in_signature='o', out_signature='')
def DropServer(self, path):
    self.servers.remove(path)
    self.RemoveObject(path);
    self.EmitSignal(MAIN_IFACE, 'LostServer', 'o', [path])
