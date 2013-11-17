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
from log import log_err
from default import WCTRL_PORT, LOCAL_HOST
from stream import stream_input, stream_output

class WCtrld(Thread):
	def add_ctrl(self, ctrl):
		self._ctrls.update({ctrl.name:ctrl})
	
	def _init_srv(self, addr):
		self._srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		self._srv.bind((addr, WCTRL_PORT))
		self._srv.listen(5)
		
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
			stream_input(sock, res)
		except TimeoutError:
			log_err(self, 'failed to process (timeout)')
			pool.terminate()
		finally:
			sock.close()
		
	def run(self):
		while True:
			try:
				sock = None
				sock = self._srv.accept()[0]
				req = stream_output(sock)
				try:
					op = req['op']
					args = req['args']
					timeout = req['timeout']
					ctrl = self._ctrls[req['ctrl']]
				except:
					log_err(self, 'invalid parameters')
					sock.close()
					continue
				thread = Thread(target=self.proc, args=(sock, ctrl, op, args, timeout))
				thread.start()
			except:
				if sock:
					sock.close()
		
if __name__ == '__main__':
	if len(sys.argv) < 2:
		sys.exit(0)
	
	if sys.argv[1] == '-l':
		local = WCtrld(LOCAL_HOST)
		local.add_ctrl(Auth())
		local.add_ctrl(Checkpoint())
		local.start()
		local.join()
	elif sys.argv[1] == '-r':
		if len(sys.argv) != 3:
			sys.exit(0)
		addr = sys.argv[2]
		remote = WCtrld(addr)
		remote.add_ctrl(Auth())
		remote.add_ctrl(Secret())
		remote.add_ctrl(Heartbeat())
		remote.add_ctrl(Checkpoint())
		remote.start()
		remote.join()
