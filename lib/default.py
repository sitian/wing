#      default.py
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

import re
from path import PATH_WING_CONFIG
    
DB_PORT = 27017
WMON_PORT = 10001
WHUB_PORT = 11001
WCTRL_PORT = 12001
WVIEW_PORT = 13001
WROUTE_PORT = 14001
WROUTE_BACKEND_PORT = 15001
WMD_PORT = 16001
WMD_GEN_PORT = 17001
WMD_SEQ_PORT = 18001
WMD_REG_PORT = 19001
WMD_MIX_PORT = 20001
WMD_HEARTBEAT_PORT = 21001
WMD_REP_PORT = 22001

DEFAULT_HOST = '*'
LOCAL_HOST = '127.0.0.1'
DEFAULT_ADDR = '10.0.1.1'
DB_ADDR = '192.168.154.185'
SRV_ADDR = '192.168.154.185'

def zmqaddr(addr, port):
    return 'tcp://%s:%d' % (addr, port)

WROUTE_ADDR = zmqaddr(SRV_ADDR, WROUTE_PORT)
WROUTE_BACKEND_ADDR = zmqaddr(SRV_ADDR, WROUTE_BACKEND_PORT)

SERVER = 0 #0: client; 1: server
WROUTE_ROLE = 0 #0: Router; 1: Worker; 2: Router and Worker
WMD_HEARTBEAT_INTERVAL = 2 # sec

DEBUG = True
LOG_ERR = True
LOG_FILE = True
NET_ADAPTER = 'eth0'
        
def getdef(attr):
    pattern = attr + '=(\d+)'
    f = open(PATH_WING_CONFIG)
    lines = f.readlines()
    f.close()
    for l in lines:
        res = re.match(pattern, l)
        if res and 1 == len(res.groups()):
            return int(res.groups()[0])
    return None
            
