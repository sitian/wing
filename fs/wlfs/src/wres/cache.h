#ifndef _CACHE_H
#define _CACHE_H

#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <list.h>
#include "resource.h"
#include "rbtree.h"

#define WRES_CACHE_GROUP_SIZE 		1024
#define WRES_CACHE_QUEUE_SIZE 		1024

typedef struct wres_cache_desc {
	unsigned long entry[2];
} wres_cache_desc_t;

typedef struct wres_cache {
	wres_cache_desc_t desc;
	size_t len;
	struct list_head list;
	char *buf;
} wres_cache_t;

typedef struct wres_cache_group {
	pthread_mutex_t mutex;
	struct list_head list;
	rbtree head;
	unsigned long count;
} wres_cache_group_t;

void wres_cache_init();
void wres_cache_release();
void wres_cache_flush(wresource_t *resource);
int wres_cache_read(wresource_t *resource, char *buf, size_t len);
int wres_cache_write(wresource_t *resource, char *buf, size_t len);

#endif
