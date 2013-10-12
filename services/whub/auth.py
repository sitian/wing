#      auth.py
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

from connector import WHubConnector
from privacy import *
from default import *

import sys
sys.path.append('../../lib')
from wview import WView
from default import *
import iface

class WHubAuth(object):
    def __init__(self):
        self._conn = WHubConnector()
    
    @property
    def name(self):
        return self.__class__.__name__
    
    def exist(self, guest, name, password, privacy, host=None):
        if not host:
            if PRIVATE == privacy:
                host = DEFAULT_HOST
            elif PUBLIC == privacy:
                host = iface.chkaddr(NET_ADAPTER)
        if not host:
            return
        return self._conn.is_active(guest, name, privacy, host)
    
    def confirm(self, guest, name, password, privacy):
        if PRIVATE == privacy:
            return WView().acceptor.accept('%s requests to join...' % name)
        elif PUBLIC == privacy:
            return True
    