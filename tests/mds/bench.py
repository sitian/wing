#      bench.py
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

import zmq
from json import loads, dumps
from datetime import datetime

import re
import sys
import string
import random

CMD_JOIN = 1
CMD_WAIT = 2
CMD_OK = 3
CMD_RETRY = 4

f = open('cfg')
line_libpath = f.readline().strip()
line_cmdsz = f.readline().strip()
line_rounds = f.readline().strip()
line_port = f.readline().strip()
line_adapter = f.readline().strip()
line_nodes = f.readline().strip()
f.close()

LIB_PATH = re.match('LIB_PATH=(.+)', line_libpath).groups(0)[0]
ROUNDS = int(re.match('ROUNDS=(\d+)', line_rounds).groups(0)[0])
PORT = int(re.match('PORT=(\d+)', line_port).groups(0)[0])
ADAPTER = re.match('ADAPTER=(.+)', line_adapter).groups(0)[0]
NODES = re.match('NODES=(.+)', line_nodes).groups(0)[0].split(',')
CMD_SIZE = int(re.match('CMD_SIZE=(\d+)', line_cmdsz).groups(0)[0])
CMD_BUF = ''.join(random.choice(string.ascii_letters) for i in range(CMD_SIZE))
LEADER = NODES[0]

sys.path.append(LIB_PATH)
from default import zmqaddr
from wmd import WMDReq
import net

def send(sock, buf):
    sock.send(dumps(buf))
    
def recv(sock):
    res = sock.recv()
    if res:
        res = loads(res)
    return res

def join(addr):
    ret = True
    print 'Join=>start (%s)' % addr
    ctx = zmq.Context()
    sock = ctx.socket(zmq.REQ)
    sock.connect(zmqaddr(LEADER, PORT))
    send(sock, {'cmd':CMD_JOIN, 'addr':addr})
    res = recv(sock)
    if not res or not res.has_key('cmd') or res['cmd'] != CMD_OK:
        print 'Failed to join'
        return False
    while True:
        send(sock, {'cmd':CMD_WAIT, 'addr':addr})
        res = recv(sock)
        if not res or not res.has_key('cmd'):
            print 'Failed to receive'
            ret = False
            break
        if res['cmd'] == CMD_OK:
            print 'Join=>finish, @%s' % addr
            break
        elif res['cmd'] != CMD_RETRY:
            print 'Invalid command'
            ret = False
            break
    ctx.destroy()
    return ret
    
def coordinate(addr):
    ret = True
    ctx = zmq.Context()
    sock = ctx.socket(zmq.REP)
    sock.bind(zmqaddr(addr, PORT))
    total = len(NODES) - 1
    nodes = {}
    accepted = {}
    prepared = False
    print 'Coordinate=>start (%s)' % addr
    while True:
        args = recv(sock)
        if not args or not args.has_key('cmd'):
            print 'Failed to receive, invalid arguments'
            ret = False
            break
        if args['cmd'] == CMD_JOIN:
            if not args.has_key('addr') or args['addr'] not in NODES:
                print 'Failed to join, invalid arguments'
                ret = False
                break
            print 'Coordinate=>join (%s)' % args['addr']
            nodes.update({args['addr']:None})
            send(sock, {'cmd':CMD_OK})
            if len(nodes) == total:
                prepared = True
        elif args['cmd'] == CMD_WAIT:
            if prepared:
                print 'Coordinate=>accept (%s)' % args['addr']
                accepted.update({args['addr']:None})
                send(sock, {'cmd':CMD_OK})
                if len(accepted) == total:
                    print 'Coordinate=>finish'
                    break
            else:
                send(sock, {'cmd':CMD_RETRY})
        else:
            print 'Invalid command'
            ret = False
            break
    ctx.destroy()
    return ret
    
def bench_start(addr):
    req = WMDReq(addr)
    req.connect()
    for i in range(ROUNDS):
        req.send(CMD_BUF)
        
if __name__ == '__main__':
    ip = net.chkiface(ADAPTER)
    if ip == LEADER:
        if not coordinate(ip):
            exit(0)
        start = datetime.now()
    else:
        if not join(ip):
            exit(0)
    bench_start(ip)
    if ip == LEADER:
        if not coordinate(ip):
            exit(0)
        stop = datetime.now()
        print 'Time=%d sec' % ((stop - start).seconds)
    else:
        if not join(ip):
            exit(0)
    print 'Finished'
