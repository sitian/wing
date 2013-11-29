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
import seq
import idx
from sub import getsub

sys.path.append('../../lib')
from log import log_file, log_clean, log, log_err, log_get
from default import DEBUG
import net

WMD_QUE_IDX = 0
WMD_QUE_CMD = 1
WMD_QUE_CNT = 2

WMD_QUE_ACTIVE = 1
WMD_QUE_INACTIVE = 2

WMD_QUE_SHOW_LEN = True
WMD_QUE_SHOW_MIN = True
WMD_QUE_SHOW_HEAD = True
WMD_QUE_SHOW_CAND = True
WMD_QUE_SHOW_HIT = False
WMD_QUE_SHOW_STAT = False
WMD_QUE_SHOW_TRACK = False
WMD_QUE_SHOW_UPDATE = False

class WMDQue(object):
    def __init__(self, ip, total, quorum):
        log_clean(self)
        self._min = {}
        self._rec = {}
        self._que = {}
        self._cnt = {}
        self._lst = {}
        self._hits = {}
        self._stat = {}
        self._cand = []
        self._chosen = {}
        self._que_addr = {}
        self._total = total
        self._available = total
        self._addr = net.aton(ip)
        self._quorum = quorum
        self._active = 0
        
    def _show_cand(self):
        if WMD_QUE_SHOW_CAND:
            if self._cand:
                log(self, 'cand=%s' % str(self._cand))
    
    def _show_head(self):
        if WMD_QUE_SHOW_HEAD:
            head = []
            for i in self._que:
                if self._que[i]:
                    head.append((net.ntoa(i), self._que[i][0][0]))
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
    
    def _show_hit(self, addr, cmd):
        if WMD_QUE_SHOW_HIT:
            index = cmd[0]
            log(self, '%s, cnt=%d, hits=%d, index=%s' % (net.ntoa(addr), self._cnt[index], self._hits[index], index))
    
    def _show_track(self, track):
        if WMD_QUE_SHOW_TRACK:
            log(self, 'track=%s' % str(track))
            log(self, 'rec=%s' % str(self._rec))
            log(self, 'lst=%s' % str(self._lst))
            
    def _show_update(self, addr, index):
        if WMD_QUE_SHOW_UPDATE:
            log(self, '[vis] %s, index=%s' % (net.ntoa(addr), index))
    
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
              
    def _hit(self, cmd):
        index = cmd[0]
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
                if idx.idxcmp(index, self._cand[i][0]) > 0:
                    pos = i
                    break
            if pos >= 0:
                self._cand.insert(pos, cmd)
            else:
                self._cand.append(cmd)
    
    def _count(self, cmd):
        index = cmd[0]
        if not self._cnt.has_key(index):
            self._cnt.update({index:0})
            self._hits.update({index:0})
        total = self._cnt[index]
        if total >= self._total:
            log_err(self, 'failed to count (out of range)')
            raise Exception(log_get(self, 'out of range'))
        self._cnt[index] = total + 1
        
    def _validate(self, addr, index):
        if not DEBUG or not self._rec.has_key(addr):
            return 0
        que = self._rec[addr]
        length = len(que)
        for i in range(length):
            if que[i][WMD_QUE_IDX] == index:
                return -1
        return 0
    
    def _chkactive(self):
        count = 0
        total = self._total
        for i in self._stat:
            if self._stat[i] == WMD_QUE_INACTIVE:
                total -= 1
            elif self._stat[i] == WMD_QUE_ACTIVE:
                count += 1
        self._active = count
        self._available = total
        if self._active > self._available:
            log_err(self, 'failed to check active queues')
            return False
        else:
            return True
        
    def mkactive(self, addr):
        if not self._stat.has_key(addr):
            if self._active + 1 > self._available:
                log_err(self, 'failed to make a queue active (out of range)')
                return False
            self._stat.update({addr:WMD_QUE_ACTIVE})
        else:
            self._stat[addr] = WMD_QUE_ACTIVE
        return self._chkactive()
    
    def mkinactive(self, rep, suspect): #rep: reporter, suspect: suspect list
        if len(suspect) >= self._quorum:
            return False
        
        chk = False
        for i in suspect:
            if not self._stat.has_key(i) or self._stat[i] != WMD_QUE_INACTIVE:
                chk = True
                break
        
        if not chk:
            return True
        
        suspect_nodes = suspect
        for i in self._stat:
            if self._stat[i] == WMD_QUE_INACTIVE:
                suspect_nodes.update({i:None})
        
        nodes = {}
        sub = getsub(net.ntoa(self._addr))
        for item in sub:
            addr = net.aton(item[0])
            if addr not in suspect_nodes:
                nodes.update({addr:None})
        
        leader = rep.get_leader(nodes)
        if leader == self._addr:
            ret = rep.coordinate(suspect_nodes, len(nodes))
        else:
            ret = rep.report(leader, suspect_nodes)
            
        if ret < 0:
            return False
        
        for i in suspect_nodes:
            self._stat.update({i:WMD_QUE_INACTIVE})
        
        return self._chkactive()
    
    def _append(self, addr, cmd):
        try:
            self._count(cmd)
            n = self._min.get(addr)
            if not n or idx.idxcmp(cmd[0], n) < 0:            
                self._min.update({addr:cmd[0]})
                self._hit(cmd)
                self._show_hit(addr, cmd)
            if not self._que.has_key(addr):
                self._que.update({addr:[cmd]})
            else:
                self._que[addr].append(cmd)
            log_file(self, 'que.' + str(addr), cmd[0])
        except:
            log_err(self, 'failed to append, addr=%s' % net.ntoa(addr))
            raise Exception(log_get(self, 'failed to append'))
    
    def _chkvis(self, vis, addr, pos):
        if not DEBUG:
            return 0
        if not vis.has_key(addr):
            if pos:
                return -1
        elif vis[addr] + 1 != pos:
            return -1
        return 0
    
    def _update(self, addr, track):
        vis = {}
        self._show_track(track)
        for index in track:
            identity = idx.idxid(index)
            src = self._que_addr.get(identity)
            if not src or not self._rec.has_key(src):
                continue
            length = len(self._rec[src])
            for pos in range(length):
                item = self._rec[src][pos]
                n = item[WMD_QUE_IDX]
                if idx.idxcmp(n, index) <= 0:
                    count = len(item[WMD_QUE_CNT])
                    if addr not in item[WMD_QUE_CNT]:
                        item[WMD_QUE_CNT].append(addr)
                        count += 1
                    if count < self._quorum:
                        if self._lst.has_key(src) and idx.idxcmp(n, self._lst[src]) <= 0:
                            count += 1
                    elif count == self._quorum:
                        cmd = self._rec[src][pos][WMD_QUE_CMD]
                        if self._chkvis(vis, src, pos) < 0:
                            log_err(self, 'failed to update, addr=%s' % net.ntoa(src))
                            raise Exception(log_get(self, 'failed to update'))
                        vis.update({src:pos})
                        self._append(src, cmd)
                        self._show_update(src, n)
                else:
                    break
                
        for src in vis:
            total = vis[src] + 1
            for i in range(total):
                del self._rec[src][0]
            if not self._rec[src]:
                del self._rec[src]
    
    def _check(self, addr, index=None):
        if self._stat.has_key(addr):
            if self._stat[addr] == WMD_QUE_INACTIVE:
                log(self, 'reject to add a cmd to an inactive queue, addr=%s' % net.ntoa(addr))
                return -1
        else:
            self.mkactive(addr)
            
        if index:
            identity = idx.idxid(index)
            src = self._que_addr.get(identity)
            if not src:
                self._que_addr.update({identity:addr})
            elif src != addr:
                log_err(self, 'failed to check (invalid address), addr=%s' % net.ntoa(addr))
                return -1
        return 0
    
    def _is_chosen(self, index):
        identity, sn = idx.idxdec(index)
        if self._chosen.has_key(identity) and sn <= self._chosen[identity]:
            return True
        else:
            return False
        
    def add(self, addr, index, cmd, update):
        if self._validate(addr, index) < 0:
            log_err(self, 'failed to add, addr=%s' % net.ntoa(addr))
            raise Exception(log_get(self, 'failed to add'))
        
        if self._check(addr, index) < 0:
            return
        
        log_file(self, addr, index)
        track, orig_index, orig_cmd = seq.unpack(cmd) 
        if self._is_chosen(orig_index):
            return
        
        rec = [index, (orig_index, orig_cmd), [addr]]
        if self._rec.has_key(addr):
            if index not in (item[WMD_QUE_IDX] for item in self._rec[addr]):
                self._rec[addr].append(rec)
        else:
            self._rec.update({addr:[rec]})
            
        self._lst.update({addr:index})
        if update:
            self._update(addr, track)
    
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
            cmd = self._que[addr][i]
            n = cmd[0]
            if n != index:
                if remove and diff and idx.idxcmp(n, index) < 0:
                    break
                if not val or idx.idxcmp(n, val) < 0:
                    val = n
                    if remove:
                        self._hit(cmd)
                        self._show_hit(addr, cmd)
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
        #remove from queues
        empty = []
        for addr in self._que:
            self._remove_from_que(addr, index)
            if not len(self._que[addr]):
                empty.append(addr)
        for addr in empty:
            del self._que[addr]
        
        # remove from record lists
        empty = []
        for addr in self._rec:
            length = len(self._rec[addr])
            for i in range(length):
                cmd = self._rec[addr][i][WMD_QUE_CMD]
                if cmd[0] == index:
                    del self._rec[addr][i]
                    break
            if not len(self._rec[addr]):
                empty.append(addr)
        for addr in empty:
            del self._rec[addr]
        del self._hits[index]
        del self._cnt[index]
        self._show_stat()
    
    def track(self):
        ret = []
        for i in self._lst:
            if self._stat[i] == WMD_QUE_ACTIVE:
                ret.append(self._lst[i])
        return ret
    
    def track_peers(self):
        ret = []
        for i in self._lst:
            if self._stat[i] == WMD_QUE_ACTIVE and i != self._addr:
                ret.append(self._lst[i])
        return ret
    
    def _get_possible_hits(self, index):
        cnt = 0
        for i in self._stat:
            if self._stat[i] == WMD_QUE_ACTIVE:
                if not self._min.has_key(i) or idx.idxcmp(index, self._min[i]) < 0: 
                    cnt += 1  
        return cnt + self._hits[index] + self._available - self._active
    
    def _is_safe(self, index):
        if self._hits[index] == self._available:
            return True
        
        count = 0
        for i in self._que:
            if len(self._que[i]) > 0:
                if index == self._que[i][0][0]:
                    count += 1
                    if count == self._quorum:
                        return True
        
        count = 0
        checked = []
        self._show_stat()
        for i in self._que:
            chk = False
            if len(self._que[i]) > 0: 
                if index == self._que[i][0][0]:
                    count += 1
                    if count == self._quorum:
                        return True
                else:
                    for cmd in self._que[i]:
                        n = cmd[0]
                        if n == index:
                            chk = True
                            break
                        elif idx.idxcmp(n, index) < 0:
                            break
            if chk:
                for cmd in self._que[i]:
                    n = cmd[0]
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
    
    def chkoff(self):
        init  = False
        sn_min = None
        sn_max = None
        for i in self._lst:
            if self._stat[i] == WMD_QUE_ACTIVE:
                sn = idx.idxsn(self._lst[i])
                if not init:
                    sn_min = sn
                    sn_max = sn
                    init = True
                else:
                    if sn < sn_min:
                        sn_min = sn
                    elif sn > sn_max:
                        sn_max = sn
        if init:
            return sn_max - sn_min
        else:
            return 0
        
    def _choose(self):
        if len(self._cand) > 0:
            cmd = self._cand[0]
            index = cmd[0]
            if self._is_safe(index):
                del self._cand[0]
                self._remove(index)
                identity, sn = idx.idxdec(index)
                self._chosen.update({identity:sn})
                log_file(self, 'cmd', index)
                return cmd
        return (None, None)
    
    def choose(self):
        return self._choose()
    