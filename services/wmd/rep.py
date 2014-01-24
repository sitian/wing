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
import seq
import time
from copy import copy
from bar import WMDBar
from threading import Thread
from json import loads, dumps

sys.path.append('../../lib')
from default import zmqaddr, WMD_REP_PORT, WMD_REP_SYNC_PORT
from log import log, log_err, log_get
import net

WMD_REP_START = 1
WMD_REP_ACCEPT = 2
WMD_REP_STOP = 3
WMD_REP_WAIT = 4
WMD_REP_EXIT = 5

WMD_REP_SLEEP_TIME = 1 #sec
WMD_REP_TIMEOUT = 5 * 1000 #msec 
WMD_REP_LONG_TIMEOUT = 10 * 1000 # msec

WMD_REP_SHOW_REQ = True
WMD_REP_SHOW_SYNC_CMDS = True
WMD_REP_SHOW_SYNC_ARGS = True

class WMDRep(Thread):
    def _init_sock(self, ip):
        self._context = zmq.Context()
        self._sock = self._context.socket(zmq.REP)
        self._sock.bind(zmqaddr(ip, WMD_REP_PORT))
    
    def __init__(self, ip):
        Thread.__init__(self)
        self._ip = ip
        self._res = -1
        self._lst = None
        self._nodes = None
        self._target = None
        self._init_sock(ip)
        self._barrier = WMDBar()
        self._addr = net.aton(ip)
   
    def _show_sync_args(self, args):
        if WMD_REP_SHOW_SYNC_ARGS:
            end = args.get('end')
            dest = args.get('dest')
            if dest and end:
                log(self, "sync, args={'dest':%s, 'end':%s}" % (net.ntoa(dest), end))
            else:
                log(self, 'sync, args=%s' % str(args))
                
    def _show_sync_cmds(self, cmds):
        if WMD_REP_SHOW_SYNC_CMDS:
            log(self, 'sync, cmds=%s' % str(cmds))
            
    def _show_req(self, cmd):
        if WMD_REP_SHOW_REQ:
            if cmd == WMD_REP_START:
                log(self, 'report=>start')
            if cmd == WMD_REP_ACCEPT:
                log(self, 'report=>accept')
            elif cmd == WMD_REP_STOP:
                log(self, 'report=>stop')
            elif cmd == WMD_REP_WAIT:
                log(self, 'report=>wait')
            elif cmd == WMD_REP_EXIT:
                log(self, 'report=>exit')
    
    def _send(self, sock, buf):
        sock.send(dumps(buf))
    
    def _send_req(self, sock, cmd, args=None):
        if args:
            args.update({'cmd':cmd})
        else:
            args = {'cmd':cmd}
        self._send(sock, args)
        self._show_req(cmd)
        
    def _recv(self, sock, timeout=None):
        if timeout:
            poller = zmq.Poller()
            poller.register(sock, zmq.POLLIN)
            socks = dict(poller.poll(timeout))
            poller.unregister(sock)
            if not socks or socks.get(sock) != zmq.POLLIN:
                return
        buf = sock.recv()
        if buf:
            return loads(buf)
    
    def _get_leader(self, nodes):
        leader = None
        for i in nodes:
            if not leader or leader < i:
                leader = i
        return leader
    
    def _add_cmd(self, addr, index, cmd):
        pass
    
    def _sync(self, addr, lst, args):
        start = idx.idxinc(lst)
        dest = args.get('dest')
        end = args.get('end')
        self._show_sync_args(args)
        if not dest or not end:
            return
        if dest != self._addr and idx.idxcmp(start, end) < 0:
            try:
                context = zmq.Context()
                sock = context.socket(zmq.REQ)
                sock.connect(zmqaddr(net.ntoa(dest), WMD_REP_SYNC_PORT))
                self._send(sock, {'addr':addr, 'start':start, 'end':end})
                cmds = self._recv(sock, WMD_REP_TIMEOUT)
                self._show_sync_cmds(cmds)
                index = start
                track = {}
                for item in cmds:
                    cmd = seq.pack(track, item[0], item[1])
                    self._add_cmd(addr, index, cmd, False)
                    index = idx.idxinc(index)
            finally:
                context.destroy()
        
    def _report(self, addr, lst, leader):
        ret = -1
        rep_id = None
        curr = WMD_REP_START
        context = zmq.Context()
        
        try:
            sock = context.socket(zmq.REQ)
            sock.connect(zmqaddr(net.ntoa(leader), WMD_REP_PORT))
            req = {'src':self._addr, 'target':addr, 'lst':lst}
            self._send_req(sock, WMD_REP_START, req)
            while True:
                args = self._recv(sock, WMD_REP_TIMEOUT)
                if not args or not args.has_key('cmd'):
                    log_err(self, 'failed to report (no reply)')
                    break
                
                cmd = args['cmd']
                if cmd != curr and cmd != WMD_REP_WAIT:
                    log_err(self, 'failed to report (invalid command)')
                    break
                
                if not args.has_key('id'):
                    log_err(self, 'failed to report (no report id)')
                    break
                
                if cmd != WMD_REP_START:
                    if args['id'] != rep_id:
                        log_err(self, 'failed to report (invalid report id)')
                        break
                else:
                    rep_id = args['id']
                    req = {'id':rep_id, 'src':self._addr}
                
                if cmd == WMD_REP_START:
                    curr = WMD_REP_ACCEPT
                elif cmd == WMD_REP_ACCEPT:
                    curr = WMD_REP_STOP
                    self._sync(addr, lst, args)
                elif cmd == WMD_REP_WAIT:
                    time.sleep(WMD_REP_SLEEP_TIME)
                elif cmd == WMD_REP_STOP:
                    ret = 0
                    break
                self._send_req(sock, curr, req)
        finally:
            context.destroy()
            return ret
        
    def report(self, addr, lst, nodes):
        if self._addr not in nodes:
            log_err(self, 'failed to report (invalid parameters)')
            return -1
        try:
            leader = self._get_leader(nodes)
            if not leader:
                log_err(self, 'failed to report (no leader)')
                return -1
            if leader == self._addr:
                self._lst = lst
                self._target = addr
                self._nodes = nodes
                self._barrier.wait()
                return self._res
            else:
                return self._report(addr, lst, leader)
        except:
            log_err(self, 'failed to report')
            return -1
    
    def _coordinate(self):
        ret = -1
        end = None
        dest = None
        req_list = []
        sock = self._sock
        rep_id = idx._idgen()
        req = {'id':rep_id}
        req_base = copy(req)
        curr = WMD_REP_START
        total = len(self._nodes) - 1
        
        try:
            while ret != 0:
                args = self._recv(sock, timeout=WMD_REP_LONG_TIMEOUT)
                if not args or not args.has_key('cmd'):
                    log_err(self, 'failed to coordinate')
                    break
                
                cmd = args['cmd']
                src = args.get('src')
                off = int(cmd) - curr
                if not src or src not in self._nodes or (cmd != WMD_REP_START and args.get('id') != rep_id) or off < 0 or off > 1:
                    self._send_req(sock, WMD_REP_EXIT)
                    continue
                
                if off != 0:
                    cmd = WMD_REP_WAIT
                    self._send_req(sock, cmd, req_base)
                    continue
                
                if cmd == WMD_REP_START:
                    target = args.get('target')
                    if target != self._target:
                        self._send_req(sock, WMD_REP_EXIT)
                        break
                    lst = args.get('lst')
                    if lst and (not end or idx.idxcmp(end, lst) < 0):
                        end = lst
                        dest = src
                
                self._send_req(sock, cmd, req)
                if src not in req_list:
                    req_list.append(src)
                if len(req_list) == total:
                    req_list = []
                    if cmd == WMD_REP_START:
                        req.update({'dest':dest, 'end':end})
                        self._sync(target, self._lst, req)
                        curr = WMD_REP_ACCEPT
                    elif cmd == WMD_REP_ACCEPT:
                        req = copy(req_base)
                        curr = WMD_REP_STOP
                    elif cmd == WMD_REP_STOP:
                        ret = 0
        except:
            self._context.destroy()
            self._init_sock(net.ntoa(self._addr))
        return ret
    
    def collect(self, addr, start, end):
        pass
    
    def _listen(self):
        context = zmq.Context()
        sock = context.socket(zmq.REP)
        sock.bind(zmqaddr(self._ip, WMD_REP_SYNC_PORT))
        try:
            while True:
                args = self._recv(sock)
                if not args:
                    log_err(self, 'failed to listen (no argument)')
                    continue
                cmds = self.collect(args['addr'], args['start'], args['end'])
                self._send(sock, cmds)
        except:
            log_err(self, 'failed to listen')
            raise Exception(log_get(self, 'failed to listen'))
        finally:
            context.destroy()
    
    def run(self):
        Thread(target=self._listen).start()
        while True:
            self._barrier.get()
            self._res = self._coordinate()
            self._barrier.put()
    