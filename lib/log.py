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
from path import PATH_LOG
from default import DEBUG, LOG_ERR, LOG_FILE

def _getcls(obj):
    if obj:
        return obj.__class__.__name__
    else:
        return 'Debug'
    
def log_get(obj, s):
    return _getcls(obj) + ': ' + str(s)

def log(obj, s):
    if DEBUG:
        print log_get(obj, s)

def log_err(obj, s):
    if DEBUG and LOG_ERR:
        print log_get(obj, s)

def log_file(obj, name, s):
    if DEBUG and LOG_FILE:
        name = _getcls(obj).lower() + '.' + str(name)
        path = os.path.join(PATH_LOG, name)
        f = open(path, 'a')
        if f:
            f.write(str(s) + '\n')
            f.close()

def log_clean(obj):
    if DEBUG:
        prefix = _getcls(obj).lower()
        for name in os.listdir(PATH_LOG):
            path = os.path.join(PATH_LOG, name)
            if not os.path.isdir(path):
                if len(prefix) <= len(name):
                    if cmp(prefix, name[:len(prefix)]) == 0:
                        os.remove(path)
        