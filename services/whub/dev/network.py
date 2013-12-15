#      network.py
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

import device
from device import WHubDev
from util import vpn, bridge

import sys
sys.path.append('../../lib')
from privacy import PUBLIC, PRIVATE
from wroute import WRoute
from wctrl import WCtrl
from log import log_err

class Network(WHubDev):
    def state(self, name, privacy, host):
        res = WCtrl().auth.exist(name, privacy, host)
        if res:
            return device.STATE_ACTIVE
        elif False == res:
            return device.STATE_INACTIVE
        else:
            return device.STATE_UNKNOWN
    
    def register(self, name, secret, privacy, host):
        res = None
        if PUBLIC == privacy:
            res = WRoute().auth.login(name, secret, privacy, host)
        elif PRIVATE == privacy:
            res = WCtrl().auth.login(name, secret, privacy, host)
        if not res:
            log_err(self, 'failed to register net')
            return
        return True

    def unregister(self, name, secret, privacy, host):
        res = None
        if PUBLIC == privacy:
            res = WRoute().auth.logout(name, secret, privacy, host)
        elif PRIVATE == privacy:
            res = WCtrl().auth.logout(name, secret, privacy, host)
        if not res:
            log_err(self, 'failed to unregister net')
            return
        return True
    
    def mount(self, guest, name, secret):
        addr = None
        try:
            vpn.create(guest, name, secret)
            addr = vpn.start(name)
            bridge.create(name, addr)
            return True
        except:
            log_err(self, 'failed to mount')
            if addr:
                vpn.stop(name)
            vpn.destroy(name)
    
    def unmount(self, name):
        try:
            vpn.stop(name)
            vpn.destroy(name)
            bridge.destroy(name)
            return True
        except:
            log_err(self, 'failed to unmount')
    