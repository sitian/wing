#ifndef _LOCK_H
#define _LOCK_H

#include <list.h>
#include <errno.h>
#include <pthread.h>
#include "resource.h"
#include "rbtree.h"
#include "trace.h"

#define WRES_LOCK_GROUP_SIZE		1024
#define WRES_LOCK_TIMEOUT			1000000 // usec

#ifdef DEBUG_LOCK
#define WRES_PRINT_LOCK
#endif

#ifdef WRES_PRINT_LOCK
#define wres_print_lock wres_show
#else
#define wres_print_lock(...) do {} while (0)
#endif

typedef struct wres_lock_desc {
	unsigned long entry[4];
} wres_lock_desc_t;

typedef struct wres_lock_group {
	pthread_mutex_t mutex;
	rbtree head;
} wres_lock_group_t;

typedef struct wres_lock {
	wres_lock_desc_t desc;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	struct list_head list;
	int count;
} wres_lock_t;

typedef struct wres_lock_list {
	pthread_t tid;
	struct list_head list;
} wres_lock_list_t;

void wres_lock_init();
void wres_lock_release();
void wres_unlock_top(wres_lock_t *lock);
void wres_unlock(wresource_t *resource);
wres_lock_t *wres_lock_top(wresource_t *resource);
int wres_lock_timeout(wresource_t *resource);
int wres_lock_buttom(wres_lock_t *lock);
int wres_lock(wresource_t *resource);

#endif
