#      storage.py
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

from util import wpfs
from device import WHubDev

class Storage(WHubDev):
    def register(self, name):
        wpfs.create(name)
        return True
    
    def unregister(self, name):
        wpfs.destroy(name)
        return True
    
    def mount(self, name):
        wpfs.start(name)
        return True
    
    def unmount(self, name):
        wpfs.stop(name)
        return True
    