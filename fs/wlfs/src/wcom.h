#ifndef _WCOM_H
#define _WCOM_H

#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <wres.h>

#define WCOM_DEBUG
#define WCOM_PRINT_ADDR

#ifdef HAVE_ZMQ
#include <zmq.h>
#include <czmq.h>
#define WCOM_BACKEND "inproc://backend"
#define WCOM_NR_WORKERS	64
#endif

#ifdef HAVE_ZMQ
typedef struct wcom_addr {
	char name[32];
} wcom_addr_t;

typedef struct wcom_desc {
	void *context;
	void *sock;
} wcom_desc_t;

typedef struct wcom_peer {
	wcom_desc_t desc;
	zframe_t *addr;
	zmsg_t *msg;
} wcom_peer_t;

typedef void *(*wcom_proc_t)(void *);

#else
typedef struct sockaddr_in wcom_addr_t;

typedef int wcom_desc_t;

typedef struct wcom_peer {
	wcom_desc_t desc;
} wcom_peer_t;
#endif

static inline int sock_bind(struct sockaddr_in *addr)
{
	int ret;
	int optval = 1;
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	
	if(fd < 0)
		return fd;
	
	ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof (optval));
	if (ret < 0)
		goto out;
	
#ifdef SO_REUSEPORT
	ret = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, (char *)&optval, sizeof (optval));
	if (ret < 0)
		goto out;
#endif
	ret = bind(fd, (struct sockaddr *)addr, sizeof(struct sockaddr_in));
out:
	if (ret < 0) {
		close(fd);
		return ret;
	}
	return fd;
}

static inline void free_msg(void *data, void *hint)
{
	free(data);
}

#ifdef WCOM_DEBUG
#define wcom_log(fmt, ...) printf("%s: " fmt "\n", __func__, ##__VA_ARGS__)
#else
#define wcom_log(fmt, ...) do {} while (0)
#endif

#ifdef WCOM_PRINT_ADDR
#define wcom_print_addr(addr) printf("%s: addr=%s, port=%d\n", __func__, inet_ntoa(addr->sin_addr), addr->sin_port)
#else
#define wcom_print_addr(...) do {} while (0)
#endif

wcom_peer_t *wcom_accept(wcom_desc_t desc);
wcom_desc_t wcom_listen(wres_addr_t raddr, int port);
wcom_desc_t wcom_connect(wres_addr_t raddr, int port);
int wcom_send_req(wcom_desc_t desc, char *buf, size_t size);
int wcom_recv_req(wcom_peer_t *peer, wres_req_t **preq);
int wcom_send_rep(wcom_peer_t *peer, char *buf, size_t size);
int wcom_recv_rep(wcom_desc_t desc, char *buf, size_t size);
int wcom_assert(wcom_desc_t desc);
void wcom_free_req(wres_req_t *req);
void wcom_free_rep(wres_reply_t *rep);
void wcom_free_peer(wcom_peer_t *peer);
void wcom_disconnect(wcom_peer_t *peer);
void wcom_release(wcom_desc_t desc);
void wcom_close(wcom_desc_t desc);
void *wcom_reuse_req(wres_req_t *req);
#ifdef HAVE_ZMQ
void wcom_poll(wcom_desc_t desc, wcom_proc_t proc);
#endif

#endif
