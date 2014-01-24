#ifndef _RAP_H
#define _RAP_H

#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include "dipc.h"

#define KEY_START		3236
#define KEY_END			3237
#define KEY_AREA		3500
#define KEY_LOCK		3600

#define RAP_CONT		1
#define RAP_ROUNDS		10000

#define RAP_AREAS		15

typedef struct rap_lock {
	shmlock_t *lock;
	int desc;
} rap_lock_t;

typedef struct rap_area {
	rap_lock_t lock;
	char *buf;
	int desc;
	int size;
} rap_area_t;

typedef struct rap_areas {
	int desc;
	int flags;
	int count;
	char *buf;
	rap_area_t areas[RAP_AREAS];
} rap_areas_t; 

#endif
