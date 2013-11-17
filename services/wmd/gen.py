#      gen.py
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

import sys
import zmq
from sub import getsub
from json import dumps

sys.path.append('../../lib')
from default import WMD_GEN_PORT, WMD_REG_PORT
from default import LOCAL_HOST, WMD_PORT
from default import zmqaddr, getdef
from path import PATH_WING_CONFIG
from log import log_err
from pub import WMDPub
import net

class WMDGen(object):
    def _init_sock(self, ip):
        self._context = zmq.Context()
        self._sock = self._context.socket(zmq.REP)
        self._sock.bind(zmqaddr(ip, WMD_PORT))
        self._ip = ip
        
    def __init__(self, ip):
        self._init_sock(ip)
        self._pub = None
    
    def _register(self):
        sub = getsub(self._ip, WMD_REG_PORT)
        if not sub:
            sys.exit(0)
        self._pub = WMDPub(self._ip, WMD_GEN_PORT)
        self._pub.register(sub)
        
    def run(self):
        self._register()
        while True:
            try:
                buf = self._sock.recv()
            except:
                log_err(self, 'failed to receive')
                continue
            try:
                self._pub.send(buf)
                ret = True
            except:
                log_err(self, 'failed to send')
                ret = False
            self._sock.send(dumps(ret))
    
if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(0)
    WMDGen(sys.argv[1]).run()
