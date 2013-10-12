#      wmon.py
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
from stream import pack, unpack
from default import WMON_ADDR, WMON_PORT

WMON_HEARTBEAT_MAX = 32 # Seconds
WMON_HEARTBEAT_MIN = WMON_HEARTBEAT_MAX / 2

class WMonProc(object):
    def __init__(self, trig, addr, timeout):
        self._sock = None
        self._trig = trig
        self._addr = addr
        self._timeout = timeout
    
    def __getattr__(self, op):
        self._op = op
        return self._proc
    
    def _open(self):
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.bind((self._addr, 0))
        self._sock.connect((WMON_ADDR, WMON_PORT))
    
    def _close(self):
        if self._sock:
            self._sock.close()
            self._sock = None
            
    def _send(self, buf):
        if self._sock:
            self._sock.send(pack(buf))
    
    def _recv(self):
        if self._sock:
            return unpack(self._sock)
        
    def _proc(self, *args):
        try:
            self._open()
            cmd = {'trigger':self._trig, 'op':self._op, 'args':args, 'timeout':self._timeout}
            self._send(cmd)
            return self._recv()
        except:
            log_err('WMonProc: failed')
        finally:
            self._close()
    
class WMon(object):
    def __init__(self, addr, timeout=None):
        self._addr = addr
        self._timeout = timeout
      
    def __getattr__(self, trig):
        return WMonProc(trig, self._addr, self._timeout)
    