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
from datetime import datetime
sys.path.append('../../lib')
from log import log

WMD_HDL_CHK_INTERVAL = 400
WMD_HDL_SHOW_SPEED = True

class WMDHdl(object):
    def __init__(self):
        self._cnt = 0
        self._start_time = None
    
    def _show_speed(self):
        if WMD_HDL_SHOW_SPEED:
            if self._cnt % WMD_HDL_CHK_INTERVAL == 0:
                d = datetime.now() - self._start_time
                t = d.seconds + d.microseconds / 1000000.0
                if t > 0:
                    log(self, 'cps=%d' % int(self._cnt / t))
    
    def _calc_speed(self):
        if self._cnt == 0:
            self._start_time = datetime.now()
        self._cnt += 1
        self._show_speed()
        
    def proc(self, cmd):
        self._calc_speed()
        
    