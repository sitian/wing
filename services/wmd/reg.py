#      reg.py
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
from sub import WMDSub
from json import dumps, loads
from threading import Thread
from threading import Event
from threading import Lock

sys.path.append('../../lib')
from log import log, log_err
from default import zmqaddr
import net

class WMDReg(Thread):
    def _init_sock(self, ip, port):
        self._context = zmq.Context()
        self._sock = self._context.socket(zmq.REP)
        self._sock.bind(zmqaddr(ip, port))
    
    def __init__(self, ip, port):
        Thread.__init__(self)
        self._init_sock(ip, port)
        self._stop_lock = Lock()
        self._sub_addr = {}
        self._sub_id = {}
        self._start = {}
        self._stop = {}
        self._sub = {}
        self._ip = ip
    
    def _proc(self, addr):
        pass
    
    def _fault(self, addr):
        pass
    
    def _live(self, addr):
        pass
    
    def _start_proc(self, addr):
        try:
            self._proc(addr)
        except:
            log_err(self, 'failed to process')
    
    def _start_sub(self, addr):
        self._sub[addr].start()
        self._start.update({addr:Event()})
        self._stop.update({addr:Event()})
        t = Thread(target=self._start_proc, args=(addr,))
        t.setDaemon(True)
        t.start()
        self._start[addr].wait()
        self._stop[addr].set()
        self._stop_lock.acquire()
        self._stop_lock.release()
    
    def _stop_sub(self, addr):
        self._stop_lock.acquire()
        self._sub[addr].stop()
        self._start[addr].set()
        self._stop[addr].wait()
        sub_id = self._sub_id[addr]
        del self._sub_addr[sub_id]
        del self._sub_id[addr]
        del self._start[addr]
        del self._stop[addr]
        del self._sub[addr]
        self._stop_lock.release()
    
    def get_addr(self, identity):
        return self._sub_addr.get(identity)
    
    @property
    def identity(self):
        return idx._id2str(chr(int(self._ip.split('.')[3])))
        
    def run(self):
        while True:
            try:
                sub = None
                buf = self._sock.recv()
                if not buf:
                    log_err(self, 'failed to receive')
                    continue
                args = loads(buf)
                ip = args['ip']
                port = args['port']
                index = args['index']
                heartbeat = args['hb']
                identity = idx.idxid(index)
                
                exist = False
                addr = net.aton(ip)
                if self._sub_id.has_key(addr): 
                    if self._sub_id[addr] != identity:
                        self._stop_sub(addr)
                    else:
                        exist = True
                    
                if not exist:
                    self._sub_id.update({addr:identity})
                    self._sub_addr.update({identity:addr})
                    sub = WMDSub(ip, port, index, heartbeat, self._fault, self._live)
                    self._sub.update({addr:sub})
                    Thread(target=self._start_sub, args=(addr,)).start()
                    
                self._sock.send(dumps(True))
            except:
                log_err(self, 'failed to register')
                continue
    