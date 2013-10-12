/*      event.c
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

#include "event.h"

int wres_event_stat = 0;
wres_event_group_t wres_event_group[WRES_EVENT_GROUP_SIZE];

void wres_event_init()
{
	int i;
	
	if (wres_event_stat & WRES_STAT_INIT)
		return;
		
	for (i = 0; i < WRES_EVENT_GROUP_SIZE; i++) {
		pthread_mutex_init(&wres_event_group[i].mutex, NULL);
		wres_event_group[i].head = rbtree_create();
	}
	wres_event_stat |= WRES_STAT_INIT;
}


static inline unsigned long wres_event_hash(wres_event_desc_t *desc) 
{
	unsigned long *ent = desc->entry;
	
	return (ent[0] ^ ent[1] ^ ent[2] ^ ent[3]) % WRES_EVENT_GROUP_SIZE;
}


static inline int wres_event_compare(char *e1, char *e2)
{
	wres_event_desc_t *desc1 = (wres_event_desc_t *)e1;
	wres_event_desc_t *desc2 = (wres_event_desc_t *)e2;
	
	return memcmp(desc1->entry, desc2->entry, sizeof(((wres_event_desc_t *)0)->entry));
}


static inline void wres_event_get_desc(wresource_t *resource, wres_event_desc_t *desc)
{
	desc->entry[0] = resource->key;
	desc->entry[1] = resource->cls;
	desc->entry[2] = resource->owner;
	desc->entry[3] = wres_get_off(resource);
}


static wres_event_t *wres_event_alloc(wres_event_desc_t *desc)
{
	wres_event_t *event = (wres_event_t *)malloc(sizeof(wres_event_t));
	
	if (!event)
		return NULL;
	
	event->buf = NULL;
	event->length = 0;
	memcpy(&event->desc, desc, sizeof(wres_event_desc_t));
	pthread_cond_init(&event->cond, NULL);
	return event;
}


static inline wres_event_t *wres_event_lookup(wres_event_group_t *group, wres_event_desc_t *desc)
{
	return rbtree_lookup(group->head, (void *)desc, wres_event_compare);
}


static inline void wres_event_insert(wres_event_group_t *group, wres_event_t *event)
{
	rbtree_insert(group->head, (void *)&event->desc, (void *)event, wres_event_compare);
}


static inline void wres_event_free(wres_event_group_t *group, wres_event_t *event)
{
	rbtree_delete(group->head, (void *)&event->desc, wres_event_compare);
	pthread_cond_destroy(&event->cond);
	free(event);
}


int wres_event_wait(wresource_t *resource, char *buf, size_t length, struct timespec *timeout)
{
	int ret = 0;
	wres_event_t *event;
	wres_event_desc_t desc;
	wres_event_group_t *grp;

	if (!(wres_event_stat & WRES_STAT_INIT))
		return -EINVAL;

	wres_event_get_desc(resource, &desc);
	grp = &wres_event_group[wres_event_hash(&desc)];
	pthread_mutex_lock(&grp->mutex);
	event = wres_event_lookup(grp, &desc);
	if (event) {
		if (event->buf) {
			if (event->length == length)
				memcpy(buf, event->buf, length);
			free(event->buf);
		}
	} else {
		event = wres_event_alloc(&desc);
		if (!event) {
			wres_print_err(resource, "no memory");
			ret = -EINVAL;
			goto out;
		}
		event->buf = buf;
		event->length = length;
		wres_event_insert(grp, event);
		if (timeout)
			ret = pthread_cond_timedwait(&event->cond, &grp->mutex, timeout);
		else
			ret = pthread_cond_wait(&event->cond, &grp->mutex);
	}
	wres_event_free(grp, event);
out:
	pthread_mutex_unlock(&grp->mutex);
	return ret > 0 ? -ret : ret;
}


int wres_event_set(wresource_t *resource, char *buf, size_t length)
{
	int ret = 0;
	wres_event_t *event;
	wres_event_desc_t desc;
	wres_event_group_t *grp;
	
	if (!(wres_event_stat & WRES_STAT_INIT)) 
		return -EINVAL;

	wres_event_get_desc(resource, &desc);
	grp = &wres_event_group[wres_event_hash(&desc)];
	pthread_mutex_lock(&grp->mutex);
	event = wres_event_lookup(grp, &desc);
	if (!event) {
		char *tmp = malloc(length);
		
		if (!tmp) {
			wres_print_err(resource, "no memory");
			ret = -ENOMEM;
			goto out;
		}
		event = wres_event_alloc(&desc);
		if (!event) {
			wres_print_err(resource, "no memory");
			ret = -ENOMEM;
			free(tmp);
			goto out;
		}
		event->buf = tmp;
		memcpy(event->buf, buf, length);
		event->length = length;
		wres_event_insert(grp, event);
	} else {
		if (event->buf && (event->length == length))
			memcpy(event->buf, buf, length);
		pthread_cond_signal(&event->cond);
	}
out:
	pthread_mutex_unlock(&grp->mutex);
	return ret;
}
