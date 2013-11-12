#      vpn.py
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
import time
from commands import getstatusoutput

import sys
sys.path.append('../../lib')
from path import *
import stream
import net

VPN_RETRY_MAX = 5
VPN_RETRY_INTERVAL = 3 # Seconds
   
def create(guest, name, secret):
    path = os.path.join(PATH_PPP_PEERS, name)
    if os.path.exists(path):
        getstatusoutput('poff %s' % name)
        getstatusoutput('pptpsetup --delete %s' % name)
    user, passwd = stream.split(secret)
    cmd = 'pptpsetup --create ' + name\
                + ' --server ' + guest\
                + ' --username ' + user\
                + ' --password ' + passwd
    if getstatusoutput(cmd)[0]:
        raise Exception('failed to create vpn')
    f = open(path, 'a')
    f.write('linkname %s\n' % name)
    f.close()

def destroy(name):
    getstatusoutput('pptpsetup --delete %s' % name)
    
def chkadapter(name):
    cnt = 0
    adapter = ''
    path = '/var/run/ppp-%s.pid' % name
    while cnt < VPN_RETRY_MAX:
        cnt += 1
        try:
            f = open(path, 'r')
            lines = f.readlines()
            f.close()
            adapter = lines[1].strip()
            break
        except:
            if cnt < VPN_RETRY_MAX:
                time.sleep(VPN_RETRY_INTERVAL)
    return adapter

def chkaddr(iface):
    cnt = 0
    vaddr = ''
    while cnt < VPN_RETRY_MAX:
        cnt += 1
        vaddr = net.chkiface(iface)
        if vaddr:
            break
        elif cnt < VPN_RETRY_MAX:
            time.sleep(VPN_RETRY_INTERVAL)
    return vaddr
    
def start(name):
    cmd = 'pon %s' % name
    if getstatusoutput(cmd)[0]:
        raise Exception('failed to start vpn')
    
    adapter = chkadapter(name)
    if not adapter:
        getstatusoutput('poff %s' % name)
        raise Exception('failed to start vpn')
    
    addr = chkaddr(adapter)
    if not addr:
        getstatusoutput('poff %s' % name)
        raise Exception('failed to start vpn')
    
    ip = addr.split('.')
    network = '%s.%s.%s.0' % (ip[0], ip[1], ip[2])
    getstatusoutput('ip route flush dev %s' %  adapter)
    getstatusoutput('ip route add %s/24 dev %s' % (network, adapter))
    return addr
    
def stop(name):
    path = '/var/run/ppp-%s.pid' % name
    try:
        f = open(path, 'r')
        lines = f.readlines()
        f.close()
        adapter = lines[1].strip()
        if not adapter:
            return
    except:
        return
    getstatusoutput('ip route flush dev %s' % adapter)
    getstatusoutput('poff %s' % name)
    