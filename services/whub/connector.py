#      connector.py
#      
#      Copyright (C) 2013 Yi-Wei Ci <ciyiwei@hotmail.com>
#      
#      This program is free software; you can redistribute it and/or modify
#      it under the terms of the GNU General Public License as published by
#      the Free Software Foundation; either version 2 of the License, or
#      (at your option) any later version.
#      
#      This program is distributed in the hope that it will be useful,
#      but WITHOUT ANY WARRANTY; without even the implied warranty of
#      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#      GNU General Public License for more details.
#      
#      You should have received a copy of the GNU General Public License
#      along with this program; if not, write to the Free Software
#      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#      MA 02110-1301, USA.

import struct
import socket
import device
from dev.network import Network
from dev.storage import Storage
from dev.monitor import Monitor
from dev.container import Container
from host import WHubHost

import sys
sys.path.append('../../lib')
from default import DEFAULT_HOST, NET_ADAPTER
from privacy import PUBLIC, PRIVATE
from log import log, log_err
from wview import WView
from wctrl import WCtrl
from whub import WHub
import net

WHUB_CONN_TIMEOUT = 30 # Seconds

class WHubConnector(object):
    def _addr2name(self, addr):
        return '%x' % struct.unpack("I", socket.inet_aton(addr))[0]
    
    def _get_real_name(self, guest):
        return self._addr2name(guest)
    
    def is_active(self, guest, name, privacy, host):
        real = self._get_real_name(guest)
        return device.STATE_ACTIVE == Network().state(real, privacy, host)
    
    def _add_dev(self, host, dev, name):
        try:
            if DEFAULT_HOST == host or net.chkiface(NET_ADAPTER) == host:
                res = dev.register(name)
                if res:
                    res = dev.mount(name)
                    if not res:
                        dev.unregister(name)
            else:
                hub = WHub(host, dev)
                res = hub.register(name)
                if res:
                    res = hub.mount(name)
                    if not res:
                        hub.unregister(name)
            return res
        except:
            return False
    
    def _remove_dev(self, host, dev, name):
        try:
            if DEFAULT_HOST == host or net.chkiface(NET_ADAPTER) == host:
                res = dev.unmount(name)
                if res:
                    res = dev.unregister(name)
            else:
                hub = WHub(host, dev)
                res = hub.unmount(name)
                if res:
                    res = hub.unregister(name)
            return res
        except:
            return False
    
    @property
    def name(self):
        return self.__class__.__name__
    
    def enter(self, addr, guest, name, secret, password='', privacy=PRIVATE):
        try:
            return WHub(addr).enter(guest, name, secret, password, privacy)
        except:
            log_err('%s: failed to enter' % self.name)
    
    def exit(self, addr, guest, name, secret, password='', privacy=PRIVATE, host=DEFAULT_HOST):
        try:
            return WHub(addr).exit(guest, name, secret, password, privacy, host)
        except:
            log_err('%s: failed to exit' % self.name)
          
    def _connect(self, guest, name, secret, privacy, host):
        try:
            dev = Network()
            if not dev.register(name, secret, privacy, host):
                return
            if PRIVATE == privacy or net.chkiface(NET_ADAPTER) == host:
                if not dev.mount(guest, name, secret):
                    dev.unregister(name, secret, privacy, host)
                    return
            else:
                if not WHub(host, dev).mount(guest, name, secret):
                    dev.unregister(name, secret, privacy, host)
                    return
            return True
        except:
            log_err('%s: failed to connect' % self.name)
    
    def connect(self, guest, name, secret, privacy):
        host = WHubHost().acquire(guest, privacy)
        if not host:
            log_err('%s: failed to acquire host, user=%s' % (self.name, name))
            return
        real = self._get_real_name(guest)
        if not self._connect(guest, real, secret, privacy, host):
            WHubHost().release(host)
            return
        res = self._add_dev(host, Container(), real)
        if not res:
            self._disconnect(real, secret, privacy, host)
            WHubHost().release(host)
            return
        res = self._add_dev(host, Monitor(), real)
        if not res:
            self._remove_dev(host, Container(), real)
            self._disconnect(real, secret, privacy, host)
            WHubHost().release(host)
            return
        res = self._add_dev(host, Storage(), real)
        if not res:
            self._remove_dev(host, Monitor(), real)
            self._remove_dev(host, Container(), real)
            self._disconnect(real, secret, privacy, host)
            WHubHost().release(host)
            return
        return host
    
    def _disconnect(self, name, secret, privacy, host):
        try:
            dev = Network()
            if not dev.unregister(name, secret, privacy, host):
                return
            if PRIVATE == privacy or net.chkiface(NET_ADAPTER) == host:
                dev.unmount(name)
            else:
                WHub(host, dev).unmount(name)
            return True
        except:
            log_err('%s: failed to disconnect', self.name)
    
    def disconnect(self, guest, name, secret, privacy, host):
        real = self._get_real_name(guest)
        self._remove_dev(host, Container(), real)
        self._remove_dev(host, Monitor(), real)
        self._remove_dev(host, Storage(), real)
        if not self._disconnect(real, secret, privacy, host):
            return
        WHubHost().release(host)
        return True

if __name__ == '__main__':
    records = {}
    ctrl = WCtrl()
    view = WView()
    privacy = PRIVATE
    connector = WHubConnector()
    guest = net.chkiface(NET_ADAPTER)
    
    while True:
        res = view.connector.enter()
        if None == res:
            exit(0)
        name = res['name']
        addr = res['addr']
        if res['enter']:
            if not name or not addr:
                continue
            secret = ctrl.secret.create()
            host = connector.enter(addr, guest, name, secret, privacy=privacy)
            if not host:
                ctrl.secret.delete(secret)
                continue
            records.update({addr:{'host':host, 'secret':secret}})
        else:
            if not name or not addr or not records.has_key(addr):
                exit(0)
            host = records[addr]['host']
            secret = records[addr]['secret']
            res = connector.exit(addr, guest, name, secret, privacy=privacy, host=host)
            if res:
                ctrl.secret.delete(secret)
                del records[addr]
                continue
        res = False
        while not res:
            res = view.connector.exit(guest, name)
            if None == res:
                exit(0)
            name = res['name']
            addr = res['addr']
            if res['enter']:
                if not name or not addr:
                    break
                secret = ctrl.secret.create()
                host = connector.enter(addr, guest, name, secret, privacy=privacy)
                if not host:
                    ctrl.secret.delete(secret)
                    break
                records.update({addr:{'host':host, 'secret':secret}})
                res = False
            else:
                if not name or not addr:
                    exit(0)
                host = records[addr]['host']
                secret = records[addr]['secret']
                res = connector.exit(addr, guest, name, secret, privacy=privacy, host=host)
                if res:
                    ctrl.secret.delete(secret)
                    del records[addr]
    