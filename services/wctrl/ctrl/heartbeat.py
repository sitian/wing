#      heartbeat.py
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

import re
import time
from threading import Lock
from threading import Thread
from control import WControl
from commands import getstatusoutput

import sys
sys.path.append('../../lib')
from default import *
from wmon import *

STAT_RUN = 1
STAT_STOP = 2
SLEEP_TIME = 1 # Seconds

class Updater(Thread):
    def __init__(self):
        Thread.__init__(self)
        self._lock = Lock()
        self._nodes = {}
        
    def add_node(self, name, addr):
        self._lock.acquire()
        self._nodes.update({addr:{'name':name, 'time':time.time()}})
        self._lock.release()
        
    def del_node(self, name, addr):
        self._lock.acquire()
        if self._nodes.has_key(addr) and self._nodes[addr]['name'] == name:
            del self._nodes[addr]
        self._lock.release()
    
    def chk_node(self, name):
        cmd = 'lxc-info -n %s' % name
        output = getstatusoutput(cmd)[1]
        m = re.search('pid:        -1', output)
        if m:
            return False
        else:
            return True
        
    def run(self):
        while True:
            timeout = []
            self._lock.acquire()
            for n in self._nodes:
                t = self._nodes[n]['time']
                name = self._nodes[n]['name'] 
                if time.time() - t >=  WMON_HEARTBEAT_MIN:
                    if self.chk_node(name):
                        if WMon(n).heartbeat.update(n):
                            self._nodes[n]['time'] = time.time()
                        else:
                            timeout.append(n)
                    else:
                        timeout.append(n)
            for n in timeout:
                del self._nodes[n]
            self._lock.release()
            time.sleep(SLEEP_TIME)
    
class Heartbeat(WControl):
    def __init__(self):
        WControl.__init__(self)
        self._stat = STAT_STOP
    
    def start(self, name, addr):
        if STAT_STOP == self._stat:
            self._stat = STAT_RUN
            self._updater = Updater()
            self._updater.start()
        self._updater.add_node(name, addr)
        return True
        
    def stop(self, name, addr):
        if STAT_RUN == self._stat:
            self._updater.del_node(name, addr)
        return True
    
    