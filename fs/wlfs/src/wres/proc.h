#ifndef _PROC_H
#define _PROC_H

#include "resource.h"
#include "trace.h"
#include "msg.h"
#include "sem.h"
#include "shm.h"
#include "tsk.h"
#include "util.h"

#ifdef DEBUG_PROC
#define WRES_PRINT_PROC
#define WRES_TRACE_PROC
#endif

#ifdef WRES_PRINT_PROC
#define wres_print_proc(req) do { \
	wresource_t *resource = &req->resource; \
	wres_show_resource(resource); \
	printf(", op=%s", wres_op2str(wres_get_op(resource))); \
	if (wres_get_op(resource) == WRES_OP_SHMFAULT) { \
		wres_shmfault_args_t *args = (wres_shmfault_args_t *)req->buf; \
		printf(", shmfault=%s", wres_shmfault2str(args->cmd)); \
	} \
	printf("\n"); \
} while(0)
#else
#define wres_print_proc(...) do {} while (0)
#endif

#ifdef WRES_TRACE_PROC
#define wres_trace_proc(req) wres_trace(req)
#else
#define wres_trace_proc(...) do {} while (0)
#endif

int wres_proc_direct(wres_args_t *args);
wres_reply_t *wres_proc(wres_req_t *req, int flags);

#endif
