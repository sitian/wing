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
from stream import stream_input, stream_output
from default import WMON_PORT
from log import log_err

class WMond(Thread):
    def _add_trig(self, trig):
        trig.start()
        self._triggers.update({trig.name:trig})
        
    def _init_trig(self):
        self._add_trig(Heartbeat())
        
    def _init_srv(self, addr):
        self._srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._srv.bind((addr, WMON_PORT))
        self._srv.listen(5)
        
    def __init__(self, addr):
        Thread.__init__(self)
        self._triggers = {}
        self._init_trig()
        self._init_srv(addr)
    
    @property
    def name(self):
        return self.__class__.__name__
    
    def proc(self, sock, trig, op, args, timeout):
        pool = ThreadPool(processes=1)
        async = pool.apply_async(trig.proc, (op, args))
        try:
            res = async.get(timeout)
            stream_input(sock, res)
        except TimeoutError:
            log_err(self, 'failed to process (timeout)')
            pool.terminate()
        finally:
            sock.close()
    
    def run(self):
        while True:
            try:
                sock = None
                sock = self._srv.accept()[0]
                req = stream_output(sock)
                try:
                    op = req['op']
                    args = req['args']
                    timeout = req['timeout']
                    trig = self._triggers[req['trigger']]
                except:
                    log_err(self, 'invalid parameters')
                    sock.close()
                    continue
                thread = Thread(target=self.proc, args=(sock, trig, op, args, timeout))
                thread.start()
            except:
                if sock:
                    sock.close()
    
if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(0)
    addr = sys.argv[1]
    mond = WMond(addr)
    mond.start()
    mond.join()
    
    

