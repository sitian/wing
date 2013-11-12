#      secret.py
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

import uuid
from random import randint
from control import WControl

import sys
sys.path.append('../../lib')
from pack import pack, unpack
from db import TempDB

PASSWORD_LENGTH = 8

class Secret(WControl):
    def __init__(self):
        WControl.__init__(self)
        self._db = TempDB()
        
    def create(self):
        name = bytes(uuid.uuid4())
        password = ''
        for i in range(PASSWORD_LENGTH):
            password += chr(randint(ord('a'), ord('z')))
        query = {'username':name}
        update = {'$set':{'password':password}}
        self._db.update(query, update)
        return pack(name, password)
    
    def delete(self, secret):
        name, password = unpack(secret)
        query = {'username':name}
        self._db.remove(query)
        return True
