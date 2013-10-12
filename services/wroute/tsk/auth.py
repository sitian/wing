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

from task import WRouteTask

import sys
sys.path.append('../../lib')
from wctrl import WCtrl

class Auth(WRouteTask):
    def login(self, name, secret, privacy, host):
        return WCtrl().auth.login(name, secret, privacy, host)
    
    def logout(self, name, secret, privacy, host):
        return WCtrl().auth.logout(name, secret, privacy, host)
    
    def exist(self, name, privacy, host):
        return WCtrl().auth.exist(name, privacy, host)
        