#      sub.py
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
import sys
import zmq
import time

from idx import WMDIndex
from threading import Event
from threading import Thread

sys.path.append('../../lib')
from default import WMD_HEARTBEAT_INTERVAL, WMD_HEARTBEAT_PORT
from default import zmqaddr, getdef
from log import log, log_err
import net

WMD_SUB_SHOW_IP = True

def getsub(ip, port=None):
    try:
        res = re.match('(\d{1,3}\.\d{1,3}\.\d{1,3}\.)\d{1,3}', ip).groups()
        if len(res) != 1:
            log_err(None, 'Failed to get subscribers, invalid address')
            return None
        net = res[0]
        mds_max = getdef('MDS_MAX')
        if not mds_max:
            log_err(None, 'Failed to get subscribers, mds_max=0')
            return None
        sub = []
        for i in range(mds_max):
            addr = '%s%d' % (net, i + 1) 
            sub.append((addr, port))
        return sub
    except:
        log_err(None, 'Failed to get subscribers')
        return None
    
class WMDSub(WMDIndex, Thread):
    def _init_sock(self, ip, port):
        self._context = zmq.Context()
        self._sock = self._context.socket(zmq.SUB)
        self._sock.setsockopt(zmq.SUBSCRIBE, "")
        self._sock.connect(zmqaddr(ip, port))
        self._release = False
        self._start = Event()
        self._stop = Event()
        self._port = port
        self._ip = ip
    
    def _show_ip(self, ip):
        if WMD_SUB_SHOW_IP:
            log(self, '<< ' + ip)
            
    def __init__(self, ip, port, index, heartbeat=False, fault=None, live=None):
        WMDIndex.__init__(self, index)
        Thread.__init__(self)
        self._heartbeat = heartbeat
        self._init_sock(ip, port)
        self._addr = net.aton(ip)
        self._fault = fault
        self._live = live
        self._show_ip(ip)
    
    def _remove(self):
        try:
            if self._fault:
                return self._fault(self._addr)
            return True
        except:
            log_err(self, 'failed to remove')
            return False
        
    def _keep_alive(self):
        try:
            if self._live:
                self._live(self._addr)
        except:
            log_err(self, 'failed to keep alive')
    
    def remove(self):
        self._start.set()
        self._remove()
    
    def recv(self):
        return self.idxrcv(self._sock)
        
    def _destroy(self):
        self._context.destroy()
        self._release = True
        self._start.set()
    
    def _start_heartbeat(self):
        context = zmq.Context()
        sock = context.socket(zmq.REQ)
        sock.connect(zmqaddr(self._ip, WMD_HEARTBEAT_PORT))
        while True:
            err = 1
            try:
                sock.send('h')
                time.sleep(WMD_HEARTBEAT_INTERVAL)
                if sock.recv(zmq.NOBLOCK) == 'l':
                    self._keep_alive()
                    err = 0
            except:
                log_err(self, 'failed to receive heartbeat, %s' % self._ip)
            finally:
                if err:
                    if self._remove():
                        context.destroy()
                        self._destroy()
                        break
                    else:
                        context.destroy()
                        context = zmq.Context()
                        sock = context.socket(zmq.REQ)
                        sock.connect(zmqaddr(self._ip, WMD_HEARTBEAT_PORT))
                        
    def run(self):
        if self._heartbeat:
            t = Thread(target=self._start_heartbeat)
            t.setDaemon(True)
            t.start()
            self._start.wait()
            self._stop.set()
    
    def stop(self):
        if self._heartbeat:
            self._start.set()
            self._stop.wait()
        if not self._release:
            self._context.destroy()
    