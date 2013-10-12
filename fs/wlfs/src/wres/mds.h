#ifndef _MDS_H
#define _MDS_H

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "resource.h"

#ifdef ZOOKEEPER
#include "zk.h"
#endif

int wres_mds_count(char *path);
int wres_mds_delete(char *path);
int wres_mds_access(char *path);
int wres_mds_get_max_key(char *path);
int wres_mds_read(char *path, char *buf, int len);
int wres_mds_write(char *path, char *buf, int len);

#endif