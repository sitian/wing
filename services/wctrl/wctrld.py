#      wctrld.py
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

import socket
from ctrl.auth import Auth
from ctrl.secret import Secret
from ctrl.ckpt import Checkpoint
from ctrl.heartbeat import Heartbeat
from multiprocessing import TimeoutError
from multiprocessing.pool import ThreadPool
from threading import Thread

import sys
sys.path.append('../../lib')
from stream import pack, unpack
from log import log_err
from default import *
import iface

class WCtrld(Thread):
	def _add_ctrl(self, ctrl):
		self._ctrls.update({ctrl.name:ctrl})
	
	def _init_ctrl(self):
		pass
	
	def _init_srv(self):
		pass
		
	def __init__(self):
		Thread.__init__(self)
		self._ctrls = {}
		self._init_ctrl()
		self._init_srv()
	
	@property
	def name(self):
		return self.__class__.__name__
	
	def proc(self, sock, ctrl, op, args, timeout):
		pool = ThreadPool(processes=1)
		async = pool.apply_async(ctrl.proc, (op, args))
		try:
			res = async.get(timeout)
			sock.send(pack(res))
		except TimeoutError:
			log_err('%s: failed to process (timeout)' % self.name)
			pool.terminate()
		finally:
			sock.close()
		
	def run(self):
		while True:
			try:
				sock = None
				sock = self._srv.accept()[0]
				req = unpack(sock)
				try:
					op = req['op']
					args = req['args']
					timeout = req['timeout']
					ctrl = self._ctrls[req['ctrl']]
				except:
					log_err('%s: invalid parameters' % self.name)
					sock.close()
					continue
				thread = Thread(target=self.proc, args=(sock, ctrl, op, args, timeout))
				thread.start()
			except:
				if sock:
					sock.close()

class WCtrldRemote(WCtrld):
	def _init_ctrl(self):
		self._add_ctrl(Auth())
		self._add_ctrl(Checkpoint())
	
	def _init_srv(self):
		addr = iface.chkaddr(NET_ADAPTER)
		self._srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		self._srv.bind((addr, WCTRL_PORT))
		self._srv.listen(5)
		
class WCtrldLocal(WCtrld):
	def _init_ctrl(self):
		self._add_ctrl(Auth())
		self._add_ctrl(Secret())
		self._add_ctrl(Checkpoint())
		self._add_ctrl(Heartbeat())
		
	def _init_srv(self):
		self._srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		self._srv.bind(('127.0.0.1', WCTRL_PORT))
		self._srv.listen(5)
		
if __name__ == '__main__':
	local = WCtrldLocal()
	remote = WCtrldRemote()
	local.start()
	remote.start()
	local.join()
	remote.join()
