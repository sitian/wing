#ifndef _SEM_H
#define _SEM_H

#include <sys/ipc.h>
#include <sys/sem.h>
#include "resource.h"
#include "record.h"
#include "redo.h"
#include "stringio.h"
#include "trace.h"
#include "util.h"

#define SEM_NSEMS 20

#define WRES_SEMVMX			32767						/* <= 32767 semaphore maximum value */
#define WRES_SEMMNI			128							/* <= IPCMNI  max # of semaphore identifiers */
#define WRES_SEMMSL			250							/* <= 8 000 max num of semaphores per id */
#define WRES_SEMMNS			(WRES_SEMMNI * WRES_SEMMSL) /* <= INT_MAX max # of semaphores in system */
#define WRES_SEMOPM			32							/* <= 1 000 max num of ops per semop call */
#define WRES_SEMMNU  		WRES_SEMMNS					/* num of undo structures system wide */
#define WRES_SEMMAP			WRES_SEMMNS					/* # of entries in semaphore map */
#define WRES_SEMUME			WRES_SEMOPM					/* max num of undo entries per process */
#define WRES_SEMUSZ			20							/* sizeof struct sem_undo */
#define WRES_SEMAEM			WRES_SEMVMX					/* adjust on exit max value */

#ifdef DEBUG_SEM
#define WRES_PRINT_SEM
#define WRES_PRINT_SEMOP
#endif

#define wres_semundo_size(un) (sizeof(wres_semundo_t) + (un)->nsems * sizeof(short))
#define wres_sma_size(nsems) (sizeof(struct wres_sem_array ) + nsems * sizeof(struct wres_sem))

typedef short wres_semadj_t;

union semun {
    int              val;		/* Value for SETVAL */
    struct semid_ds *buf;		/* Buffer for IPC_STAT, IPC_SET */
    unsigned short  *array;		/* Array for GETALL, SETALL */
    struct seminfo  *__buf;		/* Buffer for IPC_INFO (Linux specific) */
};

typedef struct wres_semop_args {
	struct timespec timeout;
	short nsops;
	struct sembuf sops[0];
} wres_semop_args_t;

typedef struct wres_semop_result {
	long retval;
} wres_semop_result_t;

typedef struct wres_semctl_args {
	int semnum;
	int cmd;
} wres_semctl_args_t;

typedef struct wres_semctl_result {
	long retval;
} wres_semctl_result_t;

typedef struct wres_semundo {
	pid_t pid;
	unsigned long nsems;
	wres_semadj_t semadj[0];
} wres_semundo_t;

struct wres_sem {
	int		semval;		/* current value */
	pid_t	sempid;		/* pid of last operation */
};

struct wres_sem_array {
	struct ipc_perm sem_perm;	/* Ownership and permissions */
    time_t          sem_otime;	/* Last semop time */
    time_t          sem_ctime;	/* Last change time */
    unsigned short  sem_nsems;	/* No. of semaphores in set */
	struct wres_sem	sem_base[0];/* ptr to first semaphore in array */
};

#ifdef WRES_PRINT_SEM
#define wres_print_sem wres_show
#else
#define wres_print_sem(...) do {} while (0)
#endif

#ifdef WRES_PRINT_SEMOP
#define wres_print_semop(key, nsops, sem_num, semval, src) \
		wres_log("key=%d, nsops=%d, sem_num=%d, semval=%d, src=%d", key, nsops, sem_num, semval, src);
#else
#define wres_print_semop(...) do {} while (0)
#endif

int wres_sem_create(wresource_t *resource);
int wres_sem_is_owner(wresource_t *resource);
int wres_sem_get_args(wresource_t *resource, wres_args_t *args);

wres_reply_t *wres_sem_semop(wres_req_t *req, int flags);
wres_reply_t *wres_sem_semctl(wres_req_t *req, int flags);

void wres_sem_exit(wres_req_t *req);

#endif
