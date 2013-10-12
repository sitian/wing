/*      wlfs.c
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

#include "wlfs.h"

int wlfs_stat = 0;
wres_addr_t local_address;
unsigned long domain_name = 0;
wlfs_lock_group_t wlfs_lock_group[WLFS_LOCK_GROUP_SIZE];

int wlfs_check_domain(char *path)
{
	char *p;
	char *pend;
	
	if (strncmp(path, WLFS_MOUNT_PATH, strlen(WLFS_MOUNT_PATH))) {
		wln_log("invalid path");
		return -EINVAL;
	}
	p = path + strlen(WLFS_MOUNT_PATH);
	domain_name = strtol(p, &pend, 16);
	if (p == pend) {
		wln_log("invalid path");
		return -EINVAL;
	}
	return 0;
}


int wlfs_check_local_address()
{
	char path[128];

	sprintf(path, "/var/run/ppp-%lx.pid", domain_name);
	if (!access(path, F_OK)) {
		int fd;
		char line[128];
		struct ifreq req;
		struct sockaddr_in *addr;
		FILE *fp = fopen(path, "r");
		
		fscanf(fp, "%s\n", line);
		fscanf(fp, "%s\n", line);
		fclose(fp);

		if (-1 == (fd = socket(PF_INET, SOCK_STREAM, 0)))
			return -EINVAL;

		bzero(&req, sizeof(struct ifreq));
		strcpy(req.ifr_name, line);
		ioctl(fd, SIOCGIFADDR, &req);
		addr = (struct sockaddr_in*)&req.ifr_addr;
		local_address = addr->sin_addr;
		close(fd);
	} else
		local_address.s_addr = LOCAL_ADDRESS;
	return 0;
}


int wlfs_create()
{
	int i;
	int ret;
	pthread_t tid;
	pthread_attr_t attr;
	
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	ret = pthread_create(&tid, &attr, wlnd_main, NULL);
	pthread_attr_destroy(&attr);
#ifdef ZOOKEEPER
	zoo_set_debug_level(ZOO_LOG_LEVEL_ERROR);
#endif
	if (ret) {
		wln_log("failed to create thread");
		return ret;
	}
	for (i = 0; i < WLFS_LOCK_GROUP_SIZE; i++) {
		pthread_mutex_init(&wlfs_lock_group[i].mutex, NULL);
		wlfs_lock_group[i].head = rbtree_create();
	}
	return 0;
}


int wlfs_init(char *path)
{
	int ret;
	
	if (wlfs_stat & WLFS_STAT_INIT)
		return 0;

	ret = wlfs_check_domain(path);
	if (ret) {
		wln_log("failed to check domain");
		return ret;
	}
	ret = wlfs_check_local_address();
	if (ret) {
		wln_log("failed to check local address");
		return ret;
	}
	ret = wlfs_create();
	if (ret) {
		wln_log("failed to create");
		return ret;
	}
	wres_init();
	wres_release();
	wlfs_stat |= WLFS_STAT_INIT;
	return 0;
}


static inline unsigned long wlfs_lock_hash(wlfs_lock_desc_t *desc)
{
	unsigned long *ent = desc->entry;
	
	return (ent[0] ^ ent[1] ^ ent[2] ^ ent[3]) % WLFS_LOCK_GROUP_SIZE;
}


static inline int wlfs_lock_compare(char *l1, char *l2)
{
	wlfs_lock_desc_t *desc1 = (wlfs_lock_desc_t *)l1;
	wlfs_lock_desc_t *desc2 = (wlfs_lock_desc_t *)l2;
	
	return memcmp(desc1->entry, desc2->entry, sizeof(((wlfs_lock_desc_t *)0)->entry));
}


static inline void wlfs_lock_get_desc(wresource_t *resource, wlfs_lock_desc_t *desc)
{
	desc->entry[0] = resource->key;
	desc->entry[1] = resource->cls;
	desc->entry[2] = resource->owner;
	if (WRES_CLS_SHM == resource->cls)
		desc->entry[3] = wres_get_off(resource);
	else
		desc->entry[3] = 0;
}


static inline wlfs_lock_t *wlfs_lock_alloc(wlfs_lock_desc_t *desc)
{
	wlfs_lock_t *lock = (wlfs_lock_t *)malloc(sizeof(wlfs_lock_t));
	
	if (!lock)
		return NULL;
	
	lock->count = 0;
	memcpy(&lock->desc, desc, sizeof(wlfs_lock_desc_t));
	pthread_mutex_init(&lock->mutex, NULL);
	pthread_cond_init(&lock->cond, NULL);
	return lock;
}


static inline wlfs_lock_t *wlfs_lock_lookup(wlfs_lock_group_t *group, wlfs_lock_desc_t *desc)
{
	return rbtree_lookup(group->head, (void *)desc, wlfs_lock_compare);
}


static inline void wlfs_lock_insert(wlfs_lock_group_t *group, wlfs_lock_t *lock)
{
	rbtree_insert(group->head, (void *)&lock->desc, (void *)lock, wlfs_lock_compare);
}


static inline wlfs_lock_t *wlfs_lock_get(wresource_t *resource) 
{
	wlfs_lock_desc_t desc;
	wlfs_lock_group_t *grp;
	wlfs_lock_t *lock = NULL;
	
	wlfs_lock_get_desc(resource, &desc);
	grp = &wlfs_lock_group[wlfs_lock_hash(&desc)];
	pthread_mutex_lock(&grp->mutex);
	lock = wlfs_lock_lookup(grp, &desc);
	if (!lock) {
		lock = wlfs_lock_alloc(&desc);
		if (!lock)
			goto out;
		wlfs_lock_insert(grp, lock);
	}
	pthread_mutex_lock(&lock->mutex);
	lock->count++;
out:
	pthread_mutex_unlock(&grp->mutex);
	return lock;
}


static void wlfs_lock_free(wlfs_lock_group_t *group, wlfs_lock_t *lock)
{
	rbtree_delete(group->head, (void *)&lock->desc, wlfs_lock_compare);
	pthread_mutex_destroy(&lock->mutex); 
	pthread_cond_destroy(&lock->cond);
	free(lock);
}


int wlfs_lock(wresource_t *resource)
{
	wlfs_lock_t *lock;
	
	if (!(wlfs_stat & WLFS_STAT_INIT))
		return -EINVAL;
	
	lock = wlfs_lock_get(resource);
	if (!lock)
		return -ENOENT;
	
	if (lock->count > 1)
		pthread_cond_wait(&lock->cond, &lock->mutex);
	pthread_mutex_unlock(&lock->mutex);
	wlfs_print_lock(resource);
	return 0;
}


void wlfs_unlock(wresource_t *resource)
{
	int empty = 0;
	wlfs_lock_t *lock;
	wlfs_lock_desc_t desc;
	wlfs_lock_group_t *grp;
	
	if (!(wlfs_stat & WLFS_STAT_INIT))
		return;
	
	wlfs_lock_get_desc(resource, &desc);
	grp = &wlfs_lock_group[wlfs_lock_hash(&desc)];
	pthread_mutex_lock(&grp->mutex);
	lock = wlfs_lock_lookup(grp, &desc);
	if (!lock) {
		wln_log("failed to unlock");
		pthread_mutex_unlock(&grp->mutex);
		return;
	}
	pthread_mutex_lock(&lock->mutex);
	if (lock->count > 0) {
		lock->count--;
		if (lock->count > 0)
			pthread_cond_signal(&lock->cond);
		else
			empty = 1;
	}
	pthread_mutex_unlock(&lock->mutex);
	if (empty)
		wlfs_lock_free(grp, lock);
	pthread_mutex_unlock(&grp->mutex);
	wlfs_print_unlock(resource);
}


static int wlfs_open(const char *path, struct fuse_file_info *fi)
{
	int ret;
	wres_args_t args;
	size_t inlen = 0;
	size_t outlen = 0;
	unsigned long addr;
	wresource_t resource;

	if (!(wlfs_stat & WLFS_STAT_INIT))
		return -EINVAL;
	
	ret = wres_path_to_resource(path, &resource, &addr, &inlen, &outlen);
	if (ret) {
		wln_log("failed to get resource");
		return ret;
	}
	wlfs_lock(&resource);
	ret = wres_get_args(&resource, addr, inlen, outlen, &args);
	if (-EAGAIN == ret)
		goto wait;
	else if (ret)
		goto out;
	ret = args.request(&args);
	if (ret)
		goto out;
wait:
	ret = wres_wait(&args);
	wres_put_args(&args);
out:
	wlfs_unlock(&resource);
	return ret ? ret : -EOK;
}


int main(int argc, char *argv[])
{
	umask(0);
	if (wlfs_init(argv[1])) {
		wln_log("failed to initialize wlfs");
		return -EINVAL;
	}
	return fuse_main(argc, argv, &wlfs_oper, NULL);
}
