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
import mds
import time
from json import dumps, loads
from threading import Lock
from reg import WMDReg
from pub import WMDPub

sys.path.append('../../lib')
from default import WMD_REG_PORT, WMD_MIX_PORT
from default import WMD_SEQ_PORT, getdef
from log import log, log_err
import net

WMD_SEQ_SHOW_RECV = True
WMD_SEQ_SHOW_UPDATE = True
WMD_SEQ_SHOW_FORWARD = True

class WMDSeq(WMDReg):
    def __init__(self, ip, cmd):
        WMDReg.__init__(self, ip, WMD_REG_PORT)
        self._addr = net.aton(ip)
        self._lock = Lock()
        self._pub = None
        self._cmd = cmd
        
    def _register(self):
        ip = net.ntoa(self._addr)
        sub = mds.get(ip, WMD_MIX_PORT)
        if not sub:
            sys.exit(0)
        self._pub = WMDPub(ip, WMD_SEQ_PORT)
        self._pub.register(sub, heartbeat=True)
    
    def _recv(self, addr):
        try:
            index, cmd = self._sub[addr].recv()
            if WMD_SEQ_SHOW_RECV:
                idx.show('WMDSeq: [rcv]', index, addr)
            return (index, cmd)
        except:
            log_err('WMDSeq: failed to receive')
            return (None, None)
    
    def _pack(self, index, cmd):
        return dumps((index, cmd, self._cmd.track()))
    
    def _unpack(self, cmd):
        return loads(cmd)
    
    def _forward(self, index, cmd):
        self._lock.acquire()
        try:
            new_idx = self._pub.idxget()
            new_cmd = self._pack(index, cmd)
            self._pub.send(new_cmd, new_idx)
            self._cmd.add(self._addr, new_idx, new_cmd)
            if WMD_SEQ_SHOW_FORWARD:
                idx.show('WMDSeq: [snd]', new_idx, self._addr)
        except:
            log_err('WMDSeq: failed to forward')
        finally:
            self._lock.release()
    
    def _proc(self, addr):
        while True:
            time.sleep(1)
            if self._pub:
                break
            log('WMDSeq: waiting ...')
        
        while True:
            index, cmd = self._recv(addr)
            if not index:
                log_err('WMDSeq: failed to process')
                self._stop_sub(addr)
                break
            self._forward(index, cmd)
    
    def update(self, cmd):
        index, orig, track = self._unpack(cmd)
        identity = idx.getid(index)
        addr = self._sub_addr.get(identity)
        if not addr or not self._sub.has_key(addr):
            log_err('WMDSeq: no subscriber')
            return
        if self._sub[addr].idxnxt(index, orig):
            if WMD_SEQ_SHOW_UPDATE:
                idx.show('WMDSeq: [new]', index, addr)
            self._forward(index, orig)
    