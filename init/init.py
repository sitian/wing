#      init.py
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
import errno
import bridge
import container

import sys
sys.path.append('../lib')
from default import *
from path import *

def mkdirs(path):
    try:
        os.makedirs(path)
    except os.error, e:
        if e.errno != errno.EEXIST:
            raise

def set_paths():
    mkdirs(PATH_WING_PUBLIC)
    mkdirs(PATH_WING_PRIVATE)
    mkdirs(PATH_WING_VAR_LIB)
    mkdirs(PATH_WING_VAR_RUN)
    mkdirs(PATH_WING_ENV)
    mkdirs(PATH_WING_MEM)
    mkdirs(PATH_WING_PROC)
    mkdirs(PATH_WING_WCFS)
    mkdirs(PATH_WING_WLFS)
    mkdirs(PATH_WING_WPFS)

def set_devices():
    bridge.init()
    container.init()
    
def set_services():
    os.system('cd ../services/wctrl;python wctrld.py')
    os.system('cd ../services/wview;python wviewd.py')
    os.system('cd ../services/whub;python whubd.py')
    if 1 == SERVER:
        os.system('cd .../services/wroute;python wrouted.py')
    
if __name__ == '__main__':
    set_paths()
    set_devices()
    #set_services()
    