#      mds.py
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

import sys
from cmd import WMDCmd
from seq import WMDSeq
from mix import WMDMix

class WMDSrv(object):
    def __init__(self, ip):
        self._cmd = WMDCmd(ip)
        self._seq = WMDSeq(ip, self._cmd)
        self._mix = WMDMix(ip, self._cmd, self._seq)
        
    def _start(self):
        self._cmd.start()
        self._mix.start()
        self._seq.start()
    
    def _join(self):
        self._cmd.join()
        self._mix.join()
        self._seq.join()
        
    def _register(self):
        self._seq.register()
        
    def run(self):
        self._start()
        self._register()
        self._join()
    
if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(0)
    WMDSrv(sys.argv[1]).run()
    