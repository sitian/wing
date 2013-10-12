/*      proc.c
 *      
 *      Copyright (C) 2013 Yi-Wei Ci <ciyiwei@hotmail.com>
 *      
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#include "proc.h"

wres_reply_t *wres_proc_msg(wres_req_t *req, int flags)
{	
	wresource_t *resource = &req->resource;
	wres_op_t op = wres_get_op(resource);
	
	switch(op) {
	case WRES_OP_MSGSND:
		return wres_msg_msgsnd(req, flags);
	case WRES_OP_MSGRCV:
		return wres_msg_msgrcv(req, flags);
	case WRES_OP_MSGCTL:
		return wres_msg_msgctl(req, flags);
	default:
		wres_print_err(resource, "invalid operation (op=%d)", op);
		break;
	}
	return NULL;
}


wres_reply_t *wres_proc_sem(wres_req_t *req, int flags)
{	
	wresource_t *resource = &req->resource;
	wres_op_t op = wres_get_op(resource);
	
	switch(op) {
	case WRES_OP_SEMOP:
		return wres_sem_semop(req, flags);
	case WRES_OP_SEMCTL:
		return wres_sem_semctl(req, flags);
	case WRES_OP_SEMEXIT:
		wres_sem_exit(req);
		break;
	default:
		wres_print_err(resource, "invalid operation (op=%d)", op);
		break;
	}
	return NULL;
}


wres_reply_t *wres_proc_shm(wres_req_t *req, int flags)
{	
	wresource_t *resource = &req->resource;
	wres_op_t op = wres_get_op(resource);
	
	switch(op) {
	case WRES_OP_SHMFAULT:
		return wres_shm_fault(req, flags);
	case WRES_OP_SHMCTL:
		return wres_shm_shmctl(req, flags);
	default:
		wres_print_err(resource, "invalid operation (op=%d)", op);
		break;
	}
	return NULL;
}


wres_reply_t *wres_proc_generic(wres_req_t *req, int flags)
{
	wresource_t *resource = &req->resource;
	wres_op_t op = wres_get_op(resource);
	
	switch (op) {
	case WRES_OP_SIGNIN:
		return wres_signin(req, flags);
	case WRES_OP_SIGNOUT:
		return wres_signout(req, flags);
	case WRES_OP_SYNCTIME:
		return wres_synctime(req, flags);
	case WRES_OP_CANCEL:
		return wres_cancel(req, flags);
	case WRES_OP_REPLY:
		return wres_reply(req, flags);
	default:
		wres_print_err(resource, "invalid operation (op=%d)", op);
		return NULL;
	}
}


wres_reply_t *wres_proc(wres_req_t *req, int flags)
{
	wresource_t *resource = &req->resource;
	wres_op_t op = wres_get_op(resource);
	wres_cls_t cls = resource->cls;
	
	wres_print_proc(req);
	wres_trace_proc(req);

	if (wres_op_generic(op))
		return wres_proc_generic(req, flags);
	
	switch(cls) {
	case WRES_CLS_MSG:
		return wres_proc_msg(req, flags);
	case WRES_CLS_SEM:
		return wres_proc_sem(req, flags);
	case WRES_CLS_SHM:
		return wres_proc_shm(req, flags);
	default:
		wres_print_err(resource, "invalid class");
		break;
	}
	return NULL;
}


int wres_proc_direct(wres_args_t *args)
{ 
	wresource_t *resource = &args->resource;
	wres_op_t op = wres_get_op(resource);
	
	switch (op) {
	case WRES_OP_MSGGET:
		return wres_ipcget(resource, wres_msg_create, NULL);
	case WRES_OP_SEMGET:
		return wres_ipcget(resource, wres_sem_create, NULL);
	case WRES_OP_SHMGET:
		return wres_ipcget(resource, wres_shm_create, wres_shm_init);
	case WRES_OP_SHMPUT:
		return wres_ipcput(resource);
	case WRES_OP_TSKGET:
		return wres_tskget(resource, (int *)args->out);
	case WRES_OP_TSKPUT:
		return wres_tskput(resource);
	case WRES_OP_RELEASE:
		wres_release();
		break;
	case WRES_OP_PROBE:
		wres_log("PROBE: id=%d, val1=%d, val2=%d",
		         wres_get_id(resource),
		         resource->entry[WRES_POS_VAL1],
		         resource->entry[WRES_POS_VAL2]);
		break;
	default:
		wres_print_err(resource, "invalid operation (op=%d)", op);
		return -EINVAL;
	}
	return 0;
}
