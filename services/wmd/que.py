#      que.py
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
from rec import WMDRec

sys.path.append('../../lib')
from log import log_file, log_clean, log, log_err, log_get
import net

WMD_QUE_SHOW_HIT = False
WMD_QUE_SHOW_LEN = False
WMD_QUE_SHOW_MIN = False
WMD_QUE_SHOW_HEAD = False
WMD_QUE_SHOW_CAND = False
WMD_QUE_SHOW_STAT = False

class WMDQue(object):
    def __init__(self, ip, quorum, total):
        log_clean(self)
        self._min = {}
        self._que = {}
        self._cnt = {}
        self._hits = {}
        self._cand = []
        self._total = total
        self._quorum = quorum
        self._rec = WMDRec(ip, quorum, total)
    
    def _show_cand(self):
        if WMD_QUE_SHOW_CAND:
            if self._cand:
                log(self, 'cand=%s' % str(self._cand))
    
    def _show_head(self):
        if WMD_QUE_SHOW_HEAD:
            head = []
            for i in self._que:
                if self._que[i]:
                    head.append((net.ntoa(i), self._que[i][0]))
            if head:
                log(self, 'head=%s' % str(head))
    
    def _show_len(self):
        if WMD_QUE_SHOW_LEN:
            que_len = []
            for i in self._que:
                if self._que[i]:
                    que_len.append((net.ntoa(i), len(self._que[i])))
            if que_len:
                log(self, 'len=%s' % str(que_len))
    
    def _show_hit(self, addr, index):
        if WMD_QUE_SHOW_HIT:
            log(self, '%s, cnt=%d, hits=%d, index=%s' % (net.ntoa(addr), self._cnt[index], self._hits[index], index))
    
    def _show_min(self):
        if WMD_QUE_SHOW_MIN:
            que_min = []
            for i in self._min:
                que_min.append((net.ntoa(i), self._min[i]))
            if que_min:
                log(self, 'min=%s' % str(que_min))
    
    def _show_stat(self):
        if WMD_QUE_SHOW_STAT:
            self._show_len()
            self._show_min()
            self._show_head()
            self._show_cand()
    
    def _log(self, s, que=None):
        name = 'que'
        if que:
            name += '.' + str(que)
        log_file(self, name, str(s))
             
    def _hit(self, index):
        if not self._cnt.has_key(index):
            log_err(self, 'failed to hit (no index)')
            raise Exception(log_get(self, 'no index'))
        hits = self._hits[index]
        if hits >= self._total:
            log_err(self, 'failed to hit (out of range)')
            raise Exception(log_get(self, 'out of range'))
        hits += 1
        self._hits[index] = hits
        if hits == self._quorum:
            pos = -1
            length = len(self._cand)
            for i in range(length):
                if idx.idxcmp(index, self._cand[i]) > 0:
                    pos = i
                    break
            if pos >= 0:
                self._cand.insert(pos, index)
            else:
                self._cand.append(index)
    
    def _count(self, index):
        if not self._cnt.has_key(index):
            self._cnt.update({index:0})
            self._hits.update({index:0})
        total = self._cnt[index]
        if total >= self._total:
            log_err(self, 'failed to count (out of range)')
            raise Exception(log_get(self, 'out of range'))
        self._cnt[index] = total + 1
    
    def _append(self, addr, index):
        try:
            self._count(index)
            n = self._min.get(addr)
            if not n or idx.idxcmp(index, n) < 0:            
                self._min.update({addr:index})
                self._hit(index)
                self._show_hit(addr, index)
            if not self._que.has_key(addr):
                self._que.update({addr:[index]})
            else:
                self._que[addr].append(index)
            self._log(index, addr)
        except:
            log_err(self, 'failed to append, addr=%s' % net.ntoa(addr))
            raise Exception(log_get(self, 'failed to append'))
    
    def add(self, addr, index, cmd):
        cmds = self._rec.add(addr, index, cmd)
        if cmds:
            for item in cmds:
                self._append(*item)
    
    def _remove_from_que(self, addr, index):
        pos = -1
        val = None
        diff = False
        remove = False
        total = len(self._que[addr])
        
        if not total:
            return
        
        if index != self._min[addr]:
            diff = True
            
        for i in range(total):
            n = self._que[addr][i]
            if n != index:
                if remove and diff and idx.idxcmp(n, index) < 0:
                    break
                if not val or idx.idxcmp(n, val) < 0:
                    val = n
                    if remove:
                        self._hit(n)
                        self._show_hit(addr, n)
            else:
                pos = i
                if val and idx.idxcmp(val, index) < 0:
                    break
                remove = True
        
        if pos >= 0:
            del self._que[addr][pos]
            if not diff:
                if val:
                    self._min[addr] = val
                else:
                    del self._min[addr]
    
    def _remove(self, index):
        for addr in self._que:
            self._remove_from_que(addr, index)
        self._rec.recycle(index)
        del self._hits[index]
        del self._cnt[index]
    
    def _get_possible_hits(self, index):
        cnt = 0
        for i in self._que:
            if self._rec.is_active(i):
                if not self._min.has_key(i) or idx.idxcmp(index, self._min[i]) < 0: 
                    cnt += 1  
        return cnt + self._hits[index] + self._rec.chkidle()
    
    def _is_safe(self, index):
        if self._hits[index] == self._total:
            return True
        
        count = 0
        for i in self._que:
            if len(self._que[i]) > 0:
                if index == self._que[i][0]:
                    count += 1
                    if count == self._quorum:
                        return True
        
        checked = []
        self._show_stat()
        for i in self._que:
            chk = False
            if len(self._que[i]) > 0: 
                if index != self._que[i][0]:
                    for n in self._que[i]:
                        if n == index:
                            chk = True
                            break
                        elif idx.idxcmp(n, index) < 0:
                            break
            if chk:
                for n in self._que[i]:
                    if n == index:
                        break
                    if n in checked:
                        continue
                    if self._get_possible_hits(n) >= self._quorum:
                        return False
                    checked.append(n)
                count += 1
                if count == self._quorum:
                    return True
        return False
    
    def _pop(self):
        if len(self._cand) > 0:
            index = self._cand[0]
            if self._is_safe(index):
                cmd = self._rec.get_cmd(index)
                if not cmd:
                    log_err(self, 'failed to get command')
                    raise Exception(log_get(self, 'failed to get command'))
                del self._cand[0]
                self._remove(index)
                self._log(index)
                return (index, cmd)
        return (None, None)
    
    def pop(self):
        return self._pop()
    
    def chklst(self, addr):
        return self._rec.chklst(addr)
    
    def chkslow(self):
        return self._rec.chkslow()
    
    def chkinactive(self, addr):
        return self._rec.chkinactive(addr)
    
    def collect(self, addr, start, end):
        return self._rec.collect(addr, start, end)
    
    def mkactive(self, addr):
        self._rec.mkactive(addr)
    
    def mkinactive(self, addr):
        self._rec.mkinactive(addr)
        
    def track(self):
        return self._rec.track()
    