#ifndef _MSG_H
#define _MSG_H

#include <sys/ipc.h>
#include <sys/msg.h>
#include "resource.h"
#include "record.h"
#include "redo.h"
#include "stringio.h"
#include "trace.h"
#include "util.h"

#define WRES_MSGMAX		8192   									/* <= INT_MAX */   /* max size of message (bytes) */
#define WRES_MSGMNI		16   									/* <= IPCMNI */     /* max # of msg queue identifiers */
#define WRES_MSGMNB		16384   								/* <= INT_MAX */   /* default max size of a message queue */
#define WRES_MSGTQL		WRES_MSGMNB            					/* number of system message headers */
#define WRES_MSGMAP		WRES_MSGMNB            					/* number of entries in message map */
#define WRES_MSGPOOL	(WRES_MSGMNI * WRES_MSGMNB / 1024)		/* size in kbytes of message pool */
#define WRES_MSGSSZ		16                						/* message segment size */
#define __MSGSEG ((WRES_MSGPOOL * 1024) / WRES_MSGSSZ) 			/* max no. of segments */
#define WRES_MSGSEG (__MSGSEG <= 0xffff ? __MSGSEG : 0xffff)

#define SEARCH_ANY			1
#define SEARCH_EQUAL		2
#define SEARCH_NOTEQUAL		3
#define SEARCH_LESSEQUAL	4

#ifdef DEBUG_MSG
#define WRES_PRINT_MSG
#endif

#ifdef WRES_PRINT_MSG
#define wres_print_msg wres_show
#else
#define wres_print_msg(...) do {} while (0)
#endif

typedef struct wres_msgsnd_args {
	long msgtyp;
	size_t msgsz;
	int msgflg;
} wres_msgsnd_args_t;

typedef struct wres_msgsnd_result {
	long retval;
} wres_msgsnd_result_t;

typedef struct wres_msgrcv_result {
	long retval;
} wres_msgrcv_result_t;

typedef struct wres_msgrcv_args {
	long msgtyp;
	size_t msgsz;
	int msgflg;
} wres_msgrcv_args_t;

typedef struct wres_msgctl_args {
	int cmd;
} wres_msgctl_args_t;

typedef struct wres_msgctl_result {
	long retval;
} wres_msgctl_result_t;

static inline int wres_msg_convert_mode(long *msgtyp, int msgflg)
{
	/*
	 *  find message of correct type.
	 *  msgtyp = 0 => get first.
	 *  msgtyp > 0 => get first message of matching type.
	 *  msgtyp < 0 => get message with least type must be < abs(msgtype).
	 */
	if (*msgtyp == 0)
		return SEARCH_ANY;
	if (*msgtyp < 0) {
		*msgtyp = -*msgtyp;
		return SEARCH_LESSEQUAL;
	}
	if (msgflg & MSG_EXCEPT)
		return SEARCH_NOTEQUAL;
	return SEARCH_EQUAL;
}

static inline int wres_msgtyp_cmp(long typ1, long typ2, int mode)
{
	switch (mode) {
	case SEARCH_ANY:
		return 1;
	case SEARCH_LESSEQUAL:
		if (typ1 <= typ2)
			return 1;
		break;
	case SEARCH_EQUAL:
		if (typ1 == typ2)
			return 1;
		break;
	case SEARCH_NOTEQUAL:
		if (typ1 != typ2)
			return 1;
		break;
	}
	return 0;
}

int wres_msg_create(wresource_t *resource);
wres_reply_t *wres_msg_msgsnd(wres_req_t *req, int flags);
wres_reply_t *wres_msg_msgrcv(wres_req_t *req, int flags);
wres_reply_t *wres_msg_msgctl(wres_req_t *req, int flags);

#endif
