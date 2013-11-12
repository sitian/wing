#      pub.py
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

from idx import WMDIndex
from threading import Thread
from json import dumps, loads

sys.path.append('../../lib')
from default import WMD_HEARTBEAT_PORT, zmqaddr
from log import log, log_err

WMD_PUB_SHOW_ADDR = True

class WMDPub(WMDIndex, Thread):
    def _init_sock(self, ip, port):
        self._context = zmq.Context()
        self._sock = self._context.socket(zmq.PUB)
        self._sock.bind(zmqaddr(ip, port))
        self._port = port
        self._ip = ip
    
    def __init__(self, ip, port):
        Thread.__init__(self)
        WMDIndex.__init__(self)
        self._init_sock(ip, port)
    
    def register(self, sub, heartbeat=False):
        if not sub:
            return
        
        if heartbeat:
            self.start()
        
        context = zmq.Context()
        req = dumps({'ip': self._ip, 'port':self._port, 'index':self.idxchk(), 'hb':heartbeat})
        for addr in sub:
            if addr[0] == self._ip:
                continue
            sock = context.socket(zmq.REQ)
            sock.connect(zmqaddr(addr[0], addr[1]))
            sock.send(req)
            buf = sock.recv()
            sock.close()
            if not buf or not loads(buf):
                log_err('WMDPub: failed to connect')
                raise Exception('WMDPub: failed to connect')
            if WMD_PUB_SHOW_ADDR:
                log('WMDPub: >> %s' % addr[0])
        if WMD_PUB_SHOW_ADDR:
            log('WMDPub: [run] %s' % self._ip)
    
    def close(self):
        if self._sock:
            self._sock.close()
            self._sock = None
    
    def send(self, buf, index=None):
        return self.idxsnd(self._sock, buf, index)
    
    def run(self):
        context = zmq.Context()
        sock = context.socket(zmq.REP)
        sock.bind(zmqaddr(self._ip, WMD_HEARTBEAT_PORT))
        while True:
            try:
                if sock.recv() != 'h':
                    break
                else:
                    sock.send('l')
            except:
                log_err('WMDPub: cannot communicate with subscribers')
                break
        sock.close()
