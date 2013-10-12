#ifndef _EVENT_H
#define _EVENT_H

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "resource.h"
#include "rbtree.h"

#define WRES_EVENT_GROUP_SIZE			1024

typedef struct wres_event_desc {
	unsigned long entry[4];
} wres_event_desc_t;

typedef struct wres_event {
	wres_event_desc_t desc;
	pthread_cond_t cond;
	size_t length;
	char *buf;
} wres_event_t;

typedef struct wres_event_group {
	pthread_mutex_t mutex;
	rbtree head;
} wres_event_group_t;

void wres_event_init();
int wres_event_set(wresource_t *resource, char *buf, size_t length);
int wres_event_wait(wresource_t *resource, char *buf, size_t length, struct timespec *timeout);

#endif
