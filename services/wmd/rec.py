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
WMD_REC_ORIG_IDX = 2

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
        self._removed = {}
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
    
    def _update(self, addr, index, track):
        vis = {}
        res = []
        if addr == self._addr:
            return res
        
        track.append(index)
        self._show_update(addr, index, track)
        
        for index in track:
            identity = idx.idxid(index)
            src = self._peers.get(identity)
            if not src or not self._rec.has_key(src):
                continue
            length = len(self._rec[src])
            for pos in range(length):
                item = self._rec[src][pos]
                count = len(item[WMD_REC_ADDR]) + 1
                if src != self._addr:
                    count += 1
                if count < self._quorum:
                    if addr != src and addr not in item[WMD_REC_ADDR] and idx.idxcmp(item[WMD_REC_IDX], index) <= 0:
                        item[WMD_REC_ADDR].append(addr)
                        count += 1
                if  count >= self._quorum:
                    orig_index = self._rec[src][pos][WMD_REC_ORIG_IDX]
                    vis.update({src:pos})
                    res.append((src, orig_index))
                    self._show_vis(src, orig_index)
                else:
                    break
        
        for i in vis:
            total = vis[i] + 1
            for j in range(total):
                del self._rec[i][0]
            if not self._rec[i]:
                del self._rec[i]
        return res
    
    def track(self):
        ret = []
        for i in self._lst:
            if self._stat[i] == WMD_REC_ACTIVE and i != self._addr:
                ret.append(self._lst[i])
        return ret
    
    def _is_removed(self, identity, sn):
        if self._removed.has_key(identity) and sn <= self._removed[identity]:
            return True
        else:
            return False
        
    def _is_recycled(self, identity, sn):
        if self._recycled.has_key(identity) and sn <= self._recycled[identity]:
            return True
        else:
            return False
    
    def is_active(self, addr):
        stat = self._stat.get(addr)
        if not stat or stat == WMD_REC_INACTIVE:
            return False
        else:
            return True
    
    def mkactive(self, addr):
        if not self._stat.has_key(addr):
            if len(self._active) + 1 > self._available:
                log_err(self, 'mkactive failed')
                raise Exception(log_get(self, 'mkactive failed'))
        self._stat.update({addr:WMD_REC_ACTIVE})
        self._active.update({addr:None})
        count = self._total
        for i in self._stat:
            if self._stat[i] == WMD_REC_INACTIVE:
                count -= 1
        self._available = count
    
    def chkinactive(self, suspect):
        if len(suspect) >= self._quorum:
            return None
        chk = False
        for i in suspect:
            if not self._stat.has_key(i) or self._stat[i] != WMD_REC_INACTIVE:
                chk = True
                break
        if not chk:
            return None    
        suspect_list = suspect
        for i in self._stat:
            if self._stat[i] == WMD_REC_INACTIVE:
                suspect_list.update({i:None})
        return suspect_list
         
    def mkinactive(self, suspect):
        for i in suspect:
            self._stat.update({i:WMD_REC_INACTIVE})
            if self._active.has_key(i):
                del self._active[i]
        count = self._total
        for i in self._stat:
            if self._stat[i] == WMD_REC_INACTIVE:
                count -= 1
        self._available = count
    
    def _try_to_remove(self, index, identity, sn):
        if len(set(self._cmds[index][WMD_REC_ADDR]) & set(self._active)) == self._available and self._available >= self._quorum:
            self._removed.update({identity:sn})
            del self._cmds[index]
    
    def _check(self, addr, index):
        if self._stat.has_key(addr):
            if not self.is_active(addr):
                log(self, 'received a command from an inactive node, addr=%s' % net.ntoa(addr))
                return -1
        else:
            self.mkactive(addr)
            
        identity = idx.idxid(index)
        src = self._peers.get(identity)
        if not src:
            self._peers.update({identity:addr})
        elif src != addr:
            log_err(self, 'invalid identity, addr=%s' % net.ntoa(addr))
            return -1
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
        else:
            return None
        
    def _new_rec(self, index, orig_index):
        return [index, [], orig_index]
    
    def add(self, addr, index, cmd):
        res = []
        if self._check(addr, index) < 0:
            log(self, 'failed to add, addr=%s' % net.ntoa(addr))
            return res
        
        track, orig_index, orig_cmd = seq.unpack(cmd)
        identity, sn = idx.idxdec(orig_index)
        if self._is_removed(identity, sn):
            return res
        
        recycle = self._is_recycled(identity, sn) 
        if not recycle:
            self._lst.update({addr:index})
            if self._rec.has_key(addr):
                if index not in (item[WMD_REC_IDX] for item in self._rec[addr]):
                    self._rec[addr].append(self._new_rec(index, orig_index))
            else:
                self._rec.update({addr:[self._new_rec(index, orig_index)]})
            res = self._update(addr, index, track)
        
        self._chkcmd(addr, orig_index, orig_cmd)
        if recycle:
            self._try_to_remove(orig_index, identity, sn)
        return res
    
    def _recycle(self, index):
        identity, sn = idx.idxdec(index)
        self._recycled.update({identity:sn})
        self._try_to_remove(index, identity, sn)
    
    def remove(self, index):
        for addr in self._rec:
            length = len(self._rec[addr])
            for i in range(length):
                orig_index = self._rec[addr][i][WMD_REC_ORIG_IDX]
                if orig_index == index:
                    del self._rec[addr][i]
                    break
        self._recycle(index)
    
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
        else:
            return None
    
    def chkavailable(self):
        return self._available
    
    def chkidle(self):
        return self._available - len(self._active)
    