#      wdump.py
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

import ioctl

WDUMP_DEV = '/dev/wdump'
WDUMP_MAGIC = 0xcc
WDUMP_CONT = 0x01
WDUMP_KILL = 0x02 
WDUMP_IOCTL_CHECKPOINT = ioctl.IOW(WDUMP_MAGIC, 1, 8)
WDUMP_IOCTL_RESTART = ioctl.IOW(WDUMP_MAGIC, 2, 8)
