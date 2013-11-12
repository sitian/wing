#      net.py
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

import fcntl
import struct
import socket

def chkiface(name):
    addr = None
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        addr = socket.inet_ntoa(fcntl.ioctl(sock.fileno(), 0x8915, struct.pack('256s', name))[20:24])
    finally:
        return addr
    
def aton(addr):
    return struct.unpack('I', socket.inet_aton(addr))[0]

def ntoa(addr):
    return socket.inet_ntoa(struct.pack('I', addr)) 

def validate(addr):
    ret = False
    try:
        if addr and aton(addr):
            ret = True
    finally:
        return ret
