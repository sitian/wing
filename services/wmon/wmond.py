#      wmond.py
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

import socket
from trig.heartbeat import Heartbeat
from multiprocessing import TimeoutError
from multiprocessing.pool import ThreadPool
from threading import Thread

import sys
sys.path.append('../../lib')
from stream import pack, unpack
from log import log_err
from default import *

class WMond(Thread):
    def _add_trig(self, trig):
        trig.start()
        self._triggers.update({trig.name:trig})
        
    def _init_trig(self):
        self._add_trig(Heartbeat())
        
    def _init_srv(self):
        self._srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._srv.bind((WMON_ADDR, WMON_PORT))
        self._srv.listen(5)
        
    def __init__(self):
        Thread.__init__(self)
        self._triggers = {}
        self._init_trig()
        self._init_srv()
    
    @property
    def name(self):
        return self.__class__.__name__
    
    def proc(self, sock, trig, op, args, timeout):
        pool = ThreadPool(processes=1)
        async = pool.apply_async(trig.proc, (op, args))
        try:
            res = async.get(timeout)
            sock.send(pack(res))
        except TimeoutError:
            log_err('%s: failed to process (timeout)' % self.name)
            pool.terminate()
        finally:
            sock.close()
    
    def run(self):
        while True:
            try:
                sock = None
                sock = self._srv.accept()[0]
                req = unpack(sock)
                try:
                    op = req['op']
                    args = req['args']
                    timeout = req['timeout']
                    trig = self._triggers[req['trigger']]
                except:
                    log_err('%s: invalid parameters' % self.name)
                    sock.close()
                    continue
                thread = Thread(target=self.proc, args=(sock, trig, op, args, timeout))
                thread.start()
            except:
                if sock:
                    sock.close()
    
if __name__ == '__main__':
    mond = WMond()
    mond.start()
    mond.join()
    
    

