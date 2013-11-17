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

sys.path.append('../../lib')
from default import getdef
from log import log_err
import net

class WMDCmd(WMDRep):
    def __init__(self, ip):
        WMDRep.__init__(self, ip)
        self._mds_max = getdef('MDS_MAX')
        self._quorum = int(self._mds_max / 2) + 1
        self._que = WMDQue(ip, self._mds_max, self._quorum)
        self._lock = Lock()
        self._index = {}
        self._head = []
    
    def _check(self, index):
        identity, sn = idx.extract(index)
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
            return False
        finally:
            self._lock.release()
    
    def mkinactive(self, suspect):
        self._lock.acquire()
        try:
            return self._que.mkinactive(self, suspect)
        except:
            log_err(self, 'failed to make inactive, suspect=%s' % str(suspect))
            return False
        finally:
            self._lock.release()
    
    def add(self, addr, index, cmd, update=False):
        self._lock.acquire()
        try:
            if self._check(index) < 0:
                return
            self._que.add(addr, index, cmd, update)
        except:
            log_err(self, 'failed to add, %s' % net.ntoa(addr))
        finally:
            self._lock.release()
    
    def track(self):
        self._lock.acquire()
        try:
            return self._que.track()
        except:
            log_err(self, 'failed to track')
        finally:
            self._lock.release()
    
    def pop(self):
        cmd = None
        index = None
        self._lock.acquire()
        try:
            index, cmd = self._que.choose()
        except:
            log_err(self, 'failed to pop')
        finally:
            self._lock.release()
            return (index, cmd)
        