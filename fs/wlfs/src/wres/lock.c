/*      lock.c
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

#include "lock.h"

int wres_lock_stat = 0;
wres_lock_group_t wres_lock_group[WRES_LOCK_GROUP_SIZE];

void wres_lock_init()
{
	int i;
	
	if (wres_lock_stat & WRES_STAT_INIT)
		return;
	
	for (i = 0; i < WRES_LOCK_GROUP_SIZE; i++) {
		pthread_mutex_init(&wres_lock_group[i].mutex, NULL);
		wres_lock_group[i].head = rbtree_create();
	}
	wres_lock_stat |= WRES_STAT_INIT;
}


static inline int wres_lock_compare(char *l1, char *l2)
{
	wres_lock_desc_t *desc1 = (wres_lock_desc_t *)l1;
	wres_lock_desc_t *desc2 = (wres_lock_desc_t *)l2;
	
	return memcmp(desc1->entry, desc2->entry, sizeof(((wres_lock_desc_t *)0)->entry));
}


static inline unsigned long wres_lock_hash(wres_lock_desc_t *desc)
{
	unsigned long *ent = desc->entry;
	
	return (ent[0] ^ ent[1] ^ ent[2] ^ ent[3]) % WRES_LOCK_GROUP_SIZE;
}


static inline void wres_lock_get_desc(wresource_t *resource, wres_lock_desc_t *desc)
{
	desc->entry[0] = resource->key;
	desc->entry[1] = resource->cls;	
	if (wres_get_op(resource) == WRES_OP_SHMFAULT) {
		desc->entry[2] = resource->owner;
		desc->entry[3] = wres_get_off(resource);
	} else {
		desc->entry[2] = 0;
		desc->entry[3] = 0;
	}
}


static inline wres_lock_t *wres_lock_alloc(wres_lock_desc_t *desc)
{
	wres_lock_t *lock = (wres_lock_t *)malloc(sizeof(wres_lock_t));
	
	if (!lock)
		return NULL;
	
	lock->count = 0;
	memcpy(&lock->desc, desc, sizeof(wres_lock_desc_t));
	pthread_mutex_init(&lock->mutex, NULL);
	pthread_cond_init(&lock->cond, NULL);
	INIT_LIST_HEAD(&lock->list);
	return lock;
}


static inline wres_lock_t *wres_lock_lookup(wres_lock_group_t *group, wres_lock_desc_t *desc)
{
	return rbtree_lookup(group->head, (void *)desc, wres_lock_compare);
}


static inline void wres_lock_insert(wres_lock_group_t *group, wres_lock_t *lock)
{
	rbtree_insert(group->head, (void *)&lock->desc, (void *)lock, wres_lock_compare);
}


static inline wres_lock_t *wres_lock_get(wresource_t *resource) 
{
	wres_lock_desc_t desc;
	wres_lock_group_t *grp;
	wres_lock_t *lock = NULL;
	
	wres_lock_get_desc(resource, &desc);
	grp = &wres_lock_group[wres_lock_hash(&desc)];
	pthread_mutex_lock(&grp->mutex);
	lock = wres_lock_lookup(grp, &desc);
	if (!lock) {
		lock = wres_lock_alloc(&desc);
		if (!lock) {
			wres_print_err(resource, "no memory");
			goto out;
		}
		wres_lock_insert(grp, lock);
	}
	pthread_mutex_lock(&lock->mutex);
	lock->count++;
out:
	pthread_mutex_unlock(&grp->mutex);
	return lock;
}


static inline void wres_lock_free(wres_lock_group_t *group, wres_lock_t *lock)
{
	rbtree_delete(group->head, (void *)&lock->desc, wres_lock_compare);
	pthread_mutex_destroy(&lock->mutex); 
	pthread_cond_destroy(&lock->cond);
	free(lock);
}


wres_lock_t *wres_lock_top(wresource_t *resource)
{
	if (!(wres_lock_stat & WRES_STAT_INIT))
		return NULL;
	
	wres_print_lock(resource);
	return wres_lock_get(resource);
}


void wres_unlock_top(wres_lock_t *lock)
{
	if (lock->count > 0)
		lock->count--;
	pthread_mutex_unlock(&lock->mutex);
}


int wres_lock_buttom(wres_lock_t *lock)
{
	wres_lock_list_t *list;
	pthread_t tid = pthread_self();
	
	if (!(wres_lock_stat & WRES_STAT_INIT))
		return -EINVAL;
	
	list = malloc(sizeof(wres_lock_list_t));
	if (!list) {
		wres_log("no memory");
		return -ENOMEM;
	}
	list->tid = tid;
	list_add_tail(&list->list, &lock->list);
	if (lock->count > 1) {
		wres_lock_list_t *curr;
		
		do {
			pthread_cond_wait(&lock->cond, &lock->mutex);
			curr = list_entry(lock->list.next, wres_lock_list_t, list);
		} while (curr->tid != tid);
	}
	pthread_mutex_unlock(&lock->mutex);
	return 0; 
}


int wres_lock(wresource_t *resource)
{	
	int ret = 0;
	wres_lock_t *lock;
	
	if (!(wres_lock_stat & WRES_STAT_INIT))
		return -EINVAL;
	
	lock = wres_lock_get(resource);
	if (!lock)
		return -ENOENT;
	
	if (lock->count > 1)
		pthread_cond_wait(&lock->cond, &lock->mutex);
	pthread_mutex_unlock(&lock->mutex);
	wres_print_lock(resource);
	return ret; 
}


int wres_lock_timeout(wresource_t *resource)
{	
	int ret = 0;
	wres_lock_t *lock;
	struct timespec time;

	if (!(wres_lock_stat & WRES_STAT_INIT))
		return -EINVAL;
	
	set_timeout(WRES_LOCK_TIMEOUT, &time);
	lock = wres_lock_get(resource);
	if (!lock) 
		return -ENOENT;
	
	if (lock->count > 1) {
		ret = pthread_cond_timedwait(&lock->cond, &lock->mutex, &time);
		if (ret)
			lock->count--;
	}
	pthread_mutex_unlock(&lock->mutex);
	wres_print_lock(resource);
	return ret > 0 ? -ret : ret; 
}


void wres_unlock(wresource_t *resource)
{
	int empty = 0;
	wres_lock_t *lock;
	wres_lock_desc_t desc;
	wres_lock_group_t *grp;
	
	if (!(wres_lock_stat & WRES_STAT_INIT)) 
		return;
	
	wres_lock_get_desc(resource, &desc);
	grp = &wres_lock_group[wres_lock_hash(&desc)];
	pthread_mutex_lock(&grp->mutex);
	lock = wres_lock_lookup(grp, &desc);
	if (!lock) {
		wres_print_err(resource, "cannot find the lock");
		goto out;
	}
	pthread_mutex_lock(&lock->mutex);
	if (lock->count > 0) {
		lock->count--;
		if (lock->count > 0) {
			if (list_empty(&lock->list))
				pthread_cond_signal(&lock->cond);
			else
				pthread_cond_broadcast(&lock->cond);
		} else
			empty = 1;
	
		if (!list_empty(&lock->list)) {
			wres_lock_list_t *curr = list_entry(lock->list.next, wres_lock_list_t, list);
			
			if (pthread_self() == curr->tid) {
				list_del(&curr->list);
				free(curr);
			} else
				wres_log("found a mismatched lock (tid=%lx)", pthread_self());
			
			if (empty && !list_empty(&lock->list))
				wres_log("failed to clear lock list");
		}
	}
	pthread_mutex_unlock(&lock->mutex);
	if (empty)
		wres_lock_free(grp, lock);
out:
	pthread_mutex_unlock(&grp->mutex);
	wres_print_lock(resource);
}
