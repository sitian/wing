#      wviewd.py
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
from util import ui
from dlg.acceptor import Acceptor
from dlg.connector import Connector
from threading import Thread
from viewer import WViewer

import sys
sys.path.append('../../lib')
from stream import pack, unpack
from default import *
    
class WViewd(Thread):
    def _init_viewers(self):
        windows = [Acceptor(), Connector()]
        self._viewers = {}
        for i in range(len(windows)):
            viewer = WViewer(windows[i])
            self._viewers.update({viewer.name:viewer})
            viewer.start()
    
    def _init_srv(self, addr):
        self._srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._srv.bind((addr, WVIEW_PORT))
        self._srv.listen(5)
            
    def __init__(self, addr=None):
        if not addr:
            addr = '127.0.0.1'
        Thread.__init__(self)
        self._init_viewers()
        self._init_srv(addr)
    
    def run(self):
        while True:
            try:
                sock = None
                sock = self._srv.accept()[0]
                req = unpack(sock)
                try:
                    viewer = self._viewers[req['viewer']]
                    op = req['op']
                    args = req['args']
                except:
                    sock.close()
                    return
                viewer.new_task(sock, op, args)
            except:
                if sock:
                    sock.close()

if __name__ == '__main__':
    ui.create()
    WViewd().start()
    ui.start()
    