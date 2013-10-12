#ifndef _PRIO_H
#define _PRIO_H

#include "stringio.h"
#include "resource.h"
#include "member.h"
#include "rwlock.h"
#include "trace.h"
#include "wait.h"
#include "util.h"

//#define WRES_PRIO_NOWAIT
#define WRES_PRIO_UPDATE_CONTROL

#define WRES_PRIO_NR_REPEATS		32
#define WRES_PRIO_NR_INTERVALS		256
#define WRES_PRIO_LOCK_GROUP_SIZE	1024
#define WRES_PRIO_PERIOD			200000		// usec
#define WRES_PRIO_SYNC_INTERVAL		5000000	// usec
#define WRES_PRIO_MAX				WRES_MEMBER_MAX

#ifdef DEBUG_PRIO
#define WRES_PRINT_PRIO_CHECK
#define WRES_PRINT_PRIO_LOCK
#define WRES_PRINT_PRIO_UNLOCK

#ifdef DEBUG_SHOW_MORE
#define WRES_PRINT_PRIO_CREATE
#define WRES_PRINT_PRIO_SYNC_TIME
#endif
#endif

#define wres_prio_time(prio, time) ((time) + (prio)->t_off)
#define wres_prio_interval(time) (((time) / WRES_PRIO_PERIOD / WRES_PRIO_NR_REPEATS) % WRES_PRIO_NR_INTERVALS)

typedef struct wres_prio_lock_desc {
	unsigned long entry[3];
} wres_prio_lock_desc_t;

typedef struct wres_prio_lock_group {
	pthread_mutex_t mutex;
	rbtree head;
} wres_prio_lock_group_t;

typedef struct wres_prio_lock {
	wres_prio_lock_desc_t desc;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int count;
} wres_prio_lock_t;

typedef struct wres_prio {
	wres_id_t id;
	wres_time_t t_off;
	wres_time_t t_sync;
	wres_time_t t_update;
#ifdef MODE_EXPLICIT_PRIORITY
	wres_time_t t_live;
	int val;
#endif
} wres_prio_t;

#ifdef WRES_PRINT_PRIO_LOCK
#define wres_print_prio_lock wres_show
#else
#define wres_print_prio_lock(...) do {} while (0)
#endif

#ifdef WRES_PRINT_PRIO_UNLOCK
#define wres_print_prio_unlock wres_show
#else
#define wres_print_prio_unlock(...) do {} while (0)
#endif

#ifdef WRES_PRINT_PRIO_CREATE
#define wres_print_prio_create(resource, offset) do { \
	wres_show_owner(resource); \
	printf("key=%d, t_off=%lld\n", resource->key, offset); \
} while (0)
#else
#define wres_print_prio_create(...) do {} while (0)
#endif

#ifdef WRES_PRINT_PRIO_SYNC_TIME
#define wres_print_prio_sync_time(resource, off) do { \
	wres_show_resource(resource); \
	printf(", t_off=%lld\n", off); \
} while (0)
#else
#define wres_print_prio_sync_time(...) do {} while (0) 
#endif

#ifdef WRES_PRINT_PRIO_CHECK
#define wres_print_prio_check(req) do { \
	wresource_t *resource = &(req)->resource; \
	wres_show_resource(resource); \
	if (WRES_OP_SHMFAULT == wres_get_op(resource)) { \
		wres_shmfault_args_t *args = (wres_shmfault_args_t *)req->buf; \
		printf(" [%d]", args->index);\
	} \
	printf("\n"); \
} while (0)
#else
#define wres_print_prio_check(...) do {} while (0) 
#endif

void wres_prio_init();
void wres_prio_clear(wres_req_t *req);

int wres_prio_check(wres_req_t *req);
int wres_prio_set_idle(wresource_t *resource);
int wres_prio_set_busy(wresource_t *resource);
int wres_prio_sync_time(wresource_t *resource);
int wres_prio_create(wresource_t *resource, wres_time_t off);

#ifdef MODE_EXPLICIT_PRIORITY
int wres_prio_check_live(wresource_t *resource, int *prio, wres_time_t *live);
#endif

#endif
