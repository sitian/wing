#      stream.py
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

import json
import struct

def stream_input(steam, obj):
    buf = json.dumps(obj)
    h = struct.pack('i', len(buf))
    return steam.send(h + buf)

def stream_output(stream):
    ret = None
    h = stream.recv(len(struct.pack('i', 0)))
    if not h:
        return
    size = struct.unpack('i', h)[0]
    if size > 0:
        buf = stream.recv(size)
        if buf:
            ret = json.loads(buf)
    return ret
