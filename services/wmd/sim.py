#      sim.py
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

from rep import WMDRep

import sys
sys.path.append('../../lib')
import net

NET_ADAPTER = 'eth0'
ip_list = ['192.168.1.1', '192.168.1.2', '192.168.1.3', '192.168.1.4', '192.168.1.5']
crashed = ['192.168.1.5']

if __name__ == '__main__':
    ip = net.chkiface(NET_ADAPTER)
    if ip not in crashed:
        nodes = []
        crashed_nodes = []
        for i in ip_list:
            if i in crashed:
                crashed_nodes.append(net.aton(i))
            else:
                nodes.append(net.aton(i))
        rep = WMDRep(ip)
        rep.start()
        leader = rep.get_leader(nodes)
        if leader == net.aton(ip):
            while True:
                ret = rep.coordinate(crashed_nodes, len(nodes))
                if not ret:
                    break
        else:
            while True:
                ret = rep.report(leader, crashed_nodes)
                if not ret:
                    break
    