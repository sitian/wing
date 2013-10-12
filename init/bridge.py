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
from commands import getstatusoutput

import sys
sys.path.append('../lib')
from default import *
from path import *

NAME = 'brw0'
ADDR = '172.16.1.1'
ADDR_RANGE = '172.16.1.2,172.16.1.254'

def init():
    path_pid = os.path.join(PATH_WING_VAR_RUN, NAME)
    if os.path.exists(path_pid):
        os.remove(path_pid)
    cmd = 'brctl show %s' % NAME
    output = getstatusoutput(cmd)[1]
    m = re.search('No such device', output)
    if not m:
        clear()
    cmd = 'brctl addbr %s' % NAME
    if getstatusoutput(cmd)[0]:
        raise Exception('failed to create %s' % NAME)
    cmd = 'ifconfig %s %s netmask 255.255.255.0 up' % (NAME, ADDR)
    if getstatusoutput(cmd)[0]:
        clear()
        raise Exception('failed to create %s' % NAME)
    cmd = 'dnsmasq --strict-order --bind-interfaces --pid-file=%s' % path_pid\
        + ' --listen-address %s --dhcp-range %s' % (ADDR, ADDR_RANGE)\
        + ' --dhcp-lease-max=253 --dhcp-no-override --except-interface=lo'\
        + ' --interface=%s' % NAME\
        + ' --dhcp-authoritative'
    if getstatusoutput(cmd)[0]:
        clear()
        raise Exception('failed to create %s' % NAME)
    
def clear():
    getstatusoutput('ifconfig %s down' % NAME)
    getstatusoutput('brctl delbr %s' % NAME)
    path_pid = os.path.join(PATH_WING_VAR_RUN, NAME)
    if os.path.exists(path_pid):
        f = open(path_pid)
        pid = f.readline().strip()
        f.close() 
        getstatusoutput('kill -9 %s' %  pid)
        os.remove(path_pid)
    