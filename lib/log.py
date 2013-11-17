#      log.py
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

import os
from default import DEBUG, LOG_ERR, LOG_FILE
from path import PATH_LOG

def _cls_name(cls):
    if cls:
        return cls.__class__.__name__
    else:
        return 'Debug'

def log(cls, s):
    if DEBUG:
        print _cls_name(cls) + ': ' + str(s)

def log_err(cls, s):
    if DEBUG and LOG_ERR:
        print _cls_name(cls) + ': ' + str(s)

def log_file(cls, name, s):
    if DEBUG and LOG_FILE:
        name = _cls_name(cls).lower() + '.' + str(name)    
        path = os.path.join(PATH_LOG, name)
        f = open(path, 'a')
        if f:
            f.write(str(s) + '\n')
            f.close()

def log_clean(cls):
    if DEBUG:
        prefix = _cls_name(cls).lower()
        for name in os.listdir(PATH_LOG):
            path = os.path.join(PATH_LOG, name)
            if not os.path.isdir(path):
                if len(prefix) <= len(name):
                    if cmp(prefix, name[:len(prefix)]) == 0:
                        os.remove(path)
        