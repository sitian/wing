#      container.py
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

from util import  lxc
from device import WHubDev

class Container(WHubDev):
    def state(self, name):
        return lxc.state(name)
    
    def register(self, name):
        lxc.create(name)
        return True
    
    def unregister(self, name):
        lxc.destroy(name)
        return True
    
    def mount(self, name):
        lxc.start(name)
        return True
    
    def unmount(self, name):
        lxc.stop(name)
        return True
    
    