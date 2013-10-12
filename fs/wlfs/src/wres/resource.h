#ifndef _RESOURCE_H
#define _RESOURCE_H

#include <eval.h>
#include <wres.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include "stringio.h"
#include "cache.h"
#include "lock.h"
#include "rwlock.h"
#include "trace.h"
#include "event.h"
#include "page.h"
#include "prio.h"
#include "path.h"
#include "member.h"
#include "mds.h"

#define WRES_DEBUG
#define WRES_PRINT_ERR
#define WRES_PRINT_FREE

#ifdef WRES_DEBUG
#define wres_log(fmt, ...) printf("%s: " fmt "\n", __func__, ##__VA_ARGS__)
#else
#define wres_log(fmt, ...) do {} while()
#endif

#ifdef WRES_PRINT_ERR
#define wres_print_err(resource, fmt, ...) do { \
	wres_show_resource(resource); \
	printf(", " fmt "\n", ##__VA_ARGS__); \
} while (0)
#else
#define wres_print_err(...) do {} while (0)
#endif

#ifdef WRES_PRINT_FREE
#define wres_print_free(path) do { stringio_lsdir(path); } while (0)
#else
#define wres_print_free(...) do {} while (0)
#endif

#define wres_generate_reply(type, errno, reply) do { \
	if (errno >= 0) { \
		reply = wres_reply_alloc(sizeof(type)); \
		if (reply) { \
			type *result = wres_result_check(reply, type);	\
			result->retval = errno;	\
		} else \
			reply = wres_reply_err(-ENOMEM); \
	} else \
		reply = wres_reply_err(errno); \
} while (0)

static inline unsigned long wres_get_nr_queues(wres_cls_t cls)
{
	switch(cls) {
	case WRES_CLS_MSG:
		return 2;
	case WRES_CLS_SHM:
		return 0;
	default:
		return 1;
	}
}

static inline wres_reply_t *wres_reply_alloc(size_t size) 
{
	wres_reply_t *reply = (wres_reply_t *)malloc(sizeof(wres_reply_t) + size);
	
	if (reply) {
		memset(reply->buf, 0, size);
		reply->length = size;
	}
	return reply;
}

static inline wres_reply_t *wres_reply_err(int err)
{
	wres_reply_t *reply = (wres_reply_t *)malloc(sizeof(wres_reply_t));
	
	if (reply)
		wres_set_err(reply, err);
	return reply;
}

int wres_has_owner_path(char *path);
int wres_new(wresource_t *resource);
int wres_free(wresource_t *resource);
int wres_save_peer(wres_desc_t *desc);
int wres_access(wresource_t *resource);
int wres_is_owner(wresource_t *resource);
int wres_check_resource(wresource_t *resource);
int wres_get_peer(wres_id_t id, wres_desc_t *desc);
int wres_get_resource_count(wres_cls_t cls);
int wres_check_path(wresource_t *resource);
int wres_has_path(wresource_t *resource);
int wres_get_max_key(wres_cls_t cls);

void wres_clear_path(wresource_t *resource);
void wres_sleep(int msec);

#endif
