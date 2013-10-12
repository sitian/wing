#      wroute.py
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
#
#      This work originally started from the example of Paranoid Pirate Pattern,
#      which is provided by Daniel Lundin <dln(at)eintr(dot)org>

import zmq
import uuid
import json
from log import log_err
from threading import Lock
from default import ROUTER_FRONTEND_ADDR

REQ_RETRIES = 3
REQ_TIMEOUT = 30 # Seconds

class WRouteReq(object):
    def _set_sock(self):
        self._sock = self._context.socket(zmq.REQ)
        self._sock.setsockopt(zmq.IDENTITY, self._id)
        self._sock.connect(ROUTER_FRONTEND_ADDR)
        self._poller.register(self._sock, zmq.POLLIN)

    @property
    def name(self):
        return self.__class__.__name__
    
    def _close_sock(self):
        self._poller.unregister(self._sock)
        self._sock.setsockopt(zmq.LINGER, 0)
        self._sock.close()
        
    def _reset_sock(self):
        self._close_sock()
        self._set_sock()
        
    def __init__(self):
        self._seq = 0
        self._id = bytes(uuid.uuid4())
        self._context = zmq.Context(1)
        self._poller = zmq.Poller()
        self._lock = Lock()
        self._set_sock()
        self._active = True
    
    def _send(self, buf):
        self._sock.send(str(self._seq), zmq.SNDMORE)
        self._sock.send(buf)
        
    def send(self, args):
        self._lock.acquire()
        if not self._active:
            self._lock.release()
            return
        cnt = 0
        res = None
        self._seq += 1
        try:
            if type(args) != type(''):
                buf = json.dumps(args)
            else:
                buf = args
            while cnt < REQ_RETRIES:
                cnt += 1
                self._send(buf)
                while True:
                    socks = dict(self._poller.poll(REQ_TIMEOUT * 1000))
                    if socks.get(self._sock):
                        reply = self._sock.recv_multipart()
                        if len(reply) != 2:
                            break
                        if int(reply[0]) == self._seq:
                            cnt = REQ_RETRIES
                            res = json.loads(reply[1])
                            break
                    else:
                        if cnt < REQ_RETRIES:
                            self._reset_sock()
                        break
        finally:
            self._lock.release()
        return res
    
    def release(self):
        self._lock.acquire()
        if not self._active:
            self._lock.release()
            return
        try:
            self._close_sock()
            self._context.term()
            self._active = False
        finally:
            self._lock.release()

class WRouteProc(object):
    def __init__(self, timeout, task):
        self._timeout = timeout
        self._task = task
    
    def __getattr__(self, op):
        self._op = op
        return self._proc
    
    def _proc(self, *args):
        try:
            req = WRouteReq()
            cmd = {'task':self._task, 'args':{'op':self._op, 'args':args}, 'timeout':self._timeout}
            res = req.send(cmd)
            req.release()
            return res
        except:
            log_err("WRouteProc: failed")
        
class WRoute(object):
    def __init__(self, timeout=None):
        self._timeout = timeout
    
    def __getattr__(self, task):
        return WRouteProc(self._timeout, task)
    