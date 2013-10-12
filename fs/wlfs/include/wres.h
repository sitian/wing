#ifndef _WRES_H
#define _WRES_H

#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <arpa/inet.h>

//#define HAVE_ZMQ
//#define HAVE_EVAL

//#define DEBUG_SHM
//#define DEBUG_MSG
//#define DEBUG_SEM
//#define DEBUG_PAGE
//#define DEBUG_LINE
//#define DEBUG_LOCK
//#define DEBUG_PROC
//#define DEBUG_REDO
//#define DEBUG_PRIO
//#define DEBUG_RWLOCK
//#define DEBUG_MEMBER
//#define DEBUG_RECORD
//#define DEBUG_STRINGIO
//#define DEBUG_SHOW_MORE

#define MODE_CHECK_TTL
#define MODE_CHECK_PRIORITY
#define MODE_EXPLICIT_PRIORITY
#define MODE_DYNAMIC_OWNER
#define MODE_FAST_REPLY
#define MODE_SYNC_TIME

#define ZOOKEEPER

#define BITS_PER_BYTE			8
#define LENGTH_OF_LISTEN_QUEUE	16
#define LOCAL_ADDRESS			16777226 // 10.0.0.1

#define EOK 					900
#define ENOOWNER				901
#define ERMID					902

#define WRES_POS_OP				0
#define WRES_POS_ID				1
#define WRES_POS_KEY			1
#define WRES_POS_VAL1			2
#define WRES_POS_CLS			2
#define WRES_POS_VAL2			3
#define WRES_POS_INDEX			3

#define WRES_STAT_INIT			1

#define WRES_RDONLY				0x0001
#define WRES_RDWR				0x0002
#define WRES_CREAT				0x0004
#define WRES_OWN				0x0010
#define WRES_CAND				0x0020
#define WRES_REDO				0x0040
#define WRES_CANCEL				0x0080
#define WRES_SELECT				0x0100
#define WRES_DIFF				0x0200
#define WRES_PEER				0x0400

enum wres_class {
	WRES_CLS_MSG,
	WRES_CLS_SEM,
	WRES_CLS_SHM,
	WRES_CLS_TSK,
	WRES_NR_CLASSES,
};

enum wres_operation {
	WRES_OP_UNUSED0,
	WRES_OP_MSGSND,
	WRES_OP_MSGRCV,
	WRES_OP_MSGCTL,
	WRES_OP_SEMOP,
	WRES_OP_SEMCTL,
	WRES_OP_SEMEXIT,
	WRES_OP_SHMFAULT,
	WRES_OP_SHMCTL,
	WRES_OP_UNUSED1,
	WRES_OP_TSKGET,
	WRES_OP_MSGGET,
	WRES_OP_SEMGET,
	WRES_OP_SHMGET,
	WRES_OP_SHMPUT,
	WRES_OP_TSKPUT,
	WRES_OP_RELEASE,
	WRES_OP_PROBE,
	WRES_OP_UNUSED2,
	WRES_OP_SIGNIN,
	WRES_OP_SIGNOUT,
	WRES_OP_SYNCTIME,
	WRES_OP_REPLY,
	WRES_OP_CANCEL,
	WRES_OP_UNUSED3,
	WRES_NR_OPERATIONS,
};

#define WRES_DIRECTOP_START		WRES_OP_UNUSED1
#define WRES_DIRECTOP_END		WRES_OP_UNUSED2
#define WRES_GENERICOP_START	WRES_OP_UNUSED2
#define WRES_GENERICOP_END		WRES_OP_UNUSED3

#define WRES_PATH_MAX			128
#define WRES_MEMBER_MAX			128
#define WRES_ERRNO_MAX			1000
#define WRES_KEY_MAX			0x7fffffff
#define WRES_ID_MAX				0x7fffffff
#define WRES_IO_MAX				((1 << 14) - 1)
#define WRES_BUF_MAX			((1 << 16) - 1)
#define WRES_INDEX_MAX			((1 << 30) - 1)

