#      device.py
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
from log import log_err

STATE_ACTIVE = 1
STATE_INACTIVE = 2
STATE_UNKNOWN = 3

class WHubDev(object):
    def __init__(self):
        self._op = ['state', 'register', 'unregister', 'mount', 'unmount']
        
    @property
    def name(self):
        return self.__class__.__name__.lower()

    def state(self, *args):
        pass
        
    def register(self, *args):
        pass
    
    def unregister(self, *args):
        pass
        
    def mount(self, *args):
        pass
    
    def unmount(self, *args):
        pass
    
    def proc(self, op, args):
        if op not in self._op:
            log_err('%s: invalid op %s' % (self.name, op))
            return
        try:
            func = getattr(self, op)
            return func(*args)
        except:
            log_err('%s: failed to process (op=%s)' % (self.name, op))
    
