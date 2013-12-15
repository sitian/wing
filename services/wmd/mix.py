#      mix.py
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
from reg import WMDReg
from hdl import WMDHdl
from threading import Lock

sys.path.append('../../lib')
from default import WMD_MIX_PORT
from log import log, log_err, log_get
import net

WMD_MIX_SHOW_POP = True
WMD_MIX_SHOW_RECV = False

class WMDMix(WMDReg):
    def __init__(self, ip, cmd, seq):
        WMDReg.__init__(self, ip, WMD_MIX_PORT)
        self._hdl = WMDHdl()
        self._lock = Lock()
        self._suspect = {}
        self._cmd = cmd
        self._seq = seq
    
    def _show_recv(self, index):
        if WMD_MIX_SHOW_RECV:
            log(self, '[rcv] index=%s' % index)
    
    def _show_pop(self, index):
        if WMD_MIX_SHOW_POP:
            identity, sn = idx.idxdec(index)
            addr = self._seq.get_addr(identity)
            log(self, '[ + ] %s, sn=%d' % (net.ntoa(addr), sn))
        
    def _fault(self, addr):
        self._suspect.update({addr:None})
        self._cmd.mkinactive(self._suspect)
    
    def _live(self, addr):
        if self._suspect.has_key(addr):
            del self._suspect[addr]
        
    def _deliver(self, addr, index, cmd):
        self._cmd.add(addr, index, cmd)
        self._seq.update(cmd)
        
    def _pop(self):
        self._lock.acquire()
        try:
            while True:
                orig_index, orig_cmd = self._cmd.pop()
                if not orig_index:
                    break
                self._hdl.proc(orig_cmd)
                self._show_pop(orig_index)
        except:
            log_err(self, 'failed to pop')
            raise Exception(log_get(self, 'failed to pop'))
        finally:
            self._lock.release()
    
    def _recv(self, addr):
        try:
            index, cmd = self._sub[addr].recv()
            self._show_recv(index)
            return (index, cmd)
        except:
            log_err(self, 'failed to receive, addr=%s' % net.ntoa(addr))
            raise Exception(log_get(self, 'failed to receive'))
    
    def _proc(self, addr):
        while True:
            index, cmd = self._recv(addr)
            self._deliver(addr, index, cmd)
            self._pop()
    