#define wres_entry_op(entry) ((wres_op_t)entry[WRES_POS_OP])
#define wres_entry_id(entry) ((wres_id_t)entry[WRES_POS_ID])
#define wres_entry_index(entry) ((wres_index_t)entry[WRES_POS_INDEX])
#define wres_entry_off(entry) wres_entry_val1(entry)
#define wres_entry_flags(entry) wres_entry_val2(entry)
#define wres_entry_val1(entry) ((wres_val_t)entry[WRES_POS_VAL1])
#define wres_entry_val2(entry) ((wres_val_t)entry[WRES_POS_VAL2])
#define wres_get_reply_owner(resource) ((resource)->key)

#define wres_is_generic_err(err) ((err) > -WRES_ERRNO_MAX && (err) < 0)
#define wres_set_err(p, err) do { (p)->length = err; } while (0)
#define wres_has_err(p) ((p)->length < 0)
#define wres_get_err(p) ((p)->length)

#define wres_result_check(p, type) (type *)((p)->buf)
#define wres_addr2str(addr) inet_ntoa(addr)

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

typedef key_t wres_key_t;
typedef int32_t wres_id_t;
typedef int32_t wres_val_t;
typedef int32_t wres_index_t;
typedef int32_t wres_entry_t;
typedef uint32_t wres_op_t;
typedef uint32_t wres_flg_t;
typedef uint32_t wres_cls_t;
typedef uint64_t wres_clk_t;
typedef uint64_t wres_version_t;
typedef struct in_addr wres_addr_t;
typedef long long wres_time_t;

typedef struct wresource {
	wres_cls_t cls;
	wres_key_t key;
	wres_entry_t entry[4];
	wres_id_t owner;
} wresource_t;

typedef struct wres_desc {
	wres_id_t id;
	wres_addr_t address;
} wres_desc_t;

typedef struct wres_peers {
	int total;
	wres_id_t list[0];
} wres_peers_t;

typedef struct wres_member {
	wres_id_t id;
	int count;
} wres_member_t;

struct wres_args;

typedef int(*func_request)(struct wres_args *);

typedef struct wres_args {
	int desc;
	char *buf;
	char *in;
	char *out;
	size_t inlen;
	size_t outlen;
	size_t size;
	void *entry;
	wres_id_t *to;
	wres_index_t index;
	func_request request;
	wres_peers_t *peers;
	wresource_t resource;
	struct timespec *timeout;
} wres_args_t;

typedef struct wres_req {
	wresource_t resource;
	wres_desc_t src;
	int length;
	char buf[0];
} wres_req_t;

typedef struct wres_reply {
	int length;
	char buf[0];
} wres_reply_t;

typedef struct wres_cancel_args {
	wres_op_t op;
} wres_cancel_args_t;

extern wres_addr_t local_address;
extern unsigned long domain_name;

static inline int wres_get_errno(wres_reply_t *reply)
{
	if (!reply)
		return 0;
	if (wres_has_err(reply))
		return wres_get_err(reply);
	return reply->length;
}

static inline int wres_op_direct(wres_op_t op)
{
	if ((op > WRES_DIRECTOP_START) && (op < WRES_DIRECTOP_END))
		return 1;
	return 0;
}

static inline int wres_op_generic(wres_op_t op)
{
	if ((op > WRES_GENERICOP_START) && (op < WRES_GENERICOP_END))
		return 1;
	return 0;
}

static inline void wres_set_entry(wres_entry_t *entry, wres_op_t op, wres_id_t id, wres_val_t val1, wres_val_t val2) 
{
	entry[WRES_POS_OP] = (wres_entry_t)op;
	entry[WRES_POS_ID] = (wres_entry_t)id; 
	entry[WRES_POS_VAL1] = (wres_entry_t)val1;
	entry[WRES_POS_VAL2] = (wres_entry_t)val2; 
}

static inline wres_op_t wres_get_op(wresource_t *resource)
{
	return wres_entry_op(resource->entry);
}

