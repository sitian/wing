#      wmd.py
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

import zmq
from json import loads
from log import log_err
from default import WMD_PORT, zmqaddr

class WMDReq(object):
    def _init_sock(self):
        self._context = zmq.Context()
        self._sock = self._context.socket(zmq.REQ)
        self._connected = False
    
    def __init__(self, addr):
        self._init_sock()
        self._addr = addr
    
    def connect(self):
        if not self._connected:
            self._sock.connect(zmqaddr(self._addr, WMD_PORT))
            self._connected = True
            
    def send(self, buf):
        ret = False
        if not self._connected:
            log_err(self, 'no connection')
            return ret
        try:
            self._sock.send(buf)
            res = self._sock.recv()
            if res and loads(res):
                ret = True
        except:
            log_err(self, 'failed to send')
        finally:
            return ret
    
    def close(self):
        if self._connected:
            self._sock.close()
            self._connected = False
    