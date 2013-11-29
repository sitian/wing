#      cmd.py
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

from rep import WMDRep
from que import WMDQue
from threading import Lock
from threading import Event

WQUE_CMD_IDX_OFFSET = 10
WQUE_CMD_CHK_INTERVAL = 10
WQUE_CMD_WAIT_TIME = 1 # sec

sys.path.append('../../lib')
from log import log, log_err, log_get
from default import getdef
import net

class WMDCmd(WMDRep):
    def __init__(self, ip):
        WMDRep.__init__(self, ip)
        self._mds_max = getdef('MDS_MAX')
        self._quorum = int(self._mds_max / 2) + 1
        self._que = WMDQue(ip, self._mds_max, self._quorum)
        self._event = Event()
        self._lock = Lock()
        self._event.set()
        self._index = {}
        self._head = []
        self._count = 0
    
    def _check(self, index):
        identity, sn = idx.idxdec(index)
        if self._index.has_key(identity) and sn < self._index[identity]:
            return -1
        else:
            self._index.update({identity:sn})
            return 0
    
    def mkactive(self, addr):
        self._lock.acquire()
        try:
            return self._que.mkactive(addr)
        except:
            log_err(self, 'failed to make active, addr=%s' % net.ntoa(addr))
            raise Exception(log_get(self, 'failed to make active'))
        finally:
            self._lock.release()
    
    def mkinactive(self, suspect):
        self._lock.acquire()
        try:
            return self._que.mkinactive(self, suspect)
        except:
            log_err(self, 'failed to make inactive, suspect=%s' % str(suspect))
            raise Exception(log_get(self, 'failed to make inactive'))
        finally:
            self._lock.release()
    
    def add(self, addr, index, cmd, update=False):
        self._lock.acquire()
        try:
            if self._check(index) < 0:
                log(self, 'cannot add (this is a duplicated command)')
                return
            self._que.add(addr, index, cmd, update)
        except:
            log_err(self, 'failed to add, %s' % net.ntoa(addr))
            raise Exception(log_get(self, 'failed to add'))
        finally:
            self._lock.release()
    
    def track(self):
        self._lock.acquire()
        try:
            return self._que.track()
        except:
            log_err(self, 'failed to track')
            raise Exception(log_get(self, 'failed to track'))
        finally:
            self._lock.release()
    
    def chkoff(self):
        self._lock.acquire()
        try:
            self._count += 1
            if self._count == WQUE_CMD_CHK_INTERVAL:
                self._count = 0
            if not self._event.is_set():
                off = self._que.chkoff()
                if off < WQUE_CMD_IDX_OFFSET:
                    self._event.set()
            elif 0 == self._count:
                off = self._que.chkoff()
                if off >= WQUE_CMD_IDX_OFFSET:
                    self._event.clear()
        except:
            log_err(self, 'failed to check offset')
            raise Exception(log_get(self, 'failed to check offset'))
        finally:
            self._lock.release()
        
        if not self._event.is_set():
            self._event.wait(timeout=WQUE_CMD_WAIT_TIME)
            
    def track_peers(self):
        self._lock.acquire()
        try:
            return self._que.track_peers()
        except:
            log_err(self, 'failed to track peers')
            raise Exception(log_get(self, 'failed to track peers'))
        finally:
            self._lock.release()
    
    def pop(self):
        self._lock.acquire()
        try:
            index, cmd = self._que.choose()
            if index and not self._event.is_set():
                off = self._que.chkoff()
                if off < WQUE_CMD_IDX_OFFSET:
                    self._event.set()
            return (index, cmd)
        except:
            log_err(self, 'failed to pop')
            raise Exception(log_get(self, 'failed to pop'))
        finally:
            self._lock.release()
    