static inline wres_id_t wres_get_id(wresource_t *resource)
{
	return wres_entry_id(resource->entry);
}

static inline wres_flg_t wres_get_flags(wresource_t *resource)
{
	return wres_entry_flags(resource->entry);
}

static inline unsigned long wres_get_off(wresource_t *resource)
{
	return wres_entry_off(resource->entry);
}

static inline void wres_set_op(wresource_t *resource, wres_op_t op)
{
	resource->entry[WRES_POS_OP] = op;
}

static inline void wres_set_id(wresource_t *resource, wres_id_t id)
{
	resource->entry[WRES_POS_ID] = id; 
}

static inline void wres_set_off(wresource_t *resource, unsigned long off)
{
	resource->entry[WRES_POS_VAL1] = off;
}

static inline void wres_set_flags(wresource_t *resource, int flags)
{
	resource->entry[WRES_POS_VAL2] = flags | wres_get_flags(resource);
}


static inline void wres_clear_flags(wresource_t *resource, int flags)
{
	resource->entry[WRES_POS_VAL2] = wres_get_flags(resource) & ~flags;
}

static inline int wres_need_timed_lock(wresource_t *resource)
{
	return WRES_OP_CANCEL == wres_get_op(resource);
}

static inline int wres_need_half_lock(wresource_t *resource) 
{
	return WRES_OP_SHMFAULT == wres_get_op(resource);
}

static inline int wres_need_lock(wresource_t *resource) 
{
	return !wres_op_generic(wres_get_op(resource));
}

static inline int wres_need_wrlock(wresource_t *resource) 
{
	wres_op_t op = wres_get_op(resource);
		
	return (WRES_OP_SHMCTL == op) || (WRES_OP_SIGNIN == op) || (WRES_OP_SIGNOUT == op);
}

static inline int wres_index_to_err(wres_index_t index)
{
	return -index - WRES_ERRNO_MAX;
}

static inline int wres_err_to_index(int err)
{
	return -err - WRES_ERRNO_MAX;
}

// get resource according to a path
static inline int wres_path_to_resource(const char *path, wresource_t *resource, unsigned long *addr, size_t *inlen, size_t *outlen)
{
	int i;
	char *pend;
	const char *ptr = path;

	if (*ptr++ != '/')
		return -EINVAL;

	resource->cls = (wres_cls_t)strtoul(ptr, &pend, 16);
	if (ptr == pend)
		return -EINVAL;
	
	ptr = pend + 1;
	resource->key = (wres_key_t)strtoul(ptr, &pend, 16);
	if (ptr == pend)
		return -EINVAL;

	for (i = 0; i < 4; i++) {
		ptr = pend + 1;
		resource->entry[i] = (wres_entry_t)strtoul(ptr, &pend, 16);
		if (ptr == pend)
			return -EINVAL;
	}
	
	resource->owner = wres_get_id(resource);
	
	ptr = pend + 1;
	*addr = strtoul(ptr, &pend, 16);
	if (ptr == pend)
		return -EINVAL;

	ptr = pend + 1;
	*inlen = (size_t)strtoul(ptr, &pend, 16);
	if (ptr == pend)
		return -EINVAL;

	ptr = pend + 1;
	*outlen = (size_t)strtoul(ptr, &pend, 16);
	if (ptr == pend)
		return -EINVAL;
	
	return 0;
}

void wres_init();
void wres_release();
void wres_put_args(wres_args_t *args);
void wres_free_args(wres_args_t *args);
void wres_release_args(wres_args_t *args);
void wres_cache_flush(wresource_t *resource);

int wres_wait(wres_args_t *args);
int wres_save_peer(wres_desc_t *desc);
int wres_proc_direct(wres_args_t *args);
int wres_get_peer(wres_id_t id, wres_desc_t *desc);
int wres_lookup(wresource_t *resource, wres_desc_t *desc);
int wres_get_args(wresource_t *resource, unsigned long addr, size_t inlen, size_t outlen, wres_args_t *args);

wres_reply_t *wres_proc(wres_req_t *req, int flags);
#endif
