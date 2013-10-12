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

DB_PORT = 27017
WMON_PORT = 10001
WHUB_PORT = 11001
WCTRL_PORT = 12001
WVIEW_PORT = 13001
ROUTER_FRONTEND_PORT = 14001
ROUTER_BACKEND_PORT = 15001

DB_ADDR = '192.168.154.185'
WMON_ADDR = '10.0.0.1'

ROUTER_FRONTEND_ADDR = 'tcp://192.168.154.185:%d' % (ROUTER_FRONTEND_PORT)
ROUTER_BACKEND_ADDR = 'tcp://192.168.154.185:%d' % (ROUTER_BACKEND_PORT)

WROUTE_ROLE = 0 #0: Router; 1: Worker; 2: Router and Worker
SERVER = 0 #0: user; 1: server

NET_ADAPTER = 'eth0'
DEFAULT_ADDR = '10.0.1.1'
DEFAULT_HOST = '*'
LOG_ERR = 1
DEBUG = 1
