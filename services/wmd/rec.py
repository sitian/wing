#      rec.py
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

import idx
import sys
import seq

sys.path.append('../../lib')
from log import log, log_err, log_get
import net

WMD_REC_CMD = 0
WMD_REC_IDX = 0
WMD_REC_ADDR = 1
WMD_REC_ORIG = 2
WMD_REC_VIS = 3

WMD_REC_ACTIVE = 1
WMD_REC_INACTIVE = 2

WMD_REC_OFFSET = 4

WMD_REC_SHOW_VIS = False
WMD_REC_SHOW_UPDATE = False

class WMDRec(object):
    def __init__(self, ip, quorum, total):
        self._rec = {}
        self._lst = {}
        self._stat = {}
        self._cmds = {}
        self._peers = {}
        self._active = {}
        self._recycled = {}
        self._total = total
        self._quorum = quorum
        self._available = total
        self._addr = net.aton(ip)
    
    def _show_update(self, addr, index, track):
        if WMD_REC_SHOW_UPDATE:
            log(self, 'rec=%s' % str(self._rec))
            log(self, 'lst=%s' % str(self._lst))
            log(self, 'addr=%s, index=%s, track=%s' % (str(addr), str(index), str(track)))
    
    def _show_vis(self, addr, index):
        if WMD_REC_SHOW_VIS:
            log(self, '[vis] %s, index=%s' % (net.ntoa(addr), index))
    
    def _is_recycled(self, index):
        identity, sn = idx.idxdec(index)
        if self._recycled.has_key(identity) and sn <= self._recycled[identity]:
            return True
        else:
            return False
    
    def _update(self, addr, index, track):
        if addr == self._addr:
            return
        
        track.append(index)
        self._show_update(addr, index, track)
        
        cmds = []
        for index in track:
            identity = idx.idxid(index)
            src = self._peers.get(identity)
            if not src or not self._rec.has_key(src):
                continue
            length = len(self._rec[src])
            for pos in range(length):
                item = self._rec[src][pos]
                if item[WMD_REC_VIS]:
                    continue
                count = len(item[WMD_REC_ADDR]) + 1
                if src != self._addr:
                    count += 1
                if count < self._quorum:
                    if addr != src and addr not in item[WMD_REC_ADDR] and idx.idxcmp(item[WMD_REC_IDX], index) <= 0:
                        count += 1
                        if count < self._quorum:
                            item[WMD_REC_ADDR].append(addr)
                if count >= self._quorum:
                    orig = item[WMD_REC_ORIG]
                    item[WMD_REC_VIS] = True
                    self._show_vis(src, orig)
                    if not self._is_recycled(orig):
                        cmds.append((src, orig))
                else:
                    break
        if cmds:
            return cmds
    
    def is_active(self, addr):
        stat = self._stat.get(addr)
        if not stat or stat == WMD_REC_INACTIVE:
            return False
        else:
            return True
    
    def mkactive(self, addr):
        if not self._stat.has_key(addr):
            if len(self._active) + 1 > self._available:
                log_err(self, 'failed to make active')
                raise Exception(log_get(self, 'failed to make active'))
        self._stat.update({addr:WMD_REC_ACTIVE})
        self._active.update({addr:None})
        count = self._total
        for i in self._stat:
            if self._stat[i] == WMD_REC_INACTIVE:
                count -= 1
        self._available = count
    
    def chkinactive(self, addr):    
        if self._available <= self._quorum or (self._stat.has_key(addr) and self._stat[addr] == WMD_REC_INACTIVE):
            return 
        suspect = [addr]
        for i in self._stat:
            if self._stat[i] == WMD_REC_INACTIVE:
                suspect.append(i)
        return suspect
         
    def mkinactive(self, addr):
        self._stat.update({addr:WMD_REC_INACTIVE})
        if self._active.has_key(addr):
            del self._active[addr]
            self._available -= 1
    
    def _remove(self, index):
        for addr in self._rec:
            length = len(self._rec[addr])
            for i in range(length): 
                if index == self._rec[addr][i][WMD_REC_ORIG]:
                    del self._rec[addr][i]
                    break
        del self._cmds[index]
                
    def _try_to_remove(self, index):
        if len(set(self._cmds[index][WMD_REC_ADDR]) & set(self._active)) == self._available and self._available >= self._quorum:
            self._remove(index)
    
    def _check(self, addr, index):
        if self._stat.has_key(addr):
            if not self.is_active(addr):
                log(self, 'failed to check (invalid addr)')
                return -1
        else:
            self.mkactive(addr)
            
        identity = idx.idxid(index)
        src = self._peers.get(identity)
        if not src:
            self._peers.update({identity:addr})
        elif src != addr:
            log_err(self, 'failed to check (invalid identity)')
            return -1
        
        self._lst.update({addr:index})
        return 0
    
    def _chkcmd(self, addr, index, cmd):
        if self._cmds.has_key(index):
            if addr not in self._cmds[index][WMD_REC_ADDR]:
                self._cmds[index][WMD_REC_ADDR].append(addr)
        else:
            self._cmds.update({index:[cmd, [addr]]})
    
    def get_cmd(self, index):
        cmd = self._cmds.get(index)
        if cmd:
            return cmd[WMD_REC_CMD]
        
    def _new_rec(self, index, orig):
        return [index, [], orig, False]
    
    def add(self, addr, index, cmd):
        if self._check(addr, index) < 0:
            return
        
        track, orig, command = seq.unpack(cmd)
        recycled = self._is_recycled(orig)
        if not recycled:
            if self._rec.has_key(addr):
                if index not in (item[WMD_REC_IDX] for item in self._rec[addr]):
                    self._rec[addr].append(self._new_rec(index, orig))
            else:
                self._rec.update({addr:[self._new_rec(index, orig)]})
        
        self._chkcmd(addr, orig, command)
        
        if not recycled:
            return self._update(addr, index, track)
        else:
            self._try_to_remove(orig)
    
    def recycle(self, index):
        identity, sn = idx.idxdec(index)
        self._recycled.update({identity:sn})
        self._try_to_remove(index)
    
    def chklst(self, addr):
        return self._lst.get(addr)
    
    def chkslow(self):
        chk = False
        addr = None
        sn_min = None
        sn_max = None
        for i in self._lst:
            if self._stat[i] == WMD_REC_ACTIVE:
                sn = idx.idxsn(self._lst[i])
                if not chk:
                    sn_min = sn
                    sn_max = sn
                    chk = True
                else:
                    if sn < sn_min:
                        sn_min = sn
                        addr = i
                    elif sn > sn_max:
                        sn_max = sn
        if chk and sn_max - sn_min >= WMD_REC_OFFSET:
            return addr
    
    def chkavailable(self):
        return self._available
    
    def chkidle(self):
        return self._available - len(self._active)
    
    def _check_range(self, addr, start, end):
        if not self._rec.has_key(addr) or idx.idxcmp(start, end) > 0:
            log_err(self, 'invalid range')
            return -1
        
        if start not in (item[WMD_REC_IDX] for item in self._rec[addr]):
            log_err(self, 'invalid range (failed to find start)')
            return -1
        
        if end not in (item[WMD_REC_IDX] for item in self._rec[addr]):
            log_err(self, 'invalid range (failed to find end)')
            return -1
        
        return 0
        
    def collect(self, addr, start, end):
        if self._check_range(addr, start, end) < 0:
            log_err(self, 'failed to collect')
            return
        
        res = 0
        cmds = []
        prev = None
        for item in self._rec[addr]:
            index = item[WMD_REC_IDX]
            if not prev and start:
                res = idx.idxcmp(index, start)
            if res >= 0:
                if prev and idx.idxinc(prev) != index:
                    log_err(self, 'failed to collect (invalid index)')
                    break
                orig = item[WMD_REC_ORIG]
                command = self.get_cmd(orig)
                if not command:
                    log_err(self, 'failed to collect (no command)')
                    break
                prev = index
                cmds.append((orig, command))
                if index == end:
                    break
        
        if prev == end:
            return cmds
    
    def track(self):
        ret = []
        for i in self._lst:
            if self._stat[i] == WMD_REC_ACTIVE and i != self._addr:
                ret.append(self._lst[i])
        return ret
    