/*      wcom.c
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

#include "wcom.h"

void wcom_set_addr(wres_addr_t raddr, int port, wcom_addr_t *caddr)
{
#ifdef HAVE_ZMQ
	sprintf(caddr->name, "tcp://%s:%d", wres_addr2str(raddr), port);
#else
	bzero(caddr, sizeof(wcom_addr_t));
	caddr->sin_family = AF_INET;
	caddr->sin_port = htons(port);
	caddr->sin_addr = raddr;
#endif
}


wcom_desc_t wcom_listen(wres_addr_t raddr, int port)
{
	wcom_desc_t desc;
	wcom_addr_t caddr;
	
	wcom_set_addr(raddr, port, &caddr);
#ifdef HAVE_ZMQ
	desc.context = zctx_new();
	desc.sock = zsocket_new(desc.context, ZMQ_ROUTER);
	zmq_bind(desc.sock, caddr.name);
#else
	desc = sock_bind(&caddr);
	if (desc < 0)
		return  -EFAULT;
	if (listen(desc, LENGTH_OF_LISTEN_QUEUE))
		return -EFAULT;
#endif
	return desc;
}


wcom_desc_t wcom_connect(wres_addr_t raddr, int port)
{
	int ret;
	wcom_desc_t desc;
	wcom_addr_t caddr;
	
	wcom_set_addr(raddr, port, &caddr);
#ifdef HAVE_ZMQ
	desc.context = zmq_init(1);
	desc.sock = zmq_socket(desc.context, ZMQ_REQ);
	ret = zmq_connect(desc.sock, dest.name);
	if (ret < 0) {
		wcom_log("failed to connect (ret=%d)", ret);
		wcom_close(desc);
		desc.context = NULL;
		desc.sock = NULL;
	}
#else
	desc = socket(AF_INET, SOCK_STREAM, 0);
	if (desc < 0) {
		wcom_log("no entry");
		return -ENOENT;
	} else {
		ret = connect(desc, (struct sockaddr *)&caddr, sizeof(struct sockaddr_in));
		if (ret < 0) {
			wcom_log("failed to connect (ret=%d)", ret);
			wcom_close(desc);
			return -EFAULT;
		}
	}
#endif
	return desc;
}


int wcom_recv(wcom_desc_t desc, char *buf, size_t size)
{
#ifndef HAVE_ZMQ
	size_t left = size;
	
	while (left > 0) {
		int ret = recv(desc, buf, left, 0);
		if (ret < 0)
			return ret;
		left -= ret;
		buf += ret;
	}
#endif
	return 0;
}


int wcom_send(wcom_desc_t desc, char *buf, size_t size)
{
#ifdef HAVE_ZMQ
	zmq_msg_t msg;
	
	zmq_msg_init_data(&msg, buf, size, free_msg, NULL);
	zmq_send(desc.sock, &msg, 0);
	zmq_msg_close(&msg);
#else
	size_t left = size;
	
	while (left > 0) {
		int ret = send(desc, buf, size, 0);
		if (ret < 0)
			return ret;
		left -= ret;
		buf += ret;
	}
#endif
	return 0;
}


int wcom_send_req(wcom_desc_t desc, char *buf, size_t size)
{
	return wcom_send(desc, buf, size);
}


int wcom_recv_rep(wcom_desc_t desc, char *buf, size_t size)
{
#ifdef HAVE_ZMQ
	int ret;
	zmq_msg_t msg;
	char *msg_data;
	size_t msg_size;
	
	zmq_msg_init(&msg);
	ret = zmq_recv(desc.sock, &msg, 0);
	if (ret)
		return ret;
	msg_size = zmq_msg_size(&msg);
	if (msg_size < sizeof(int))
		return -EINVAL;
	msg_data = (char *)zmq_msg_data(&msg);
	ret = *(int *)msg_data;
	msg_size -= sizeof(int);
	if ((msg_size <= size) && (ret == msg_size))
		memcpy(buf, msg_data + sizeof(int), msg_size);
	zmq_msg_close(&msg);
	return ret  > 0 ? 0 : ret;
#else
	int ret = 0;
	
	if (wcom_recv(desc, (char *)&ret, sizeof(int)) < 0)
		return -EIO;
	if (ret > 0) {
		if (ret > size) {
			wcom_log("failed (invalid length)");
			return -EINVAL;
		}
		if (wcom_recv(desc, buf, ret) < 0) {
			wcom_log("failed");
			return -EIO;
		}
		ret = 0;
	}
	return ret;
#endif
}


int wcom_recv_req(wcom_peer_t *peer, wres_req_t **preq)
{
#ifdef HAVE_ZMQ
	zframe_t *frame = zmsg_first(peer->msg);
	
	*preq = (wres_req_t *)zframe_data(frame);
#else
	int ret;
	wres_req_t r;
	wres_req_t *req;
	wcom_desc_t desc = peer->desc;
	
	ret = wcom_recv(desc, (char *)&r, sizeof(wres_req_t));
	if ((ret < 0) || (r.length > WRES_BUF_MAX))
		return -EINVAL;
	req = (wres_req_t *)malloc(sizeof(wres_req_t) + r.length);
	if (!req)
		return -ENOMEM;
	if (r.length > 0) {
		ret = wcom_recv(desc, req->buf, r.length);
		if (ret) {
			free(req);
			return ret;
		}
	}
	memcpy(req, &r, sizeof(wres_req_t));
	*preq = req;
#endif
	return 0;
}


int wcom_send_rep(wcom_peer_t *peer, char *buf, size_t size)
{
#ifdef HAVE_ZMQ
	zmsg_t *msg = zmsg_new();
	zframe_t *body = zframe_new(buf, size);
	
	zmsg_push(msg, body);
	zmsg_wrap(msg, peer->addr);
	zmsg_send(&msg, peer->desc.sock);
	return 0;
#else
	return wcom_send(peer->desc, buf, size);
#endif
}


int wcom_assert(wcom_desc_t desc)
{
#ifdef HAVE_ZMQ
	if (!desc.context || !desc.sock)
		return -1;
#else
	if (desc < 0)
		return -1;
#endif
	return 0;
}


wcom_peer_t *wcom_accept(wcom_desc_t desc)
{
	wcom_peer_t *peer = malloc(sizeof(wcom_peer_t));
	
	if (peer) {
#ifdef HAVE_ZMQ
		zmsg_t *msg = zmsg_recv(desc.sock);
		
		if (!msg)
			goto out;
		
		peer->desc = desc;
		peer->msg = msg;
		peer->addr =  zmsg_unwrap(msg);
#else
		struct sockaddr_in addr;
		socklen_t len = sizeof(struct sockaddr_in);
		int fd = accept(desc, (struct sockaddr *)&addr, &len);
		
		if (fd < 0)
			goto out;
		peer->desc = fd;
#endif
	}
	return peer;
out:
	free(peer);
	return NULL;
}


void wcom_close(wcom_desc_t desc)
{
#ifdef HAVE_ZMQ
	zmq_close(desc.sock);
	zmq_term(desc.context);
#else
	close(desc);
#endif
}


void *wcom_reuse_req(wres_req_t *req)
{
#ifdef HAVE_ZMQ
	return NULL;
#else
	return (void *)req;
#endif
}


void wcom_free_req(wres_req_t *req)
{
#ifndef HAVE_ZMQ
	free(req);
#endif
}


void wcom_free_rep(wres_reply_t *rep)
{
#ifndef HAVE_ZMQ
	free(rep);
#endif
}


void wcom_free_peer(wcom_peer_t *peer)
{
#ifdef HAVE_ZMQ
	zmsg_destroy(&peer->msg);
#endif
	free(peer);
}


void wcom_disconnect(wcom_peer_t *peer)
{
#ifndef HAVE_ZMQ
	close(peer->desc);
#endif
}


void wcom_release(wcom_desc_t desc)
{
#ifdef HAVE_ZMQ
	zctx_t *ctx = desc.context;
	
	zsocket_destroy(ctx, desc.sock);
	zctx_destroy(&ctx);
#else
	close(desc);
#endif
}


#ifdef HAVE_ZMQ
static void wcom_start_proc(void *args, zctx_t *ctx, void *pipe)
{
	wcom_desc_t desc;
	wcom_proc_t proc = (wcom_proc_t)args;
	
	desc.sock = zsocket_new(ctx, ZMQ_DEALER);
	desc.context = ctx;
    zsocket_connect (desc.sock, WCOM_BACKEND);
	for (;;) {
		wcom_peer_t *peer = wcom_accept(desc);
		if (!peer) {
			wcom_log("failed");
			break;
		}
		proc(peer);
	}
	zmq_close(desc.sock);
}


void wcom_poll(wcom_desc_t desc, wcom_proc_t proc)
{
	int i;
	zctx_t *ctx = desc.context;
	void *frontend = desc.sock;
	void *backend = zsocket_new(ctx, ZMQ_DEALER);
	
    zsocket_bind(backend, WCOM_BACKEND);
	for (i = 0; i < WCOM_NR_WORKERS; i++)
		zthread_fork(ctx, wcom_start_proc, proc);
	while (1) {
		zmq_pollitem_t items[] = {
			{frontend, 0, ZMQ_POLLIN, 0},
			{backend, 0, ZMQ_POLLIN, 0}
		};
		zmq_poll(items, 2, -1);
		if (items[0].revents & ZMQ_POLLIN) {
			zmsg_t *msg = zmsg_recv(frontend);
			zmsg_send(&msg, backend);
		}
		if (items[1].revents & ZMQ_POLLIN) {
			zmsg_t *msg = zmsg_recv(backend);
			zmsg_send(&msg, frontend);
		}
    }
	zsocket_destroy(ctx, backend);
}
#endif
