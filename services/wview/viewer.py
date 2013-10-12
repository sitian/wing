#      viewer.py
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

import threading
from threading import Thread

import sys
sys.path.append('../../lib')
from stream import pack

class WViewer(Thread):
    def __init__(self, window):
        Thread.__init__(self)
        self._window = window
        self._lock_sock = threading.Lock()
        self._lock_proc = threading.Lock()
        self._lock_proc.acquire()
        self._sock = None
        self._op = None
        self._args = None
    
    @property
    def name(self):
        return self._window.name
    
    def new_task(self, sock, op, args):
        self._lock_sock.acquire()
        self._sock = sock
        self._op = op
        self._args = args
        self._lock_proc.release()
        
    def run(self):
        while True:
            self._lock_proc.acquire()
            try:
                func = getattr(self._window, self._op)
                if self._args:
                    res = func(*self._args)
                else:
                    res = func()
                self._sock.send(pack(res))
            finally:
                self._sock.close()
                self._sock = None
                self._op = None
                self._args = None
                self._lock_sock.release()
        
    