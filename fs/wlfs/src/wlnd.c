/*      wlnd.c
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

#include "wlnd.h"

EVAL_DECL(wln_proc);
void *wln_proc(void *arg)
{
	int ret;
	wres_req_t *req;
	wresource_t *resource;
	wres_reply_t *rep = NULL;
	wcom_peer_t *peer = (wcom_peer_t *)arg;
	
	EVAL_START(wln_proc);
	ret = wcom_recv_req(peer, &req);
	if (ret) {
		wln_log("failed to receive request");
		goto out;
	}
	ret = wres_save_peer(&req->src);
	if (ret) {
		wln_log("failed to save addr");
		goto out;
	}
	resource = &req->resource;
	if (wres_need_timed_lock(resource))
		ret = wres_lock_timeout(resource);
	else if (wres_need_half_lock(resource)) {
		int err;
		wres_lock_t *lock = wres_lock_top(resource);
		
		if (!lock) {
			wres_print_err(resource, "failed to lock");
			ret = -EINVAL;
		}
		err = wcom_send_rep(peer, (char *)&ret, sizeof(int));
		if (err) {
			wres_print_err(resource, "failed to send reply");
			ret = err;
		}
		wcom_disconnect(peer);
		if (!ret)
			wres_lock_buttom(lock);
		else if (lock)
			wres_unlock_top(lock);
	} else if (wres_need_wrlock(resource))
		ret = wres_rwlock_wrlock(resource);
	else if (wres_need_lock(resource))
		ret = wres_lock(resource);
	
	if (ret)
		goto reply;
	
	ret = wres_check_resource(resource);
	if (ret)
		goto unlock;
	
	rep = wres_proc(req, 0);
	ret = wres_get_errno(rep);
	if (ret < 0) {
		free(rep);
		rep = NULL;
	}
unlock:
	if (wres_need_wrlock(resource))
		wres_rwlock_unlock(resource);
	else if (wres_need_lock(resource))
		wres_unlock(resource);
reply:
	if (!wres_need_half_lock(resource)) {
		if (rep)
			wcom_send_rep(peer, (void *)rep, rep->length + sizeof(wres_reply_t));
		else
			wcom_send_rep(peer, (void *)&ret, sizeof(int));
		wcom_disconnect(peer);
	}
	wcom_free_req(req);
	if (rep)
		wcom_free_rep(rep);
out:
	wcom_free_peer(peer);
	EVAL_END(wln_proc);
	return NULL;
}


int wln_start_proc(void *ptr)
{
	int ret;
	pthread_t tid;
	pthread_attr_t attr;
	
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	ret = pthread_create(&tid, &attr, wln_proc, ptr);
	pthread_attr_destroy(&attr);
	return ret;
}


void *wlnd_main(void *args)
{
	wcom_desc_t srv = wcom_listen(local_address, WLN_PORT);
	
	if (wcom_assert(srv)) {
		wln_log("failed to create server");
		return NULL;
	}
#ifdef HAVE_ZMQ
	wcom_poll(srv, wln_proc);
#else
	for (;;) {
		wcom_peer_t *peer = wcom_accept(srv);
		
		if (!peer) {
			wln_log("failed to connect with client");
			continue;
		}
		if (wln_start_proc((void *)peer)) {
			wln_log("failed to create thread");
			wcom_close(peer->desc);
			free(peer);
			break;
		}
	}
#endif
	wcom_release(srv);
	return NULL;
}
