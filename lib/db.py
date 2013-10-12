#      db.py
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

import pymongo
from datetime import datetime
from default import DB_PORT

EXPIRE_TIME = 500 # seconds

class DataBase(object):
    def connect(self):
        pass
    
    def update(self, query, update, upsert):
        pass
    
    def __init__(self, addr=None):
        if not addr:
            addr = '127.0.0.1'
        self._conn = pymongo.Connection(addr, DB_PORT)
        self.connect()
    
    def remove(self, query):
        self._db.remove(query)
    
    def exist(self,query):
        if self._db.find_one(query):
            return True
        else:
            return False


class MainDB(DataBase):
    def connect(self):
        self._db = self._conn.test.main
        
    def update(self, query, update, upsert=True):
        self._db.update(query, update, upsert)
        

class TempDB(DataBase):
    def connect(self):
        self._db = self._conn.test.temp
        self._db.ensure_index('status', expireAfterSeconds=EXPIRE_TIME)
    
    def update(self, query, update, upsert=True):
        status = {'status':datetime.utcnow()}
        if not update.has_key('$set'):
            update.update({'$set':status})
        else:
            update['$set'].update(status)
        self._db.update(query, update, upsert)
    