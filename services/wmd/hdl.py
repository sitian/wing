#      hdl.py
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

sys.path.append('../../lib')
from log import log

WMD_HDL_CHK_INTERVAL = 16
WMD_HDL_SHOW_CNT = False

class WMDHdl(object):
    def __init__(self):
        self._cnt = 0
    
    def _show_cnt(self):
        if WMD_HDL_SHOW_CNT:
            if self._cnt % WMD_HDL_CHK_INTERVAL == 0:
                log(self, 'cnt=%d' % self._cnt)
            
    def proc(self, cmd):
        self._cnt += 1
        self._show_cnt()
        
    