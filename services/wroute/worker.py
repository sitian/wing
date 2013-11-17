#      worker.py
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
import uuid
import json
import random

from ppp import *
from tsk.auth import Auth
from threading import Thread
from multiprocessing import TimeoutError
from multiprocessing.pool import ThreadPool

import sys
sys.path.append('../../lib')
from default import WROUTE_BACKEND_ADDR
from log import log_err

class WRouteWorker(Thread):    
    def _init_tasks(self):
        self._tasks = {}
        self.add_task(Auth())
        
    def _set_sock(self):
        self._sock = self._context.socket(zmq.DEALER)
        self._sock.setsockopt(zmq.IDENTITY, self._id)
        self._poller.register(self._sock, zmq.POLLIN)
        self._sock.connect(WROUTE_BACKEND_ADDR)
        self._sock.send(PPP_READY)
    
    def _reset_sock(self):
        self._poller.unregister(self._sock)
        self._sock.setsockopt(zmq.LINGER, 0)
        self._sock.close()
        self._set_sock()
    
    def _init_sock(self):
        self._context = zmq.Context(1)
        self._poller = zmq.Poller()
        self._set_sock()
        
    def __init__(self):
        Thread.__init__(self)
        self._id = bytes(uuid.uuid4())
        self._init_sock()
        self._init_tasks()
    
    @property
    def name(self):
        return self.__class__.__name__
    
    def add_task(self, tsk):
        self._tasks.update({tsk.name:tsk})
     
    def proc(self, buf):
        try:
            args = json.loads(buf)
            tsk_name = args['task']
            tsk_args = args['args']
            tsk_timeout = args['timeout']
            if not self._tasks.has_key(tsk_name):
                log_err(self, 'no such task %s', tsk_name)
                return
            pool = ThreadPool(processes=1)
            async = pool.apply_async(self._tasks[tsk_name].proc, (), tsk_args)
            try:
                return async.get(tsk_timeout)
            except TimeoutError:
                log_err(self, 'failed to process (timeout)')
                pool.terminate()
                return
        except:
            log_err(self, 'failed to process')
        
    def run(self):
        liveness = PPP_HEARTBEAT_LIVENESS
        timeout = time.time() + PPP_HEARTBEAT_INTERVAL
        while True:
            socks = dict(self._poller.poll(PPP_HEARTBEAT_INTERVAL * 1000))
            if socks.get(self._sock) == zmq.POLLIN:
                frames = self._sock.recv_multipart()
                if not frames:
                    log_err(self, 'found an empty request')
                    break
                if len(frames) == PPP_NR_FRAMES:
                    res = self.proc(frames[PPP_FRAME_BUF])
                    msg = [frames[PPP_FRAME_ID], '', frames[PPP_FRAME_SEQ], json.dumps(res)]
                    self._sock.send_multipart(msg)
                elif len(frames) == 1 and frames[0] == PPP_HEARTBEAT:
                    liveness = PPP_HEARTBEAT_LIVENESS
                else:
                    log_err(self, "invalid request, %s" % frames)
            else:
                liveness -= 1
                if liveness == 0:
                    time.sleep(random.randint(0, PPP_SLEEP_TIME))
                    self._reset_sock()
                    liveness = PPP_HEARTBEAT_LIVENESS
                    
            if time.time() > timeout:
                timeout = time.time() + PPP_HEARTBEAT_INTERVAL
                self._sock.send(PPP_HEARTBEAT)
    