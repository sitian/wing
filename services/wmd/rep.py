#      rep.py
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
import zmq
import idx
import time
from threading import Event
from threading import Thread
from json import loads, dumps

sys.path.append('../../lib')
from default import zmqaddr, WMD_REP_PORT
from log import log, log_err
import net

WMD_REP_START = 1
WMD_REP_ACCEPT = 2
WMD_REP_STOP = 3
WMD_REP_WAIT = 4
WMD_REP_ABORT = 5

WMD_REP_SLEEP_TIME = 1 #sec
WMD_REP_TIMEOUT = 3 * 1000 #msec 
WMD_REP_LONG_TIMEOUT = 9 * 1000 # msec

WMD_REP_SHOW_STAT = True

class WMDRep(Thread):
    def _init_sock(self, ip):
        self._context = zmq.Context()
        self._sock = self._context.socket(zmq.REP)
        self._sock.bind(zmqaddr(ip, WMD_REP_PORT))
        
    def _free_sock(self):
        self._context.destroy()
        self._context = None
        self._poller = None
        self._sock = None
    
    def __init__(self, ip):
        Thread.__init__(self)
        self._res = -1
        self._total = 0
        self._nodes = {}
        self._init_sock(ip)
        self._stop = Event()
        self._start = Event()
        self._addr = net.aton(ip)
    
    def _send(self, sock, buf):
        sock.send(dumps(buf))
    
    def _recv(self, sock, timeout=WMD_REP_TIMEOUT):
        if timeout:
            poller = zmq.Poller()
            poller.register(sock, zmq.POLLIN)
            socks = dict(poller.poll(timeout))
            poller.unregister(sock)
            if not socks or socks.get(sock) != zmq.POLLIN:
                return None
        res = sock.recv()
        if res:
            return loads(res)
        else:
            return None
    
    def get_leader(self, nodes):
        leader = None
        for i in nodes:
            if not leader or leader < i:
                leader = i
        return leader
            
    def _report(self, leader, nodes):
        ret = -1
        rep_id = None
        stat = WMD_REP_START
        context = zmq.Context()
        
        if WMD_REP_SHOW_STAT:
            log(self, 'report=>start, %s, leader=%s' % (net.ntoa(self._addr), net.ntoa(leader)))
                
        try:
            sock = context.socket(zmq.REQ)
            sock.connect(zmqaddr(net.ntoa(leader), WMD_REP_PORT))
            self._send(sock, {'cmd':WMD_REP_START})
            while True:
                args = self._recv(sock)
                if not args or not args.has_key('cmd'):
                    log_err(self, 'failed to report, invalid command')
                    break
                
                cmd = args['cmd']
                if cmd != stat and cmd != WMD_REP_WAIT:
                    log_err(self, 'invalid command, cmd=%d' % cmd)
                    break
                
                if not args.has_key('id'):
                    log_err(self, 'no report id, cmd=%d' % cmd)
                    break
                
                if cmd != WMD_REP_START and args['id'] != rep_id:
                    log_err(self, 'invalid report id, cmd=%d' % cmd)
                    break
                
                if cmd == WMD_REP_START:
                    rep_id = args['id']
                    self._send(sock, {'cmd':WMD_REP_ACCEPT, 'id':rep_id, 'addr':self._addr, 'nodes':nodes})
                    stat = WMD_REP_ACCEPT
                    if WMD_REP_SHOW_STAT:
                        log(self, 'report=>accept (id=%s)' % rep_id)
                elif cmd == WMD_REP_ACCEPT:
                    self._send(sock, {'cmd':WMD_REP_STOP, 'addr':self._addr, 'id':rep_id})
                    stat = WMD_REP_STOP
                    if WMD_REP_SHOW_STAT:
                        log(self, 'report=>stop (id=%s)' % rep_id)
                elif cmd == WMD_REP_WAIT:
                    time.sleep(WMD_REP_SLEEP_TIME)
                    self._send(sock, {'cmd':WMD_REP_STOP, 'addr':self._addr, 'id':rep_id})
                    if WMD_REP_SHOW_STAT:
                        log(self, 'report=>wait (id=%s)' % rep_id)
                elif cmd == WMD_REP_STOP:
                    if WMD_REP_SHOW_STAT:
                        log(self, 'report=>finish (id=%s)' % rep_id)
                    ret = 0
                    break 
        finally:
            context.destroy()
            return ret
            
    def report(self, leader, nodes):
        if not nodes or self._addr in nodes:
            log_err(self, 'failed to report, invalid parameters')
            return -1
        try:
            return self._report(leader, nodes)
        except:
            log_err(self, 'failed to report')
            return -1
        
    def _coordinate(self):
        ret = -1
        stop = False
        stopped = {}
        accepted = {}
        total = self._total - 1
        rep_id = idx._idgen()
        
        if WMD_REP_SHOW_STAT:
            log(self, 'coordinate=>start, %s (id=%s)' % (net.ntoa(self._addr), rep_id))
            
        try:
            while True:
                addr = None
                args = self._recv(self._sock, timeout=WMD_REP_LONG_TIMEOUT)
                if not args or not args.has_key('cmd'):
                    log_err(self, 'failed to coordinate, invalid command')
                    break
                
                cmd = args['cmd']
                if cmd != WMD_REP_START:
                    if not args.has_key('addr') or not args.has_key('id') or args['id'] != rep_id:
                        self._send(self._sock, {'cmd':WMD_REP_ABORT})
                        continue
                    addr = args['addr']
                    
                if stop and cmd != WMD_REP_STOP:
                    self._send(self._sock, {'cmd':WMD_REP_ABORT})
                    continue
                
                if cmd == WMD_REP_ACCEPT:
                    if not args.has_key('nodes'):
                        log_err(self, 'failed to get nodes')
                        self._send(self._sock, {'cmd':WMD_REP_ABORT})
                        break
                    
                    match = True
                    nodes = args['nodes']
                    if len(nodes) != len(self._nodes):
                        match = False
                    else:
                        for i in nodes:
                            if int(i) not in self._nodes:
                                match = False
                                break
                    if not match:
                        self._send(self._sock, {'cmd':WMD_REP_ABORT})
                        break
                    
                    accepted.update({addr:None})
                    if len(accepted) == total:
                        stop = True
                    
                    if WMD_REP_SHOW_STAT:
                        log(self, 'coordinate=>accept, %s (id=%s)' % (net.ntoa(addr), rep_id))
                elif cmd == WMD_REP_STOP:
                    if not stop:
                        cmd = WMD_REP_WAIT
                        if WMD_REP_SHOW_STAT:
                            log(self, 'coordinate=>wait, %s (id=%s)' % (net.ntoa(addr), rep_id))
                    else:
                        if addr in self._nodes:
                            log(self, 'found an available node')
                            break
                        stopped.update({addr:None})
                        if WMD_REP_SHOW_STAT:
                            log(self, 'coordinate=>stop, %s (id=%s)' % (net.ntoa(addr), rep_id))
                self._send(self._sock, {'cmd':cmd, 'id':rep_id})
                if len(stopped) == total:
                    ret = 0
                    if WMD_REP_SHOW_STAT:
                        log(self, 'coordinate=>finish (id=%s)' % rep_id)
                    break
        except:
            self._free_sock()
            self._init_sock(net.ntoa(self._addr))
        return ret
    
    def coordinate(self, nodes, total):
        if not nodes or self._addr in nodes or total <= 0:
            log_err(self, 'failed to coordinate, invalid parameters')
            return -1
        self._nodes = nodes
        self._total = total
        self._start.set()
        self._stop.wait()
        self._stop.clear()
        return self._res
    
    def run(self):
        while True:
            self._start.wait()
            self._start.clear()
            self._res = self._coordinate()
            self._stop.set()
    