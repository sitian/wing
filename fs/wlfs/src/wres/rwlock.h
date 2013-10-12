#ifndef _RWLOCK_H
#define _RWLOCK_H

#include <list.h>
#include <errno.h>
#include <pthread.h>
#include "resource.h"
#include "rbtree.h"
#include "trace.h"

#define WRES_RWLOCK_GROUP_SIZE		1024

#ifdef DEBUG_RWLOCK
#define WRES_PRINT_RWLOCK
#endif

#ifdef WRES_PRINT_RWLOCK
#define wres_print_rwlock wres_show
#else
#define wres_print_rwlock(...) do {} while (0)
#endif

typedef struct wres_rwlock_desc {
	unsigned long entry[2];
} wres_rwlock_desc_t;

typedef struct wres_rwlock_group {
	pthread_mutex_t mutex;
	rbtree head;
} wres_rwlock_group_t;

typedef struct wres_rwlock {
	wres_rwlock_desc_t desc;
	pthread_rwlock_t lock;
	struct list_head list;
} wres_rwlock_t;

void wres_rwlock_init();
void wres_rwlock_release();
void wres_rwlock_unlock(wresource_t *resource);

int wres_rwlock_rdlock(wresource_t *resource);
int wres_rwlock_wrlock(wresource_t *resource);

#endif
