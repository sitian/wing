#      container.py
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
import struct
import socket
import bridge
from commands import getstatusoutput

import sys
sys.path.append('../lib')
from default import *
from path import *

ADDR = '10.0.0.1'
NAME = '%x' % struct.unpack("I", socket.inet_aton(ADDR))[0]

def init():
    dir_lxc = os.path.join(PATH_LXC, NAME)
    if os.path.exists(dir_lxc):
        clear()
    
    dir_env = os.path.join(PATH_WING_ENV, NAME)
    if os.path.exists(dir_env):
        getstatusoutput('rm -rf %s' % dir_env)
    os.mkdir(dir_env)
    
    cfg = []
    cfg.append('lxc.network.type = veth\n')
    cfg.append('lxc.network.flags = up\n')
    cfg.append('lxc.network.link = %s\n' % bridge.NAME)
    cfg.append('lxc.network.name = eth0\n')
    cfg.append('lxc.network.ipv4 = %s/16\n' % DEFAULT_ADDR)
    cfg.append('lxc.network.mtu = 1500\n')
    
    path = os.path.join(dir_env, 'config')
    f = open(path, 'w')
    f.writelines(cfg)
    f.close()
    
    cmd = 'lxc-create -n %s -t ubuntu -f %s' % (NAME, path)
    if getstatusoutput(cmd)[0]:
        getstatusoutput('rm -rf %s' % dir_env)
        raise Exception('failed to create env')
    
    path = os.path.join(PATH_LXC, NAME, PATH_WING_PUBLIC)
    getstatusoutput('umount %s' % path)
    getstatusoutput('mount --bind %s %s' % (PATH_NFS_ROOT, path))
    cmd = 'lxc-start -n %s -d' % NAME
    if getstatusoutput(cmd)[0]:
        clear()
        raise Exception('failed to start env')
    
def clear():
    getstatusoutput('lxc-stop -n %s' % NAME)
    path = os.path.join(PATH_LXC, NAME, PATH_WING_PUBLIC)
    getstatusoutput('umount %s' % path)
    getstatusoutput('lxc-destroy -n %s' % NAME)
    dir_env = os.path.join(PATH_WING_ENV, NAME)
    if os.path.exists(dir_env):
        getstatusoutput('rm -rf %s' % dir_env)
    