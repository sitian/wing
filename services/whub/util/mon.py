#      mon.py
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

import vpn
import sys
sys.path.append('../../lib')
from wctrl import WCtrl
from wmon import WMon

def chkaddr(name):
    adapter = vpn.chkadapter(name)
    return vpn.chkaddr(adapter)

def create(name):
    addr = chkaddr(name)
    if not addr:
        raise Exception('failed to create mon')
    if not WMon(addr).heartbeat.create(addr):
        raise Exception('failed to create mon')

def destroy(name):
    addr = chkaddr(name)
    if not addr:
        raise Exception('failed to destroy mon')
    if not WMon(addr).heartbeat.destroy(addr):
        raise Exception('failed to destroy mon')

def start(name):
    addr = chkaddr(name)
    if not addr:
        raise Exception('failed to start mon')
    if not WCtrl().heartbeat.start(name, addr):
        raise Exception('failed to start mon')
    
def stop(name):
    addr = chkaddr(name)
    if not addr:
        raise Exception('failed to stop mon')
    if not WCtrl().heartbeat.stop(name, addr):
        raise Exception('failed to stop mon')
