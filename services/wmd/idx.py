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
import base64
from threading import Lock
from json import dumps, loads

sys.path.append('../../lib')
from log import log, log_err, log_get
import net

WMD_IDX_SEP = '.'
WMD_IDX_CMP_ID_FIRST = False

def _sn2str(n):
    return hex(n)[2:]

def _str2sn(s):
    return int(s, 16)

def _inc_sn(sn):
    return sn + 1

def _idgen():
    return _id2str(uuid.uuid4().bytes)

def _id2str(identity):
    return base64.urlsafe_b64encode(identity).rstrip('=')

def idxid(index):
    return str(index).split(WMD_IDX_SEP)[0]

def idxsn(index):
    return _str2sn(str(index).split(WMD_IDX_SEP)[1])

def idxdec(index):
    i, s = str(index).split(WMD_IDX_SEP)
    return (i, _str2sn(s))

def idxenc(identity, sn):
    return identity + WMD_IDX_SEP + _sn2str(sn)

def idxinc(index):
    identity, sn = idxdec(index)
    return idxenc(identity, _inc_sn(sn))

def idxgen(identity=None, sn=0):
    if not identity:
        return idxenc(_idgen(), sn)
    else:
        return idxenc(identity, sn)

def idxcmp(i1, i2):
    id1, sn1 = idxdec(i1)
    id2, sn2 = idxdec(i2)
    if WMD_IDX_CMP_ID_FIRST:
        ret = cmp(id1, id2)
        if ret != 0:
            return ret
    if sn1 < sn2:
        return -1
    elif sn1 > sn2:
        return 1
    else:
        if not WMD_IDX_CMP_ID_FIRST:
            return cmp(id1, id2)
        else:
            return 0

class WMDIndex(object):
    def __init__(self, index):
        self.__que = {}
        self.__lock = Lock()
        self.__id, self.__sn = idxdec(index)
    
    def _update_sn(self):
        self.__sn = _inc_sn(self.__sn)
        
    def idxnxt(self, index, cmd):
        self.__lock.acquire()
        try:
            identity, sn = idxdec(index)
            if identity == self.__id:
                if sn > self.__sn:
                    self.__que.update({sn:(index, cmd)})
                elif sn == self.__sn:
                    self._update_sn()
                    return True
            return False
        except:
            log_err(self, 'failed to update')
        finally:
            self.__lock.release()
    
    def idxchk(self):
        return idxenc(self.__id, self.__sn)
     
    def idxget(self):
        self.__lock.acquire()
        res = self.idxchk()
        self._update_sn()
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
            args = loads(buf)
            return (args[0], args[1])
        else:
            return (None, None) 
    
    def idxrcv(self, sock):
        cmd = None
        index = None
        while True:
            self.__lock.acquire()
            try:
                if self.__que.has_key(self.__sn):
                    index, cmd = self.__que[self.__sn]
                    del self.__que[self.__sn]
                    self._update_sn()
                    return (index, cmd)
                else:
                    self.__lock.release()
                    try:
                        index, cmd = self._idxrcv(sock)
                    except:
                        raise Exception(log_get(self, 'failed to receive'))
                    finally:
                        self.__lock.acquire()
                    if not index:
                        log_err(self, 'invalid index')
                        raise Exception(log_get(self, 'invalid index'))
                    identity, sn = idxdec(index)
                    if identity != self.__id:
                        log_err(self, 'invalid id')
                        raise Exception(log_get(self, 'invalid id'))
                    if sn == self.__sn:
                        self._update_sn()
                        return (index, cmd)
                    elif sn > self.__sn:
                        self.__que.update({sn:(index, cmd)})
            finally:
                self.__lock.release()
        