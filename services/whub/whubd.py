#      whubd.py
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

import struct
import socket
from dev.network import Network
from dev.storage import Storage
from dev.container import Container
from multiprocessing.pool import ThreadPool
from multiprocessing import TimeoutError
from connector import WHubConnector
from threading import Lock, Thread
from auth import WHubAuth

import sys
sys.path.append('../../lib')
from stream import stream_input, stream_output
from default import NET_ADAPTER, WHUB_PORT
from log import log, log_err
import net

WHUB_NR_LOCKS = 1024

class WHubd(object):
    def _add_dev(self, dev):
        self._dev.update({dev.name:dev})
        
    def _init_dev(self):
        self._dev = {}
        self._add_dev(Network())
        self._add_dev(Storage())
        self._add_dev(Container())
        
    def _init_srv(self):
        addr = net.chkiface(NET_ADAPTER)
        self._srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._srv.bind((addr, WHUB_PORT))
        self._srv.listen(5)
    
    def __init__(self):
        self._init_dev()
        self._init_srv()
        self._auth = WHubAuth()
        self._conn = WHubConnector()
        self._locks = []
        for i in range(0, WHUB_NR_LOCKS):
            self._locks.append(Lock())

    def _lock_get(self, addr):
        val = struct.unpack('!I', socket.inet_aton(addr))[0]
        return self._locks[val % WHUB_NR_LOCKS]

    def _lock_acquire(self, addr):
        lock = self._lock_get(addr)
        lock.acquire()
  
    def _lock_release(self, addr):
        lock = self._lock_get(addr)
        lock.release()

    @property
    def name(self):
        return self.__class__.__name__
    
    def enter(self, guest, name, secret, password, privacy):
        if not net.validate(guest):
            log_err('%s: failed, invalid addr' % self.name)
            return
        res = None
        self._lock_acquire(guest)
        if not self._auth.exist(guest, name, password, privacy):
            if self._auth.confirm(guest, name, password, privacy):
                log('%s: enter (%s)' % (self.name, name))
                res = self._conn.connect(guest, name, secret, privacy)
        self._lock_release(guest)
        return res
    
    def exit(self, guest, name, secret, password, privacy, host):
        if not net.validate(guest):
            log_err('%s: failed, invalid addr' % self.name)
            return
        res = None
        self._lock_acquire(guest)
        if self._auth.exist(guest, name, password, privacy, host):
            log('%s: exit (%s)' % (self.name, name))
            res = self._conn.disconnect(guest, name, secret, privacy, host)
        self._lock_release(guest)
        return res
    
    def proc(self, sock, dev, op, args, timeout):
        try:
            res = None
            if dev:
                if not self._dev.has_key(dev):
                    log_err('%s: no such device %s' % (self.name, dev))
                    return
                pool = ThreadPool(processes=1)
                async = pool.apply_async(self._dev[dev].proc, (op, args))
                try:
                    res = async.get(timeout)
                except TimeoutError:
                    log_err('%s: failed to process (timeout)' % self.name)
                    pool.terminate()
                    return
            else:
                if op == 'enter':
                    res = self.enter(*args)
                elif op == 'exit':
                    res = self.exit(*args)
            stream_input(sock, res)
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
                    if req.has_key('dev'):
                        dev = req['dev']
                    else:
                        dev = None
                except:
                    log_err('%s: invalid parameters' % self.name)
                    sock.close()
                    continue
                thread = Thread(target=self.proc, args=(sock, dev, op, args, timeout))
                thread.start()
            except:
                if sock:
                    sock.close()
    
if __name__ == '__main__':
    WHubd().run()
