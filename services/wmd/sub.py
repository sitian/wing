#      sub.py
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
import idx
import time

from idx import WMDIndex
from threading import Event
from threading import Thread

sys.path.append('../../lib')
from default import WMD_HEARTBEAT_INTERVAL, WMD_HEARTBEAT_PORT
from default import zmqaddr
from log import log
import net

WMD_SUB_SHOW_ADDR = True
WMD_SUB_SHOW_DISCONNECT = True
        
class WMDSub(WMDIndex, Thread):
    def _init_sock(self, ip, port):
        self._context = zmq.Context()
        self._sock = self._context.socket(zmq.SUB)
        self._sock.setsockopt(zmq.SUBSCRIBE, "")
        self._sock.connect(zmqaddr(ip, port))
        self._release = False
        self._start = Event()
        self._stop = Event()
        self._port = port
        self._ip = ip
    
    def __init__(self, ip, port, index, heartbeat=False, fault=None, live=None):
        Thread.__init__(self)
        WMDIndex.__init__(self, index)
        self._heartbeat = heartbeat
        self._init_sock(ip, port)
        self._addr = net.aton(ip)
        self._fault = fault
        self._live = live
        if WMD_SUB_SHOW_ADDR:
            log('WMDSub: << %s (id=%s)' % (ip, str(idx.getid(index))))
    
    def _remove(self):
        if self._fault:
            return self._fault(self._addr)
        else:
            return True
        
    def _keep_alive(self):
        if self._live:
            self._live(self._addr)
    
    def remove(self):
        self._start.set()
        self._remove()
    
    def recv(self):
        return self.idxrcv(self._sock)
        
    def _destroy(self):
        self._context.destroy()
        self._release = True
        self._start.set()
    
    def _start_heartbeat(self):
        context = zmq.Context()
        sock = context.socket(zmq.REQ)
        sock.connect(zmqaddr(self._ip, WMD_HEARTBEAT_PORT))
        while True:
            err = 1
            try:
                sock.send('h')
                time.sleep(WMD_HEARTBEAT_INTERVAL)
                if sock.recv(zmq.NOBLOCK) == 'l':
                    self._keep_alive()
                    err = 0
            finally:
                if err:
                    if WMD_SUB_SHOW_DISCONNECT:
                        log('WMDSub: cannot connect to %s' % self._ip)
                    if self._remove():
                        context.destroy()
                        self._destroy()
                        break
                    else:
                        context.destroy()
                        context = zmq.Context()
                        sock = context.socket(zmq.REQ)
                        sock.connect(zmqaddr(self._ip, WMD_HEARTBEAT_PORT))
                        
    def run(self):
        if self._heartbeat:
            t = Thread(target=self._start_heartbeat)
            t.setDaemon(True)
            t.start()
            self._start.wait()
            self._stop.set()
    
    def stop(self):
        if self._heartbeat:
            self._start.set()
            self._stop.wait()
        if not self._release:
            self._context.destroy()
    