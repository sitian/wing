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
from rep import WMDRep
from que import WMDQue
from sub import getsub
from threading import Lock
from threading import Event

WMD_CMD_WAIT_TIME = 1 # sec
WMD_CMD_CHK_INTERVAL = 4

sys.path.append('../../lib')
from log import log, log_err, log_get
import net

class WMDCmd(WMDRep):
    def __init__(self, ip):
        WMDRep.__init__(self, ip)
        self._que = WMDQue(ip)
        self._event = Event()
        self._lock = Lock()
        self._event.set()
        self._count = 0
    
    def mkactive(self, addr):
        self._lock.acquire()
        try:
            self._que.mkactive(addr)
        except:
            log_err(self, 'failed to make active, addr=%s' % net.ntoa(addr))
            raise Exception(log_get(self, 'failed to make active'))
        finally:
            self._lock.release()
    
    def mkinactive(self, suspect):
        self._lock.acquire()
        try:
            suspect = self._que.chkinactive(suspect)
            if not suspect:
                return True
            nodes = {}
            sub = getsub(net.ntoa(self._addr))
            for item in sub:
                addr = net.aton(item[0])
                if addr not in suspect:
                    nodes.update({addr:None})
            leader = self.get_leader(nodes)
            if leader == self._addr:
                ret = self.coordinate(suspect, len(nodes))
            else:
                ret = self.report(leader, suspect)
            if ret < 0:
                log_err(self, 'failed to report, suspect=%s' % str(suspect.keys()))
                raise Exception(log_get(self, 'failed to report'))
            self._que.mkinactive(suspect)
        except:
            log_err(self, 'failed to make inactive, suspect=%s' % str(suspect.keys()))
            raise Exception(log_get(self, 'failed to make inactive'))
        finally:
            self._lock.release()
    
    def add(self, addr, index, cmd):
        self._lock.acquire()
        try:
            return self._que.add(addr, index, cmd)
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
    
    def chkslow(self):
        self._lock.acquire()
        try:
            self._count += 1
            if self._count == WMD_CMD_CHK_INTERVAL:
                self._count = 0
            addr = self._que.chkslow()
            if not self._event.is_set():
                if not addr or addr == self._addr:
                    self._event.set()
            elif 0 == self._count:
                if addr and addr != self._addr:
                    self._event.clear()
        except:
            log_err(self, 'failed to check offset')
            raise Exception(log_get(self, 'failed to check offset'))
        finally:
            self._lock.release()
        
        if not self._event.is_set():
            self._event.wait(timeout=WMD_CMD_WAIT_TIME)
    
    def pop(self):
        self._lock.acquire()
        try:
            index, cmd = self._que.choose()
            if index and not self._event.is_set():
                addr = self._que.chkslow()
                if not addr or addr == self._addr:
                    self._event.set()
            return (index, cmd)
        except:
            log_err(self, 'failed to pop')
            raise Exception(log_get(self, 'failed to pop'))
        finally:
            self._lock.release()
    