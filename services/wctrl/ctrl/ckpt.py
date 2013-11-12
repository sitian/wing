#      ckpt.py
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

import os
import fcntl
import struct
from control import WControl
from util.wdump import WDUMP_DEV, WDUMP_KILL, WDUMP_IOCTL_CHECKPOINT

class Checkpoint(WControl):
	def save(self, pid):
		f = open(WDUMP_DEV, 'w')
		if not f:
			return False
		try:
			buf = struct.pack('ii', pid, WDUMP_KILL)
			if not fcntl.ioctl(f.fileno(), WDUMP_IOCTL_CHECKPOINT, buf):
				return True
			return False
		finally:
			f.close()
			
	def restore(self, pid):
		os.system('gnome-terminal -x bash -c "restart ' + str(pid) + ';exec bash"')
		return True
