#      wview.py
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
from log import log_err
from default import WVIEW_PORT, LOCAL_HOST
from stream import stream_input, stream_output

class WViewProc(object):
    def __init__(self, viewer):
        self._viewer = viewer
        self._sock = None
    
    def __getattr__(self, op):
        self._op = op
        return self._proc
    
    def _open(self):
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.connect((LOCAL_HOST, WVIEW_PORT))
    
    def _close(self):
        if self._sock:
            self._sock.close()
            self._sock = None
    
    def _send(self, cmd):
        if self._sock:
            stream_input(self._sock, cmd)
    
    def _recv(self):
        if self._sock:
            return stream_output(self._sock)
    
    def _proc(self, *args):
        try:
            self._open()
            cmd = {'viewer':self._viewer, 'op':self._op, 'args':args}
            self._send(cmd)
            return self._recv()
        except:
            log_err(self, 'failed')
        finally:
            self._close()
    
class WView(object):
    def __getattr__(self, viewer):
        return WViewProc(viewer)
