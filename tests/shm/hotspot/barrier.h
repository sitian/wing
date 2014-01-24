#ifndef _BARRIER_H
#define _BARRIER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "dipc.h"

#define BARRIER_KEY  		100
#define BARRIER_USER_KEY	1152

union semun {
	int val;                /* value for SETVAL */
	struct semid_ds *buf;   /* buffer for IPC_STAT & IPC_SET */
	unsigned short *array;  /* array for GETALL & SETALL */
	struct seminfo *__buf;  /* buffer for IPC_INFO */
};

void barrier_wait();
int barrier_init(int total);

#endif
