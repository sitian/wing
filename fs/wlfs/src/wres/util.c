/*      util.c
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

#include "util.h"

void wres_free_args(wres_args_t *args)
{
	if (args->buf) {
		munmap(args->buf, args->size);
		close(args->desc);
		args->buf = NULL;
	}
}


int wres_check_args(wresource_t *resource, unsigned long addr, size_t inlen, size_t outlen, wres_args_t *args)
{
	int ret = 0;
	size_t size;
	char *buf = NULL;

	size = max(inlen, outlen);
	bzero(args, sizeof(wres_args_t));
	args->resource = *resource;
	args->index = -1;
	
	if (addr && size) {
		int desc = memget(addr, size, &buf);

		if (desc < 0) {
			wres_print_err(resource, "failed to get mem");
			return -EINVAL;
		}
		
		args->desc = desc;
		args->outlen = outlen;
		args->inlen = inlen;
		args->size = size;
		args->buf = buf;
		args->out = buf;
		args->in = buf;
	}
	
	switch (resource->cls) {
	case WRES_CLS_SHM:
		ret = wres_shm_check_args(resource, args);
		break;
	default:
		break;
	}
	
	if (ret && (ret != -EAGAIN))
		wres_free_args(args);
	return ret;
}


int wres_get_args(wresource_t *resource, unsigned long addr, size_t inlen, size_t outlen, wres_args_t *args)
{
	int ret;
	wres_op_t op = wres_get_op(resource);

	ret = wres_check_args(resource, addr, inlen, outlen, args);
	if (ret)
		return ret;
	
	if (wres_op_direct(op))
		args->request = wres_proc_direct;
	else {
		if (op == WRES_OP_SHMFAULT)
			args->request = wres_shm_request;
		else
			args->request = wres_send_request;
	}
	
	switch (resource->cls) {
	case WRES_CLS_SHM:
		ret = wres_shm_get_args(resource, args);
		break;
	case WRES_CLS_SEM:
		ret = wres_sem_get_args(resource, args);
		break;
	}
	if (ret)
		wres_free_args(args);
	return ret;
}


void wres_put_args(wres_args_t *args)
{
	wresource_t *resource = &args->resource;
	
	if (WRES_CLS_SHM == resource->cls)
		wres_shm_put_args(args);
	wres_free_args(args);
}


int wres_signin_request(wresource_t *resource, wres_signin_result_t **result)
{
	int ret;
	size_t size = 0;
	wresource_t res = *resource;
	wres_signin_result_t *p = NULL;
	
	if (wres_member_is_public(resource)) {
		size  = sizeof(wres_signin_result_t) + WRES_MEMBER_MAX * sizeof(wres_id_t);
		p = (wres_signin_result_t *)malloc(size);
		if (!p) {
			wres_print_err(resource, "no memory");
			return -ENOMEM;
		}
	}
	wres_set_op(&res, WRES_OP_SIGNIN);
	ret = wres_ioctl(&res, NULL, 0, (char *)p, size, NULL);
	if (ret) {
		wres_print_err(resource, "failed to signin");
		goto out;
	}
	if (p && p->retval) {
		wres_print_err(resource, "failed to get members");
		ret = -EINVAL;
		goto out;
	}
	*result = p;
out:
	if (ret && p)
		free(p);
	return ret;
}


int wres_signout_request(wresource_t *resource)
{
	int ret;
	wresource_t res = *resource;
	wres_signout_result_t result;

	wres_set_op(&res, WRES_OP_SIGNOUT);
	ret = wres_ioctl(&res, NULL, 0, (char *)&result, sizeof(wres_signout_result_t), NULL);
	if (ret || (result.retval && (result.retval != -ENOOWNER))) {
		wres_print_err(resource, "failed to signout");
		return -EFAULT;
	}
	return 0;
}


int wres_synctime_request(wresource_t *resource, wres_time_t *off)
{
	int ret;
	wres_time_t start;
	wresource_t res = *resource;
	wres_synctime_result_t result;
	
	start = get_time();
	wres_set_op(&res, WRES_OP_SYNCTIME);
	ret = wres_ioctl(&res, NULL, 0, (char *)&result, sizeof(wres_synctime_result_t), NULL);
	if (ret) {
		wres_print_err(resource, "failed to get time");
		return ret;
	}
	ret = result.retval;
	if (!ret)
		*off = result.time - start - (get_time() - start) / 2;
	return ret;
}


int wres_ipcget(wresource_t *resource, func_ipc_create create, func_ipc_init init)
{
	int ret = 0;
	int owner = 0;
	int flags = wres_get_flags(resource);

	wres_rwlock_wrlock(resource);
	ret = wres_check_path(resource);
	if (ret) {
		wres_print_err(resource, "failed to create path");
		wres_rwlock_unlock(resource);
		return ret;
	}
	
	if (flags & IPC_CREAT) {
		ret = wres_new(resource);
		if (!ret)
			owner = 1;
		else if ((-EEXIST == ret) && !(flags & IPC_EXCL))
			ret = 0;
	} else
		ret = wres_access(resource);

	if (ret)
		goto out;
	
	if (owner) {
		if (create) {
			ret = create(resource);
			if (ret) {
				wres_print_err(resource, "failed to create");
				goto out;
			}
		}
		if (wres_member_new(resource) < 0) {
			ret = -EFAULT;
			goto out;
		}
	} else if (!owner) {
		wres_signin_result_t *result = NULL;
		
		ret = wres_signin_request(resource, &result);
		if (ret) {
			wres_print_err(resource, "failed to sign in");
			goto out;
		}
		if (result) {
			ret = wres_member_save(resource, result->list, result->total);
			free(result);
			if (ret) {
				wres_print_err(resource, "failed to save member list");
				goto out;
			}
		}
	}
	if (init) {
		ret = init(resource);
		if (ret) {
			wres_print_err(resource, "failed to initialize");
			goto out;
		}
	}
out:
	//Todo: clean up
	if (ret) {
		if (owner)
			wres_free(resource);
		wres_clear_path(resource);
	}
	wres_rwlock_unlock(resource);
	return ret;
}


wres_reply_t *wres_cancel(wres_req_t *req, int flags)
{
	int ret;
	char path[WRES_PATH_MAX];
	wresource_t res = req->resource;
	wres_index_t index = wres_get_off(&res);
	wres_cancel_args_t *args = (wres_cancel_args_t *)req->buf;
	
	wres_set_op(&res, args->op);
	wres_get_record_path(&res, path);
	ret = wres_record_remove(path, index);
	if (ret)
		return wres_reply_err(ret);
	return NULL;
}


wres_reply_t *wres_reply(wres_req_t *req, int flags)
{
	int ret = wres_event_set(&req->resource, req->buf, req->length);
	
	if (ret)
		return wres_reply_err(ret);
	return NULL;
}


wres_reply_t *wres_synctime(wres_req_t *req, int flags)
{
	int ret = 0;
	wres_reply_t *reply = NULL;
	wres_synctime_result_t *result;

	reply = wres_reply_alloc(sizeof(wres_synctime_result_t));
	if (!reply) {
		ret = -ENOMEM;
		goto out;
	}
	result = wres_result_check(reply, wres_synctime_result_t);
	result->time = get_time();
	result->retval = 0;
out:
	if (ret)
		return wres_reply_err(ret);
	return reply;
}


wres_reply_t *wres_signin(wres_req_t *req, int flags)
{
	int total;
	int ret = 0;
	int public = 0;
	wres_reply_t *reply = NULL;
	wresource_t *resource = &req->resource;
	
	if (wres_member_is_public(resource) && wres_is_owner(resource)) {
		ret = wres_member_notify(req);
		if (ret) {
			wres_print_err(resource, "failed to signin");
			goto out;
		}
		public = 1;
	}
	
	total = wres_member_new(resource);
	if (total < 0) {
		wres_print_err(resource, "failed to add member");
		ret = -EINVAL;
		goto out;
	}
	
	if (public) {
		wres_signin_result_t *result;
		size_t size = sizeof(wres_signin_result_t) + total * sizeof(wres_id_t);
		
		reply = wres_reply_alloc(size);
		if (!reply) {
			wres_print_err(resource, "no memory");
			ret = -ENOMEM;
			goto out;
		}
		result = wres_result_check(reply, wres_signin_result_t);
		result->retval = 0;
		result->total = total;
		ret = wres_member_list(resource, result->list);
		if (ret) {
			wres_print_err(resource, "failed to get members");
			free(reply);
		}
	}
out:
	if (ret)
		return wres_reply_err(ret);
	return reply;
}


int wres_do_signout(wresource_t *resource)
{
	int ret = 0;
	int own = wres_is_owner(resource);
	int pub = wres_member_is_public(resource);
	
	if (own || pub) {
		ret = wres_member_delete(resource);
		if (ret) {
			wres_print_err(resource, "failed to delete this member");
			return ret;
		}
		if (own) {
			switch(resource->cls) {
			case WRES_CLS_SHM: 
				ret = wres_shm_destroy(resource);
				break;
			default:
				break;
			}
		}
	}
	return ret;
}


wres_reply_t *wres_signout(wres_req_t *req, int flags)
{
	int ret = wres_do_signout(&req->resource);

	if (ret)
		return wres_reply_err(ret);
	return NULL;
}


int wres_ioctl(wresource_t *resource, char *in, size_t inlen, char *out, size_t outlen, wres_id_t *to)
{
	return wln_ioctl(resource, in, inlen, out, outlen, to);
}


int wres_ioctl_async(wresource_t *resource, char *in, size_t inlen, char *out, size_t outlen, wres_id_t *to, pthread_t *tid)
{
	return wln_ioctl_async(resource, in, inlen, out, outlen, to, tid);
}


int wres_broadcast_request(wres_args_t *args)
{
	return wln_broadcast_request(args);
}


int wres_send_request(wres_args_t *args)
{
	return wln_send_request(args);
}


int wres_cancel_request(wres_args_t *args, wres_index_t index)
{
	int ret;
	wres_cancel_args_t cancel;
	wresource_t res = args->resource;
	wres_op_t op = wres_get_op(&res);
	
	cancel.op = op;
	wres_set_op(&res, WRES_OP_CANCEL);
	wres_set_off(&res, index);
	do {
		ret = wres_ioctl(&res, (char *)&cancel, sizeof(wres_cancel_args_t), NULL, 0, args->to);
		if (-ENOENT == ret) {
			ret = wres_event_wait(&res, args->out, args->outlen, NULL);
			if (!ret) //This means that the request has been processed.
				ret = -EOK;
		} else if (-ETIMEDOUT == ret) {
			struct timespec time;
			
			set_timeout(WRES_RETRY_INTERVAL, &time);
			ret = wres_event_wait(&res, args->out, args->outlen, &time);
		}
	} while (-ETIMEDOUT == ret);
	return ret;
}


int wres_wait(wres_args_t *args)
{
	int ret;
	wresource_t res = args->resource;

	if (-1 == args->index)
		return 0;

	wres_set_off(&res, args->index);
	ret = wres_event_wait(&res, args->out, args->outlen, args->timeout);
	if (args->timeout && (-ETIMEDOUT == ret)) {
		if (wres_cancel_request(args, args->index)) {
			wres_log("failed to cancel request");
			return -EFAULT;
		}
	}
	return ret;
}


int wres_ipcput(wresource_t *resource)
{
	int ret = 0;

	wres_rwlock_wrlock(resource);
	ret = wres_do_signout(resource);
	wres_rwlock_unlock(resource);
	if (ret) {
		wres_print_err(resource, "failed to sign out");
		return ret;
	}
	if (!wres_is_owner(resource))
		ret = wres_signout_request(resource);
	return ret;
}
