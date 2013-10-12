/*      zk.c
 *      
 *      Copyright (C) 2013 Yi-Wei Ci <ciyiwei@hotmail.com>
 *      
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#include "zk.h"

static inline void watcher(zhandle_t *zzh, int type, int state, const char *path,
             void *watcherCtx) {}

static inline void free_string_vector(struct String_vector *v) {
    if (v->data) {
        int32_t i;
        for (i = 0; i < v->count; i++)
            free(v->data[i]);
        free(v->data);
        v->data = 0;
    }
}


int wres_zk_resolve(char *addr, int port)
{
	char *p;
	FILE *fp;
	char cmd[128];
	char mds_addr[16] = {0};

	sprintf(cmd, "grep mds%lx /etc/hosts | awk -F' ' '{print $1}'", domain_name);
	fp = popen(cmd, "r");
	if (!fp) {
		wres_log("failed to resolve domain %lx", domain_name);
		return -EINVAL;
	}
	fgets(mds_addr, sizeof(mds_addr), fp);
	pclose(fp);
	p = strstr(mds_addr, "\n");
	if (p)
		p[0] = 0;
	if (INADDR_NONE == inet_addr(mds_addr)) {
		wres_log("invalid mds address");
		return -EINVAL;
	}
	sprintf(addr, "%s:%d", mds_addr, port);
	return 0;
}


int wres_zk_read(char *addr, char *path, char *buf, int len)
{
	int ret;
	struct Stat stat;
	zhandle_t *zh = zookeeper_init(addr, watcher, 50000, 0, 0, 0);
	
	if (!zh)
		return -EINVAL;
	ret = zoo_get(zh, path, 0, (char *)buf, &len, &stat);
	zookeeper_close(zh);
	return ret;
}


int wres_zk_write(char *addr, char *path, char *buf, int len)
{
	char *end;
	char *p = path;
	char subpath[WRES_PATH_MAX];
	struct Stat stat;
	zhandle_t *zh;
	int ret = 0;
	
	if (path[0] != '/')
		return -EINVAL;

	zh = zookeeper_init(addr, watcher, 50000, 0, 0, 0);
	if (!zh)
		return -EINVAL;
	
	while ((end = strchr(p + 1, '/')) != NULL) {
		int n = end - path;

		p = end;
		memcpy(subpath, path, n);
		subpath[n] = '\0';
		if (!ret)
			ret = zoo_exists(zh, subpath, 0, &stat);
  		if (ZNONODE == ret) {
			int err = zoo_create(zh, subpath, NULL, 0, &ZOO_OPEN_ACL_UNSAFE, 0, NULL, 0);
			 
			if ((err != ZOK) && (err != ZNODEEXISTS)) {
				ret = -EFAULT;
				goto out;
			}
  		} else if (ret) {
			ret = -EFAULT;
			goto out;
		}
	}
	ret = zoo_create(zh, path, buf, len, &ZOO_OPEN_ACL_UNSAFE, 0, NULL, 0);
	if (ZNODEEXISTS == ret)
		ret = -EEXIST;
	else if (ret)
		ret = -EFAULT;
out:
	zookeeper_close(zh);
	return ret;
}


int wres_zk_access(char *addr, char *path)
{
	int ret;
	zhandle_t *zh;
	struct Stat stat;

	zh = zookeeper_init(addr, watcher, 50000, 0, 0, 0);
	if (!zh)
		return -EINVAL;
	ret = zoo_exists(zh, path, 0, &stat);
	if (ZNONODE == ret)
		ret = -ENOENT;
	else if (ret)
		ret = -EFAULT;
	zookeeper_close(zh);
	return ret;
}


int wres_zk_do_delete(zhandle_t *zh, char *path)
{
	int i;
	int ret;
	int root = 0;
	char *pstr;
	char subpath[WRES_PATH_MAX];
	struct String_vector vector;

	vector.count = 0;
	vector.data = NULL;
	ret = zoo_get_children(zh, path, 0, &vector);
	if (ret)
		return ret;
	if ((1 == strlen(path)) && ('/' == path[0]))
		root = 1;
	strcpy(subpath, path);
	pstr = subpath + strlen(subpath);
	for (i = 0; i < vector.count; i++) {
		if (root) {
			if (!strcmp(vector.data[i], "zookeeper"))
				continue;
			strcat(subpath, vector.data[i]);
		} else
			sprintf(pstr, "/%s", vector.data[i]);
		ret = wres_zk_do_delete(zh, subpath);
		*pstr = '\0';
		if (ret)
			goto out;
	}
	if (!root)
		ret = zoo_delete(zh, path, -1);
out:
	if (vector.data)
		free_string_vector(&vector);
	return ret;
}


int wres_zk_delete(char *addr, char *path)
{
	int ret;
	zhandle_t *zh;
		
	zh = zookeeper_init(addr, watcher, 50000, 0, 0, 0);
	if (!zh)
		return -EINVAL;

	ret = wres_zk_do_delete(zh, path);
	zookeeper_close(zh);
	return ret;
}


int wres_zk_count(char *addr, char *path)
{
	int ret;
	struct String_vector strings;
	zhandle_t *zh = zookeeper_init(addr, watcher, 50000, 0, 0, 0);
	
	if (!zh)
		return -EINVAL;
	
	ret = zoo_get_children(zh, path, 0, &strings);
	if (ZOK == ret) {
		ret = strings.count;
		free_string_vector(&strings);
	} else
		ret = -EINVAL;
	zookeeper_close(zh);
	return ret;
}


int wres_zk_get_max_key(char *addr, char *path)
{
	int ret = 0;
	struct String_vector strings;
	zhandle_t *zh = zookeeper_init(addr, watcher, 50000, 0, 0, 0);
	
	if (!zh)
		return -EINVAL;
	
	ret = zoo_get_children(zh, path, 0, &strings);
	if (ZOK == ret) {
		int i, key = -1;
		for (i = 0; i < strings.count; i++) {
			int val = strtol(strings.data[i], NULL, 16);
			if (val > key)
				key = val;
		}
		free_string_vector(&strings);
		ret = key;
	} else
		ret = -EINVAL;
	zookeeper_close(zh);
	return ret;
}
