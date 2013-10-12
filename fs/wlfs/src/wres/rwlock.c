/*      rwlock.c
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

#include "rwlock.h"

int wres_rwlock_stat = 0;
wres_rwlock_group_t wres_rwlock_group[WRES_RWLOCK_GROUP_SIZE];

void wres_rwlock_init()
{
	int i;
	
	if (wres_rwlock_stat & WRES_STAT_INIT)
		return;
	
	for (i = 0; i < WRES_RWLOCK_GROUP_SIZE; i++) {
		pthread_mutex_init(&wres_rwlock_group[i].mutex, NULL);
		wres_rwlock_group[i].head = rbtree_create();
	}
	wres_rwlock_stat |= WRES_STAT_INIT;
}


static inline int wres_rwlock_compare(char *l1, char *l2)
{
	wres_rwlock_desc_t *desc1 = (wres_rwlock_desc_t *)l1;
	wres_rwlock_desc_t *desc2 = (wres_rwlock_desc_t *)l2;
	
	return memcmp(desc1->entry, desc2->entry, sizeof(((wres_rwlock_desc_t *)0)->entry));
}


static inline unsigned long wres_rwlock_hash(wres_rwlock_desc_t *desc)
{
	unsigned long *ent = desc->entry;
	
	return (ent[0] ^ ent[1]) % WRES_RWLOCK_GROUP_SIZE;
}


static inline void wres_rwlock_get_desc(wresource_t *resource, wres_rwlock_desc_t *desc)
{
	desc->entry[0] = resource->key;
	desc->entry[1] = resource->cls;
}


static inline wres_rwlock_t *wres_rwlock_alloc(wres_rwlock_desc_t *desc)
{
	wres_rwlock_t *lock = (wres_rwlock_t *)malloc(sizeof(wres_rwlock_t));
	
	if (!lock)
		return NULL;
	
	memcpy(&lock->desc, desc, sizeof(wres_rwlock_desc_t));
	pthread_rwlock_init(&lock->lock, NULL);
	INIT_LIST_HEAD(&lock->list);
	return lock;
}


static inline wres_rwlock_t *wres_rwlock_lookup(wres_rwlock_group_t *group, wres_rwlock_desc_t *desc)
{
	return rbtree_lookup(group->head, (void *)desc, wres_rwlock_compare);
}


static inline void wres_rwlock_insert(wres_rwlock_group_t *group, wres_rwlock_t *lock)
{
	rbtree_insert(group->head, (void *)&lock->desc, (void *)lock, wres_rwlock_compare);
}


static inline wres_rwlock_t *wres_rwlock_get(wresource_t *resource) 
{
	wres_rwlock_desc_t desc;
	wres_rwlock_group_t *grp;
	wres_rwlock_t *lock = NULL;
	
	wres_rwlock_get_desc(resource, &desc);
	grp = &wres_rwlock_group[wres_rwlock_hash(&desc)];
	pthread_mutex_lock(&grp->mutex);
	lock = wres_rwlock_lookup(grp, &desc);
	if (!lock) {
		lock = wres_rwlock_alloc(&desc);
		if (!lock) {
			wres_print_err(resource, "no memory");
			goto out;
		}
		wres_rwlock_insert(grp, lock);
	}
out:
	pthread_mutex_unlock(&grp->mutex);
	return lock;
}


int wres_rwlock_rdlock(wresource_t *resource)
{	
	wres_rwlock_t *lock;
	
	if (!(wres_rwlock_stat & WRES_STAT_INIT))
		return -EINVAL;
	
	lock = wres_rwlock_get(resource);
	if (!lock)
		return -ENOENT;
	
	pthread_rwlock_rdlock(&lock->lock);
	wres_print_rwlock(resource);
	return 0; 
}


int wres_rwlock_wrlock(wresource_t *resource)
{	
	wres_rwlock_t *lock;
	
	if (!(wres_rwlock_stat & WRES_STAT_INIT))
		return -EINVAL;
	
	lock = wres_rwlock_get(resource);
	if (!lock)
		return -ENOENT;
	
	pthread_rwlock_wrlock(&lock->lock);
	wres_print_rwlock(resource);
	return 0; 
}


void wres_rwlock_unlock(wresource_t *resource)
{
	wres_rwlock_t *lock;
	
	if (!(wres_rwlock_stat & WRES_STAT_INIT)) 
		return;

	lock = wres_rwlock_get(resource);
	if (!lock)
		return;
	pthread_rwlock_unlock(&lock->lock);
	wres_print_rwlock(resource);
}
