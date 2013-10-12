#ifndef _ZK_H
#define _ZK_H

#include "resource.h"
#include <zookeeper.h>
#include <zoo_lock.h>

int wres_zk_count(char *addr, char *path);
int wres_zk_resolve(char *addr, int port);
int wres_zk_delete(char *addr, char *path);
int wres_zk_access(char *addr, char *path);
int wres_zk_get_max_key(char *addr, char *path);
int wres_zk_read(char *addr, char *path, char *buf, int len);
int wres_zk_write(char *addr, char *path, char *buf, int len);

#endif
