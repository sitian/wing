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
from sub import getsub
from json import loads

sys.path.append('../../lib')
from log import log_file, log_clean
from log import log, log_err
from default import DEBUG
import net

WMD_QUE_IDX = 0
WMD_QUE_CMD = 1
WMD_QUE_CNT = 2

WMD_QUE_ACTIVE = 1
WMD_QUE_INACTIVE = 2

WMD_QUE_SHOW_CMD = True
WMD_QUE_SHOW_HEAD = True
WMD_QUE_SHOW_CAND = True
WMD_QUE_SHOW_UPDATE = True
WMD_QUE_SHOW_TRACK = False

class WMDQue(object):
    def __init__(self, ip, total, quorum):
        log_clean(self)
        self._idx = {}
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
        self._active = total
        self._quorum = quorum
        self._addr = net.aton(ip)
        
    def _show_cand(self):
        if WMD_QUE_SHOW_CAND:
            if self._cand:
                log(self, 'cand=%s' % str(self._cand))
                
    def _show_cmd(self, addr, cmd):
        if WMD_QUE_SHOW_CMD:
            index = cmd[0]
            log(self, '%s, cnt=%d, hits=%d, cmd=%s' % (net.ntoa(addr), self._cnt[index], self._hits[index], str(cmd)))
    
    def _show_head(self):
        if WMD_QUE_SHOW_HEAD:
            head = []
            for i in self._que:
                if self._que[i]:
                    head.append((net.ntoa(i), self._que[i][0][0]))
            log(self, 'head, %s' % str(head))
    
    def _show_track(self, track):
        if WMD_QUE_SHOW_TRACK:
            log(self, 'track=%s' % str(track))
            log(self, 'rec=%s' % str(self._rec))
            log(self, 'lst=%s' % str(self._lst))
            
    def _show_update(self, addr, index):
        if WMD_QUE_SHOW_UPDATE:
            idx.show(self, '[vis]', index, addr)
                
    def _hit(self, cmd):
        index = cmd[0]
        if not self._cnt.has_key(index):
            log_err(self, 'no index')
            raise Exception('WMDQue: no index')
        hits = self._hits[index]
        if hits >= self._total:
            log_err(self, 'out of range')
            raise Exception("WMDQue: out of range")
        hits += 1
        self._hits[index] = hits
        if hits == self._quorum:
            pos = -1
            length = len(self._cand)
            for i in range(length):
                if idx.compare(index, self._cand[i][0]) > 0:
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
            log_err(self, 'out of range')
            raise Exception('WMDQue: out of range')
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
        count = self._total
        for i in self._stat:
            if self._stat[i] == WMD_QUE_INACTIVE:
                count -= 1
        self._active = count
        
    def mkactive(self, addr):
        if not self._stat.has_key(addr):
            self._stat.update({addr:WMD_QUE_ACTIVE})
        else:
            self._stat[addr] = WMD_QUE_ACTIVE
        self._chkactive()
        return True
    
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
        
        self._chkactive()
        return True
    
    def _append(self, addr, cmd):
        try:
            self._count(cmd)
            n = self._idx.get(addr)
            if not n or idx.compare(cmd[0], n) < 0:            
                self._idx.update({addr:cmd[0]})
                self._hit(cmd)
                self._show_cmd(addr, cmd)
            if not self._que.has_key(addr):
                self._que.update({addr:[cmd]})
            else:
                self._que[addr].append(cmd)
            log_file(self, 'que.' + str(addr), cmd[0])
        except:
            log_err(self, 'failed to append, addr=%s' % net.ntoa(addr))
            raise Exception('WMDQue: failed to append')
    
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
            identity = idx.getid(index)
            src = self._que_addr.get(identity)
            if not src or not self._rec.has_key(src):
                continue
            length = len(self._rec[src])
            for pos in range(length):
                item = self._rec[src][pos]
                n = item[WMD_QUE_IDX]
                if idx.compare(n, index) <= 0:
                    count = len(item[WMD_QUE_CNT])
                    if not item[WMD_QUE_CNT].has_key(addr):
                        item[WMD_QUE_CNT].update({addr:None})
                        count += 1
                    if count < self._quorum and src != self._addr:
                        if self._lst.has_key(src) and idx.compare(n, self._lst[src]) <= 0:
                            count += 1
                    if count == self._quorum:
                        cmd = self._rec[src][pos][WMD_QUE_CMD]
                        if self._chkvis(vis, src, pos) < 0:
                            log_err(self, 'failed to make a command visible, addr=%s' % net.ntoa(src))
                            raise Exception('WMDQue: failed to make a command visible')
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
                log(self, 'failed to check (inactive queue), addr=%s' % net.ntoa(addr))
                return -1
        else:
            self._stat.update({addr:WMD_QUE_ACTIVE})
            
        if index:
            identity = idx.getid(index)
            src = self._que_addr.get(identity)
            if not src:
                self._que_addr.update({identity:addr})
            elif src != addr:
                log_err(self, 'failed to check (invalid address), addr=%s' % net.ntoa(addr))
                return -1
        return 0
    
    def _extract(self, cmd):
        return loads(cmd)
    
    def _is_chosen(self, index):
        identity, sn = idx.extract(index)
        if self._chosen.has_key(identity) and sn <= self._chosen[identity]:
            return True
        else:
            return False
        
    def add(self, addr, index, cmd, update):
        if self._validate(addr, index) < 0:
            log_err(self, 'failed to add, addr=%s' % net.ntoa(addr))
            raise Exception('WMDQue: failed to add')
        
        if self._check(addr, index) < 0:
            return
        
        log_file(self, addr, index)
        n, c, t = self._extract(cmd) #index:n, command:c, track:t
        if self._is_chosen(n):
            return
        
        rec = [index, (n, c), {addr:None}]
        if self._rec.has_key(addr):
            self._rec[addr].append(rec)
        else:
            self._rec.update({addr:[rec]})
        
        self._lst.update({addr:index})
        if update:
            self._update(addr, t)
    
    def _remove_from_que(self, addr, index):
        pos = -1
        chk = False
        remove = False
        smallest = None
        total = len(self._que[addr])
        
        if not total:
            return
        
        if index != self._idx[addr]:
            chk = True
            
        for i in range(total):
            cmd = self._que[addr][i]
            if cmd[0] != index:
                if remove and chk and idx.compare(cmd[0], index) < 0:
                    break
                elif not smallest or idx.compare(cmd[0], smallest) < 0:
                    smallest = cmd[0]
                    if remove:
                        self._hit(cmd)
                        self._show_cmd(addr, cmd)
            else:
                pos = i
                if smallest and idx.compare(smallest, index) < 0:
                    break
                remove = True
        
        if pos >= 0:
            del self._que[addr][pos]
            if not chk:
                if smallest:
                    self._idx[addr] = smallest
                else:
                    del self._idx[addr]
    
    def _remove(self, index):
        #remove from queues
        empty = []
        for addr in self._que:
            self._remove_from_que(addr, index)
            if not len(self._que[addr]):
                empty.append(addr)
        for addr in empty:
            del self._que[addr]
        
        #remove from record lists
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
    
    def track(self):
        ret = []
        for i in self._lst:
            if self._stat[i] == WMD_QUE_ACTIVE:
                ret.append(self._lst[i])
        return ret
    
    def _get_possible_hits(self, index):
        cnt = 0
        for i in self._que:
            if self._stat[i] == WMD_QUE_ACTIVE:
                if not self._idx.has_key(i) or idx.compare(index, self._idx[i]) < 0: 
                    cnt += 1  
        return cnt + self._hits[index]
    
    def _is_safe(self, index):
        if self._hits[index] == self._active:
            return True
        cnt = 0
        for i in self._que:
            if len(self._que[i]) > 0:
                if index == self._que[i][0][0]:
                    cnt += 1
                    if cnt == self._quorum:
                        return True
        self._show_head()
        self._show_cand()
        checked = []
        for i in self._que:
            chk = False
            if len(self._que[i]) > 0 and index != self._que[i][0][0]:
                for cmd in self._que[i]:
                    n = cmd[0]
                    if n == index:
                        chk = True
                        break
                    elif idx.compare(n, index) < 0:
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
        return True
    
    def _choose(self):
        if len(self._cand) > 0:
            cmd = self._cand[0]
            index = cmd[0]
            if self._is_safe(index):
                del self._cand[0]
                self._remove(index)
                identity, sn = idx.extract(index)
                self._chosen.update({identity:sn})
                log_file(self, 'cmd', index)
                return cmd
        return (None, None)
    
    def choose(self):
        return self._choose()
    