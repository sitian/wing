#      idx.py
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
import uuid
from threading import Lock
from json import dumps, loads

sys.path.append('../../lib')
from default import DEBUG
from log import log_err
import net

WMD_IDX_SEPERATOR = '.'

def getid(index):
    return int(str(index).split(WMD_IDX_SEPERATOR)[0])

def getsn(index):
    return int(str(index).split(WMD_IDX_SEPERATOR)[1])

def extract(index):
    i, s = str(index).split(WMD_IDX_SEPERATOR)
    return (int(i), int(s))

def compare(index1, index2):
    i1 = str(index1).split(WMD_IDX_SEPERATOR)
    i2 = str(index2).split(WMD_IDX_SEPERATOR)
    id1 = int(i1[0])
    id2 = int(i2[0])
    if id1 < id2:
        return -1
    elif id1 > id2:
        return 1
    else:
        sn1 = int(i1[1])
        sn2 = int(i2[1])
        if sn1 < sn2:
            return -1
        elif sn1 > sn2:
            return 1
        else:
            return 0

def show(string, index, addr=None):
    if DEBUG:
        if not addr:
            print '%s id=%d, sn=%d' % (string, getid(index), getsn(index))
        else:
            print '%s %s, sn=%d' % (string, net.ntoa(addr), getsn(index))

class WMDIndex(object):
    def __init__(self, index=None):
        if not index:
            self.__sn = 0
            self.__id = uuid.uuid4().int
        else:
            self.__sn = getsn(index)
            self.__id = getid(index)
        self.__lock = Lock()
        self.__que = {}
    
    def _idxchg(self):
        self.__sn = self.__sn + 1
        
    def idxnxt(self, index, cmd):
        self.__lock.acquire()
        try:
            identity, sn = extract(index)
            if identity == self.__id:
                if sn > self.__sn:
                    self.__que.update({sn:(index, cmd)})
                elif sn == self.__sn:
                    self._idxchg()
                    return True
            return False
        except:
            log_err('WMDIdx: failed to update')
        finally:
            self.__lock.release()
            
    def idxchk(self):
        return '%d%s%d' % (self.__id, WMD_IDX_SEPERATOR, self.__sn)

    def idxget(self):
        self.__lock.acquire()
        res = self.idxchk()
        self._idxchg()
        self.__lock.release()
        return res

    def _idxsnd(self, sock, buf, index):
        sock.send(dumps((index, buf)))
        
    def idxsnd(self, sock, buf, index=None):
        if not index:
            index = self.idxget()
        self._idxsnd(sock, buf, index)
    
    def _idxrcv(self, sock):
        buf = sock.recv()
        if buf:
            msg = loads(buf)
            return (msg[0], msg[1])
        else:
            return (None, None) 
    
    def idxrcv(self, sock):
        cmd = None
        index = None
        
        while True:
            self.__lock.acquire()
            try:
                if self.__que.has_key(self.__sn):
                    index, cmd = self._cmd_que[self.__sn]
                    del self._cmd_que[self.__sn]
                    self._idxchg()
                    return (index, cmd)
                else:
                    self.__lock.release()
                    try:
                        index, cmd = self._idxrcv(sock)
                    except:
                        log_err('WMDIndex: failed to receive')
                        raise Exception('WMDIndex: failed to receive')
                    finally:
                        self.__lock.acquire()
                    if not index:
                        log_err('WMDIndex: invalid index')
                        raise Exception('WMDIndex: invalid index')
                    if getid(index) != self.__id:
                        log_err('WMDIndex: invalid id')
                        raise Exception('WMDIndex: invalid id')
                    sn = getsn(index)
                    if sn == self.__sn:
                        self._idxchg()
                        return (index, cmd)
                    elif getsn(index) > self.__sn:
                        self.__que.update({sn:(index, cmd)})
            finally:
                self.__lock.release()
        