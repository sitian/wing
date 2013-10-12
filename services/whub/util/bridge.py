#      bridge.py
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

import os
import re
import device
from commands import getstatusoutput

import sys
sys.path.append('../../lib')
from path import *

def chkaddr(addr):
    ip = addr.split('.')
    return '172.16.%s.1' % ip[3]

def chkrange(addr):
    ip = addr.split('.')
    return '172.16.%s.2,172.16.%s.254' % (ip[3], ip[3])

def chkname(name):
    return 'br%s' % name

def add_dhcp(name, addr):
    br_name = chkname(name)
    br_addr = chkaddr(addr)
    br_range = chkrange(addr)
    path_pid = os.path.join(PATH_WING_VAR_RUN, br_name)
    cmd = 'dnsmasq --strict-order --bind-interfaces --pid-file=%s' % path_pid\
        + ' --listen-address %s --dhcp-range %s' % (br_addr, br_range)\
        + ' --dhcp-lease-max=253 --dhcp-no-override --except-interface=lo'\
        + ' --interface=%s' % br_name\
        + ' --dhcp-authoritative'
    if getstatusoutput(cmd)[0]:
        return False
    return True
    
def del_dhcp(name):
    br_name = chkname(name)
    path_pid = os.path.join(PATH_WING_VAR_RUN, br_name)
    if os.path.exists(path_pid):
        f = open(path_pid)
        pid = f.readline().strip()
        f.close()
        getstatusoutput('kill -9 %s' %  pid)
        os.remove(path_pid)
    
def add_br(name, addr):
    br_name = chkname(name)
    br_addr = chkaddr(addr)
    cmd = 'brctl addbr %s' % br_name
    if getstatusoutput(cmd)[0]:
        return False
    cmd = 'ifconfig %s %s netmask 255.255.255.0 up' % (br_name, br_addr)
    if getstatusoutput(cmd)[0]:
        return False
    return True

def del_br(name):
    br_name = chkname(name)
    getstatusoutput('ifconfig %s down' % br_name)
    getstatusoutput('brctl delbr %s' %  br_name)
    
def state(name):
    br_name = chkname(name)
    cmd = 'brctl show ' + br_name
    output = getstatusoutput(cmd)[1]
    m = re.search('No such device', output)
    if m:
        return device.STATE_INACTIVE
    else:
        return device.STATE_ACTIVE
    
def create(name, addr):
    if state(name) == device.STATE_ACTIVE:
        destroy(name)
    
    if not add_br(name, addr):
        destroy(name)
        raise Exception('failed to create bridge')
    
    del_dhcp(name)
    if not add_dhcp(name, addr):
        destroy(name)
        raise Exception('failed to create bridge')
    
def destroy(name):
    del_dhcp(name)
    del_br(name)
    