#      heartbeat.py
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

import time
from threading import Lock
from hdlr.crash import Crash
from trigger import WMonTrigger

import sys
sys.path.append('../../lib')
from wmon import *

SLEEP_TIME = 1 # Seconds

class Heartbeat(WMonTrigger):
    def __init__(self):
        WMonTrigger.__init__(self)
        self._lock = Lock()
        self._nodes = {}
        
    def run(self):
        while True:
            time.sleep(SLEEP_TIME)
            self._lock.acquire()
            timeout = []
            for n in self._nodes:
                if time.time() - self._nodes[n] >= WMON_HEARTBEAT_MAX:
                    timeout.append(n)
                    Crash(n).start()
            for n in timeout:
                del self._nodes[n]
            self._lock.release()
    
    def update(self, addr):
        self._lock.acquire()
        if self._nodes.has_key(addr):
            self._nodes.update({addr:time.time()})
            ret = True
        else:
            ret = False
        self._lock.release()
        return ret
    
    def create(self, addr):
        self._lock.acquire()
        self._nodes.update({addr:time.time()})
        self._lock.release()
        return True
    
    def destroy(self, addr):
        self._lock.acquire()
        if self._nodes.has_key(addr):
            del self._nodes[addr]
        self._lock.release()
        return True
    