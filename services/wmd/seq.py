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
from sub import getsub
from json import dumps, loads
from reg import WMDReg
from pub import WMDPub

sys.path.append('../../lib')
from default import WMD_REG_PORT, WMD_MIX_PORT
from default import WMD_SEQ_PORT, getdef
from log import log, log_err
import net

WMD_SEQ_SHOW_RECV = True
WMD_SEQ_SHOW_FORWARD = True

class WMDSeq(WMDReg):
    def __init__(self, ip, cmd):
        WMDReg.__init__(self, ip, WMD_REG_PORT)
        self._addr = net.aton(ip)
        self._pub = None
        self._cmd = cmd
    
    def _show_recv(self, index):
        if WMD_SEQ_SHOW_RECV:
            idx.show(self, '[rcv]', index, self.get_addr(idx.getid(index)))
    
    def _show_forward(self, index):
        if WMD_SEQ_SHOW_FORWARD:
            idx.show(self, '[snd]', index, self._addr)
            
    def _register(self):
        ip = net.ntoa(self._addr)
        sub = getsub(ip, WMD_MIX_PORT)
        if not sub:
            sys.exit(0)
        self._pub = WMDPub(ip, WMD_SEQ_PORT)
        self._pub.register(sub, heartbeat=True)
    
    def _recv(self, addr):
        index, cmd = self._sub[addr].recv()
        self._show_recv(index)
        return (index, cmd)
    
    def _pack(self, index, cmd):
        return dumps((index, cmd, self._cmd.track()))
    
    def _unpack(self, cmd):
        return loads(cmd)
    
    def _forward(self, index, cmd):
        new_idx = self._pub.idxget()
        new_cmd = self._pack(index, cmd)
        self._pub.send(new_cmd, new_idx)
        self._cmd.add(self._addr, new_idx, new_cmd)
        self._show_forward(new_idx)
            
    def _proc(self, addr):
        wait = False
        while not self._pub:
            if not wait:
                log(self, 'wait')
                wait = True
            time.sleep(1)
        try:
            while True:
                try:
                    index, cmd = self._recv(addr)
                except:
                    log_err(self, 'failed to receive')
                    self._stop_sub(addr)
                    break
                self._forward(index, cmd)
        except:
            log_err(self, 'failed to process')
    
    def update(self, cmd):
        index, orig, track = self._unpack(cmd)
        identity = idx.getid(index)
        addr = self._sub_addr.get(identity)
        if not addr or not self._sub.has_key(addr):
            log_err(self, 'no subscriber')
            return
        if self._sub[addr].idxnxt(index, orig):
            self._forward(index, orig)
        