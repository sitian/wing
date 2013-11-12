#      poller.py
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
import time
from ppp import *
from queue import WRouteQueue
from queue import WRouteItem
from threading import Thread

import sys
sys.path.append('../../lib')
from default import WROUTE_ADDR, WROUTE_BACKEND_ADDR
from log import log_err

class WRoutePoller(Thread):
    def _init_sock(self):
        self._context = zmq.Context(1)
        self._frontend = self._context.socket(zmq.ROUTER)
        self._backend = self._context.socket(zmq.ROUTER)
        self._frontend.bind(WROUTE_ADDR)
        self._backend.bind(WROUTE_BACKEND_ADDR)
        self._poller_backend = zmq.Poller()
        self._poller_backend.register(self._backend, zmq.POLLIN)
        self._poller = zmq.Poller()
        self._poller.register(self._frontend, zmq.POLLIN)
        self._poller.register(self._backend, zmq.POLLIN)
        
    def __init__(self):
        Thread.__init__(self)
        self._queue = WRouteQueue()
        self._init_sock()
        self._active = True
    
    @property
    def name(self):
        return self.__class__.__name__
    
    def run(self):
        timeout = time.time() + PPP_HEARTBEAT_INTERVAL
        while self._active:
            if len(self._queue.queue) > 0:
                poller = self._poller
            else:
                poller = self._poller_backend
            socks = dict(poller.poll(PPP_HEARTBEAT_INTERVAL * 1000))
            if socks.get(self._backend) == zmq.POLLIN:
                frames = self._backend.recv_multipart()
                if not frames:
                    break
                identity = frames[0]
                self._queue.add(WRouteItem(identity))
                msg = frames[1:]
                if len(msg) == 1:
                    if msg[0] not in (PPP_READY, PPP_HEARTBEAT):
                        log_err("%s: invalid message, %s" % (self.name, msg))
                else:
                    #log("Reply: id=%s, seq=%s" % (msg[PPP_FRAME_ID], msg[PPP_FRAME_SEQ]))
                    self._frontend.send_multipart(msg)
                
                if time.time() >= timeout:
                    for identity in self._queue.queue:
                        msg = [identity, PPP_HEARTBEAT]
                        self._backend.send_multipart(msg)
                    timeout = time.time() + PPP_HEARTBEAT_INTERVAL
                    
            if socks.get(self._frontend) == zmq.POLLIN:
                frames = self._frontend.recv_multipart()
                if not frames:
                    break
                frames.insert(0, self._queue.pop())
                self._backend.send_multipart(frames)
            
            self._queue.purge()
            
    def stop(self):
        self._active = False
    