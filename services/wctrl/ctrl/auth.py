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

from control import WControl

import sys
sys.path.append('../../lib')
from privacy import PRIVATE, PUBLIC
from default import DB_ADDR
from db import MainDB

class Auth(WControl):     
    def login(self, name, secret, privacy, host):
        if PRIVATE == privacy:
            db = MainDB()
        elif PUBLIC == privacy:
            db = MainDB(DB_ADDR)
        else:
            return
        query = {'username':name}
        update = {'$addToSet':{'host':host}}
        db.update(query, update)
        update = {'$addToSet':{'secret':secret}}
        db.update(query, update)
        return True
    
    def logout(self, name, secret, privacy, host):
        if PRIVATE == privacy:
            db = MainDB()
        elif PUBLIC == privacy:
            db = MainDB(DB_ADDR)
        else:
            return
        query = {'username':name}
        update = {'$pull':{'host':host}}
        db.update(query, update, False)
        update = {'$pull':{'secret':secret}}
        db.update(query, update, False)
        return True
    
    def exist(self, name, privacy, host):
        if PRIVATE == privacy:
            db = MainDB()
        elif PUBLIC == privacy:
            db = MainDB(DB_ADDR)
        else:
            return
        query = {'username':name, 'host':host}
        if db.exist(query):
            return True
        else:
            return False
    