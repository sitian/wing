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

sys.path.append('../../lib')
from default import WMD_MIX_PORT
from default import DEBUG
from log import log_err
import net

WMD_MIX_SHOW_RECV = True
WMD_MIX_SHOW_DELIVER = True

class WMDMix(WMDReg):
    def __init__(self, ip, cmd, seq):
        WMDReg.__init__(self, ip, WMD_MIX_PORT)
        self._hdl = WMDHdl()
        self._suspect = {}
        self._cmd = cmd
        self._seq = seq
    
    def _fault(self, addr):
        self._suspect.update({addr:None})
        return self._cmd.mkinactive(self._suspect)
    
    def _live(self, addr):
        if self._suspect.has_key(addr):
            del self._suspect[addr]
        
    def _deliver(self, addr, index, cmd):
        self._cmd.add(addr, index, cmd, update=True)
        while True:
            n, c = self._cmd.pop() #n:index, c:command
            if not n:
                break
            if WMD_MIX_SHOW_DELIVER:
                idx.show('WMDMix: [ + ]', n, self._seq.get_addr(idx.getid(n)))
            self._hdl.proc(c)
    
    def _recv(self, addr):
        try:
            index, cmd = self._sub[addr].recv()
            if WMD_MIX_SHOW_RECV:
                idx.show('WMDMix: [rcv]', index, addr)
            return (index, cmd)
        except:
            log_err('WMDMix: failed to receive, addr=%s' % net.ntoa(addr))
            return (None, None)
    
    def _proc(self, addr):
        while True:
            index, cmd = self._recv(addr)
            if not index:
                break
            self._deliver(addr, index, cmd)
            self._seq.update(cmd)
    
