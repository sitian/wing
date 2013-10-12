/*      wln.c
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

#include "wln.h"

EVAL_DECL(wln_ioctl);
int wln_ioctl(wresource_t *resource, char *in, size_t inlen, char *out, size_t outlen, wres_id_t *to)
{
	int ret = 0;
	int retry = 0;
	char *buf = NULL;
	wcom_desc_t desc;
	wres_desc_t peer;
	wres_req_t *req = NULL;
	int buflen = sizeof(wres_req_t) + inlen;
	
	EVAL_START(wln_ioctl);
	if ((inlen > WLN_IO_MAX) || (outlen > WLN_IO_MAX)) {
		wln_log("invalid parameters");
		return -EINVAL;
	}
again:
	if (!to) {
		ret = wres_lookup(resource, &peer);
		if (ret) {
			wln_log("failed to get address");
			goto release;
		}
	} else {
		ret = wres_get_peer(*to, &peer);
		if (ret) {
			wln_log("failed to get address, id=%d", *to);
			goto release;
		}
	}
	
	if (!buf) {
		buf = malloc(buflen);
		if (!buf) {
			wln_log("no memory");
			ret = -ENOMEM;
			goto release;
		}
		req = (wres_req_t *)buf;
		req->resource = *resource;
		req->length = inlen;
		ret = wres_get_peer(wres_get_id(resource), &req->src);
		if (ret) {
			wln_log("failed to get address, id=%d", wres_get_id(resource));
			goto release;
		}
		if (inlen > 0)
			memcpy(req->buf, in, inlen);
	}
	req->resource.owner = peer.id;
	desc = wcom_connect(peer.address, WLN_PORT);
	if (wcom_assert(desc)) {
		wln_log("failed to initialize");
		ret = -EFAULT;
		goto retry;
	}
	ret = wcom_send_req(desc, buf, buflen);
	if (ret) {
		wln_log("failed to send request");
		goto retry;
	}
	ret = wcom_recv_rep(desc, out, outlen);
	if (-ENOOWNER == ret) {
		wres_cache_flush(resource);
		goto retry;
	}
	EVAL_END(wln_ioctl);
	wcom_close(desc);
release:
	if (req)
		wcom_free_req(req);
	return ret;
retry:
	if (!wcom_assert(desc))
		wcom_close(desc);
	buf = wcom_reuse_req(req);
	if (++retry < WLN_RETRY_MAX) {
		wait(WLN_RETRY_INTERVAL);
		goto again;
	}
	wcom_free_req(req);
 	return ret;
}


static void *wln_create_sender(void *buf)
{
	wres_args_t *args = (wres_args_t *)buf;
	wln_ioctl(&args->resource, args->in, args->inlen, args->out, args->outlen, args->to);
	free(buf);
	return NULL;
}


int wln_ioctl_async(wresource_t *resource, char *in, size_t inlen, char *out, size_t outlen, wres_id_t *to, pthread_t *tid)
{
	int ret;
	pthread_attr_t attr;
	wres_args_t *args = (wres_args_t *)malloc(sizeof(wres_args_t));
	
	if (!args) {
		wln_log("failed (no memory)");
		return -ENOMEM;
	}
	args->to = to;
	args->in = in;
	args->inlen = inlen;
	args->out = out;
	args->outlen = outlen;
	args->resource = *resource;
	
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	ret = pthread_create(tid, &attr, wln_create_sender, (void *)args);
	pthread_attr_destroy(&attr);
	if (ret)
		free(args);
	return ret;
}


int wln_send_request(wres_args_t *args)
{
	int ret = wln_ioctl(&args->resource, args->in, args->inlen, args->out, args->outlen, args->to);
	if (ret && !wres_is_generic_err(ret)) {
		args->index = wres_err_to_index(ret);
		ret = 0;
	}
	return ret;
}


int wln_broadcast_request(wres_args_t *args)
{	
	int ret = 0;
	
	if (!args->peers)
		ret = wln_ioctl(&args->resource, args->in, args->inlen, args->out, args->outlen, args->to);
	else  {
		int i;
		int count = 0;
		pthread_t *thread = (pthread_t *)malloc(args->peers->total * sizeof(pthread_t));
		if (!thread)
			return -ENOMEM;
		for (i = 0; i < args->peers->total; i++) {
			ret = wln_ioctl_async(&args->resource, args->in, args->inlen, args->out, args->outlen, &args->peers->list[i], &thread[i]);
			if (ret) {
				wln_log("aync ioctl failed");
				break;
			}
			count++;
		}
		for (i = 0; i < count; i++)
			pthread_join(thread[i], NULL);
		free(thread);
	}
	return ret;
}
