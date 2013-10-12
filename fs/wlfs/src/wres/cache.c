/*      cache.c
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

#include "cache.h"

int wres_cache_stat = 0;
wres_cache_group_t wres_cache_group[WRES_CACHE_GROUP_SIZE];

void wres_cache_init()
{
	int i;
	
	if (wres_cache_stat & WRES_STAT_INIT) 
		return;
	
	for (i = 0; i < WRES_CACHE_GROUP_SIZE; i++) {
		pthread_mutex_init(&wres_cache_group[i].mutex, NULL);
		wres_cache_group[i].head = rbtree_create();
		INIT_LIST_HEAD(&wres_cache_group[i].list);
		wres_cache_group[i].count = 0;
	}
	wres_cache_stat |= WRES_STAT_INIT;
}


static inline int wres_cache_compare(char *c1, char *c2)
{
	wres_cache_desc_t *desc1 = (wres_cache_desc_t *)c1;
	wres_cache_desc_t *desc2 = (wres_cache_desc_t *)c2;
	
	return memcmp(desc1->entry, desc2->entry, sizeof(((wres_cache_desc_t *)0)->entry));
}


static inline unsigned long wres_cache_hash(wres_cache_desc_t *desc)
{
	unsigned long *ent = desc->entry;
	
	return (ent[0] ^ ent[1]) % WRES_CACHE_GROUP_SIZE;
}


static inline void wres_cache_get_desc(wresource_t *resource, wres_cache_desc_t *desc)
{
	desc->entry[0] = resource->key;
	desc->entry[1] = resource->cls;
}


static inline wres_cache_t *wres_cache_alloc(wres_cache_desc_t *desc, size_t len)
{
	wres_cache_t *cache;

	if (!len)
		return NULL;
	
	cache = (wres_cache_t *)malloc(sizeof(wres_cache_t));
	if (!cache)
		return NULL;
	
	cache->buf = malloc(len);
	if (!cache->buf) {
		free(cache);
		return NULL;
	}
	cache->len = len;
	INIT_LIST_HEAD(&cache->list);
	memcpy(&cache->desc, desc, sizeof(wres_cache_desc_t));
	return cache ;
}


static inline wres_cache_t *wres_cache_lookup(wres_cache_group_t *group, wres_cache_desc_t *desc)
{
	return rbtree_lookup(group->head, desc, wres_cache_compare);
}


static inline void wres_cache_free(wres_cache_group_t *group, wres_cache_t *cache)
{
	rbtree_delete(group->head, (void *)&cache->desc, wres_cache_compare);
	list_del(&cache->list);
	free(cache->buf);
	free(cache);
	group->count--;
}


static inline void wres_cache_insert(wres_cache_group_t *group, wres_cache_t *cache)
{
	rbtree_insert(group->head, &cache->desc, (void *)cache, wres_cache_compare);
	list_add(&cache->list, &group->list);
	group->count++;
}

int wres_cache_realloc(wres_cache_t *cache, size_t len)
{
	if (len > cache->len) {
		cache->buf = realloc(cache->buf, len);
		if (!cache->buf)
			return -EINVAL;
		cache->len = len;
	}
	return 0;
}


int wres_cache_write(wresource_t *resource, char *buf, size_t len)
{
	int ret = 0;
	wres_cache_t *cache;
	wres_cache_desc_t desc;
	wres_cache_group_t *grp;
	
	if (!(wres_cache_stat & WRES_STAT_INIT)) 
		return -EINVAL;

	wres_cache_get_desc(resource, &desc);
	grp = &wres_cache_group[wres_cache_hash(&desc)];
	pthread_mutex_lock(&grp->mutex);
	cache = wres_cache_lookup(grp, &desc);
	if (!cache) {
		cache = wres_cache_alloc(&desc, len);
		if (!cache) {
			wres_print_err(resource, "no memory");
			ret = -ENOMEM;
			goto out;
		}
		wres_cache_insert(grp, cache);
		if (WRES_CACHE_QUEUE_SIZE == grp->count)
			wres_cache_free(grp, list_entry(grp->list.prev, wres_cache_t, list));
	} else {
		ret = wres_cache_realloc(cache, len);
		if (ret) {
			wres_print_err(resource, "failed to write cache");
			goto out;
		}
	}
	memcpy(cache->buf, buf, len);
out:
	pthread_mutex_unlock(&grp->mutex);
	return ret;
}


int wres_cache_read(wresource_t *resource, char *buf, size_t len)
{
	int ret = 0;
	wres_cache_desc_t desc;
	wres_cache_group_t *grp;
	wres_cache_t *cache = NULL;
	
	if (!(wres_cache_stat & WRES_STAT_INIT)) 
		return -EINVAL;
	
	wres_cache_get_desc(resource, &desc);
	grp = &wres_cache_group[wres_cache_hash(&desc)];
	pthread_mutex_lock(&grp->mutex);
	cache = wres_cache_lookup(grp, &desc);
	if (!cache) {
		ret = -ENOENT;
		goto out;
	}
	if (len > cache->len) {
		ret = -EINVAL;
		goto out;
	} else if (len > 0) {
		list_del(&cache->list);
		list_add(&cache->list, &grp->list);
		memcpy(buf, cache->buf, len);
	}
out:
	pthread_mutex_unlock(&grp->mutex);
	return ret;
}


void wres_cache_flush(wresource_t *resource)
{
	wres_cache_t *cache;
	wres_cache_desc_t desc;
	wres_cache_group_t *grp;
	
	if (!(wres_cache_stat & WRES_STAT_INIT)) 
		return;
	
	wres_cache_get_desc(resource, &desc);
	grp = &wres_cache_group[wres_cache_hash(&desc)];
	pthread_mutex_lock(&grp->mutex);
	cache = wres_cache_lookup(grp, &desc);
	if (cache)
		wres_cache_free(grp, cache);
	pthread_mutex_unlock(&grp->mutex);
}


void wres_cache_release()
{
	int i, j;
	wres_cache_group_t *grp = wres_cache_group;
	
	if (!(wres_cache_stat & WRES_STAT_INIT)) 
		return;
	
	for (i = 0; i < WRES_CACHE_GROUP_SIZE; i++) {
		pthread_mutex_lock(&grp->mutex);
		for (j = 0; j < grp->count; j++)
			wres_cache_free(grp, list_entry(grp->list.next, wres_cache_t, list));
		pthread_mutex_unlock(&grp->mutex);
		grp++;
	}
}
