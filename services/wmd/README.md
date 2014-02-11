WMD
---
---
Yi-Wei Ci &lt;ciyiwei@hotmail.com&gt;  
v0.0.1

Overview
--------
A Meta-Data Service Using Index-Based Consensus


Implementations
---------------
**Generator:**	gen.py
**Sequencer:**	seq.py
**Mixer:**	mix.py

Configuration
-------------
### /etc/default/wing
MDS_MAX=3

Quick Start
-----------
### Clients
**client1@192.168.1.101:**
path/to/wmd/gen.py 192.168.1.101

**client2@192.168.1.102:**
path/to/wmd/gen.py 192.168.1.102

**client3@192.168.1.103:**
path/to/wmd/gen.py 192.168.1.103

### Servers
**server1@192.168.1.1:**
path/to/wmd/mds.py 192.168.1.1

**server2@192.168.1.2:**
path/to/wmd/mds.py 192.168.1.2

**server3@192.168.1.3:**
path/to/wmd/mds.py 192.168.1.3

Testing
-------
###Download
<https://github.com/sitian/wing/tree/master/tests/mds>

###Configuration
path/to/tests/mds/cfg

###Example
**client1@192.168.1.101**
path/to/tests/mds/bench.py

**client2@192.168.1.102**
path/to/tests/mds/bench.py

**client3@192.168.1.103**
path/to/tests/mds/bench.py
