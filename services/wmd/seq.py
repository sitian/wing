#      seq.py
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

import sys
import idx
import time
from reg import WMDReg
from pub import WMDPub
from sub import getsub
from threading import Lock
from json import dumps, loads

sys.path.append('../../lib')
from default import WMD_REG_PORT, WMD_MIX_PORT, WMD_SEQ_PORT
from log import log, log_err, log_get
import net

WMD_SEQ_CHK_SLOW = True
WMD_SEQ_SHOW_FWD = False

def pack(track, index, cmd):
    return dumps((track, index, cmd))

def unpack(buf):
    return loads(buf)

class WMDSeq(WMDReg):
    def __init__(self, ip, cmd):
        WMDReg.__init__(self, ip, WMD_REG_PORT)
        self._addr = net.aton(ip)
        self._lock = Lock()
        self._pub = None
        self._cmd = cmd
    
    def _show_fwd(self, index):
        if WMD_SEQ_SHOW_FWD:
            log(self, '[fwd] sn=%d' % idx.idxsn(index))
        
    def register(self):
        ip = net.ntoa(self._addr)
        sub = getsub(ip, WMD_MIX_PORT)
        if not sub:
            sys.exit(0)
        self._pub = WMDPub(ip, WMD_SEQ_PORT, idx.idxgen(identity=self.identity))
        self._pub.register(sub, heartbeat=True)
    
    def _wrapper(self, track, index, cmd):
        new_index = self._pub.idxget()
        new_cmd = pack(track, index, cmd)
        return (new_index, new_cmd)
        
    def _forward(self, index, cmd):
        self._lock.acquire()
        try:
            new_index, new_cmd = self._cmd.wrap(self._addr, index, cmd, self._wrapper)
            self._pub.send(new_cmd, new_index)
            self._show_fwd(new_index)
        except:
            log_err(self, 'failed to forward, index=%s' % index)
            raise Exception(log_get(self, 'failed to forward'))
        finally:
            self._lock.release()
    
    def _recv(self, addr):
        try:
            if WMD_SEQ_CHK_SLOW:
                self._cmd.chkslow()
            return self._sub[addr].recv()
        except:
            log_err(self, 'failed to receive, %s' % net.ntoa(addr))
            self._stop_sub(addr)
            raise Exception(log_get(self, 'failed to receive'))
    
    def _can_start(self):
        while not self._pub:
            time.sleep(1)
        return True
    
    def _proc(self, addr):
        if self._can_start():
            while True:
                self._forward(*self._recv(addr))
                
    def update(self, cmd):
        track, orig, command = unpack(cmd)
        identity = idx.idxid(orig)
        addr = self._sub_addr.get(identity)
        if not addr or not self._sub.has_key(addr):
            log_err(self, 'failed to update (no subscriber)')
            raise Exception(log_get(self, 'failed to update'))
        if self._sub[addr].idxnxt(orig, command):
            self._forward(orig, command)
        