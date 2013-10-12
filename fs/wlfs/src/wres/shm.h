#ifndef _SHM_H
#define _SHM_H

#include <sys/ipc.h>
#include <sys/shm.h>
#include "event.h"
#include "lock.h"
#include "page.h"
#include "redo.h"
#include "rwlock.h"
#include "record.h"
#include "resource.h"
#include "stringio.h"
#include "prio.h"
#include "util.h"
#include "wait.h"

#define WRES_SHM_RETRY_TIMES			128
#define WRES_SHM_CHECK_INTERVAL			2000 	// usec	
#define WRES_SHM_NR_POSSIBLE_HOLERS		(WRES_PAGE_NR_HOLDERS - 1)

#define WRES_SHMMAX 0x2000000			/* max shared seg size (bytes) */
#define WRES_SHMMIN 1			 		/* min shared seg size (bytes) */
#define WRES_SHMMNI 4096				/* max num of segs system wide */
#define WRES_SHMALL (WRES_SHMMAX / PAGE_SIZE * (WRES_SHMMNI / 16)) /* max shm system wide (pages) */
#define WRES_SHMSEG WRES_SHMMNI			/* max shared segs per process */

#ifdef MODE_DYNAMIC_OWNER
#define WRES_SHM_NR_AREAS			3
#define WRES_SHM_NR_VISITS			1
#endif

#ifdef MODE_CHECK_TTL
#define WRES_SHM_TTL_MAX			12
#endif

#ifdef DEBUG_SHM
#define WRES_PRINT_SHM_GET_ARGS
#define WRES_PRINT_SHM_FAST_REPLY
#define WRES_PRINT_SHM_CHECK_OWNER
#define WRES_PRINT_SHM_CHECK_HOLDER
#define WRES_PRINT_SHM_NOTIFY_OWNER
#define WRES_PRINT_SHM_NOTIFY_HOLDER
#define WRES_PRINT_SHM_NOTIFY_PROPOSER
#define WRES_PRINT_SHM_DELIVER_EVENT
#define WRES_PRINT_SHM_EXPIRED_REQ
#define WRES_PRINT_SHM_SAVE_REQ

#ifdef DEBUG_SHOW_MORE
#define WRES_PRINT_SHM_CHECK_ARGS
#define WRES_PRINT_SHM_CHECK_FAST_REPLY
#define WRES_PRINT_SHM_SILENT_HOLDERS
#define WRES_PRINT_SHM_HANDLE_ZEROPAGE
#define WRES_PRINT_SHM_SAVE_UPDATES
#define WRES_PRINT_SHM_CLOCK_UPDATE
#define WRES_PRINT_SHM_PASSTHROUGH
#define WRES_PRINT_SHM_SELECT
#define WRES_PRINT_SHM_WAKEUP
#define WRES_PRINT_SHM_CACHE
#endif
#endif

#define wres_shm_is_silent_holder(page) (!(page)->hid)

enum wres_shm_operations {
	WRES_SHM_UNUSED0,
	WRES_SHM_PROPOSE,
	WRES_SHM_CHK_OWNER,
	WRES_SHM_CHK_HOLDER,
	WRES_SHM_NOTIFY_OWNER,
	WRES_SHM_NOTIFY_HOLDER,
	WRES_SHM_NOTIFY_PROPOSER,
	WRES_SHM_NR_OPERATIONS,
};

typedef struct wres_shmfault_args {
	int cmd;
#ifdef MODE_CHECK_TTL
	int ttl;
#endif
#ifdef MODE_EXPLICIT_PRIORITY
	int prio;			// priority
	wres_time_t live;   // live time
#endif
	wres_clk_t clk;
	wres_index_t index;
	wres_version_t version;
	wres_id_t owner;
	wres_peers_t peers;
} wres_shmfault_args_t;

typedef struct wres_shmfault_result {
	long retval;
	char buf[PAGE_SIZE];
} wres_shmfault_result_t;

typedef struct wres_shm_peer_info {
	int total;
	wres_desc_t owner;
	wres_desc_t list[0];
} wres_shm_peer_info_t;

typedef struct wres_shm_notify_proposer_args {
	wres_shmfault_args_t args;
	int total;
	int nr_lines;
	wres_line_t lines[0];
} wres_shm_notify_proposer_args_t;

typedef struct wres_shmctl_args {
	int cmd;
} wres_shmctl_args_t;

typedef struct wres_shmctl_result {
	long retval;
} wres_shmctl_result_t;

typedef struct wres_shm_cache {
	int index;
	wres_id_t id;
	wres_version_t version;
} wres_shm_cache_t;

#define wres_show_shm_page(page) \
		printf(", pg <hid=%d, cnt=%d, clk=%lld, ver=%lld, hld=%d, own=%d, flg=%x>", \
		page->hid, page->count, page->clk, page->version, page->nr_holders, page->owner, page->flags) \

#define wres_show_shm_peers(peers) do {\
	int i; \
	if (!peers || !peers->total) \
		break; \
	printf(", peers <"); \
	for (i = 0; i < peers->total - 1; i++) \
		printf("%d ", peers->list[i]); \
	printf("%d>", peers->list[i]); \
} while (0)

#define wres_show_shm_holders(page) do { \
	int i; \
	if (!page->nr_holders) \
		break; \
	printf(", holders <"); \
	for (i = 0; i < page->nr_holders - 1; i++) \
		printf("%d ", page->holders[i]); \
	printf("%d>", page->holders[i]); \
} while (0)

#define wres_show_shm_args(args) do {\
	if (NULL == args) { \
		printf("\n"); \
		break; \
	} \
	printf(", args <clk=%d, ver=%d> [%d] \n", (int)(args)->clk, (int)(args)->version, (int)(args)->index); \
} while (0)

#define wres_show_shm_silent_holders(resource, page) do { \
	int nr_silent_holders = page->nr_silent_holders; \
	printf("silent holders="); \
	if (nr_silent_holders > 0) { \
		char path[WRES_PATH_MAX]; \
		stringio_entry_t *entry; \
		wres_get_holder_path(resource, path); \
		entry = stringio_get_entry(path, nr_silent_holders * sizeof(wres_id_t), STRINGIO_RDONLY); \
		if (entry) { \
			int i; \
			wres_id_t *holders = stringio_get_desc(entry, wres_id_t); \
			for (i = 0; i < nr_silent_holders; i++) \
				printf("%d ", holders[i]); \
			printf("\n"); \
			break; \
		} \
	} \
	printf("NULL\n"); \
} while (0)

#define wres_show_shm_cache(id, version) printf(", cache <id=%d, ver=%d>", id, version)
#define wres_show_shm_index(index) printf(" [%d]\n", index)

#ifdef WRES_PRINT_SHM_CHECK_OWNER
#define wres_print_shm_check_owner(page, req) do { \
	wresource_t *resource = &(req)->resource; \
	wres_shmfault_args_t *args = (wres_shmfault_args_t *)(req->buf); \
	wres_show_resource(resource); \
	wres_show_shm_holders(page); \
	wres_show_shm_page(page); \
	wres_show_shm_args(args); \
} while (0)
#else
#define wres_print_shm_check_owner(...) do {} while (0) 
#endif

#ifdef WRES_PRINT_SHM_FAST_REPLY
#define wres_print_shm_fast_reply(resource, hid, nr_peers) do { \
	wres_show_resource(resource); \
	printf(", hid=%d, nr_peers=%d\n", hid, nr_peers); \
} while (0)
#else
#define wres_print_shm_fast_reply(...) do {} while (0)
#endif

#ifdef WRES_PRINT_SHM_CHECK_HOLDER
#define wres_print_shm_check_holder(page, req) do { \
	wresource_t *resource = &(req)->resource; \
	wres_shmfault_args_t *args = (wres_shmfault_args_t *)(req->buf); \
	wres_show_resource(resource); \
	wres_show_shm_page(page); \
	wres_show_shm_args(args); \
} while (0)
#else
#define wres_print_shm_check_holder(...) do {} while (0) 
#endif

#ifdef WRES_PRINT_SHM_CHECK_FAST_REPLY
#define wres_print_shm_check_fast_reply(req, hid, nr_peers) do { \
	wresource_t *resource = &(req)->resource; \
	wres_shmfault_args_t *args = (wres_shmfault_args_t *)(req->buf); \
	wres_show_resource(resource); \
	printf(", hid=%d, nr_peers=%d", hid, nr_peers); \
	wres_show_shm_args(args); \
} while (0)
#else
#define wres_print_shm_check_fast_reply(...) do {} while (0) 
#endif

#ifdef WRES_PRINT_SHM_NOTIFY_OWNER
#define wres_print_shm_notify_owner(page, req) do { \
	wresource_t *resource = &(req)->resource; \
	wres_shmfault_args_t *args = (wres_shmfault_args_t *)(req->buf); \
	wres_show_resource(resource); \
	wres_show_shm_holders(page); \
	wres_show_shm_page(page); \
	wres_show_shm_args(args); \
} while (0)
#else
#define wres_print_shm_notify_owner(...) do {} while (0) 
#endif

#ifdef WRES_PRINT_SHM_PASSTHROUGH
#define wres_print_shm_passthrough(resource) do { \
	wres_show_resource(resource); \
	printf("\n"); \
} while (0)
#else
#define wres_print_shm_passthrough(...) do {} while (0) 
#endif

#ifdef WRES_PRINT_SHM_NOTIFY_HOLDER
#define wres_print_shm_notify_holder(page, req) do { \
	wresource_t *resource = &(req)->resource; \
	wres_shmfault_args_t *args = (wres_shmfault_args_t *)(req->buf); \
	wres_show_resource(resource); \
	wres_show_shm_page(page); \
	wres_show_shm_args(args); \
} while (0)
#else
#define wres_print_shm_notify_holder(...) do {} while (0) 
#endif

#ifdef WRES_PRINT_SHM_NOTIFY_PROPOSER
#define wres_print_shm_notify_proposer(page, req) do { \
	wresource_t *resource = &(req)->resource; \
	wres_shmfault_args_t *args = (wres_shmfault_args_t *)(req->buf); \
	wres_show_resource(resource); \
	wres_show_shm_page(page); \
	wres_show_shm_args(args); \
} while (0)
#else
#define wres_print_shm_notify_proposer(...) do {} while (0)
#endif

#ifdef WRES_PRINT_SHM_UPDATE_HOLDER
#define wres_print_shm_update_holder(page, req) do { \
	wresource_t *resource = &(req)->resource; \
	wres_shmfault_args_t *args = (wres_shmfault_args_t *)(req->buf); \
	wres_show_owner(resource); \
	wres_show_shm_holders(page); \
	wres_show_shm_args(args); \
} while (0)
#else
#define wres_print_shm_update_holder(...) do {} while (0) 
#endif

#ifdef WRES_PRINT_SHM_HANDLE_ZEROPAGE
#define wres_print_shm_handle_zeropage(resource, page) do { \
	wres_show_resource(resource); \
	wres_show_shm_page(page); \
	printf("\n"); \
} while (0)
#else
#define wres_print_shm_handle_zeropage(...) do {} while (0) 
#endif

#ifdef WRES_PRINT_SHM_CHECK_ARGS
#define wres_print_shm_check_args(resource) do { \
	wres_show_resource(resource); \
	printf("\n"); \
} while (0)
#else
#define wres_print_shm_check_args(...) do {} while (0) 
#endif

#ifdef WRES_PRINT_SHM_SAVE_UPDATES
#define wres_print_shm_save_updates(resource, nr_lines) do { \
	wres_show_resource(resource); \
	printf(", updates=%d\n", nr_lines); \
} while (0)
#else
#define wres_print_shm_save_updates(...) do {} while (0)
#endif

#ifdef WRES_PRINT_SHM_GET_ARGS
#define wres_print_shm_get_args(args) do { \
	wres_page_t *page; \
	wresource_t *resource; \
	if ((args)->in == (args)->buf) \
		break; \
	resource = &(args)->resource; \
	page = stringio_get_desc((stringio_entry_t *)(args)->entry, wres_page_t); \
	wres_show_resource(resource); \
	wres_show_shm_peers((args)->peers); \
	wres_show_shm_page(page); \
	wres_show_shm_args((wres_shmfault_args_t *)(args)->in); \
} while (0)
#else
#define wres_print_shm_get_args(...) do {} while (0)
#endif

#ifdef WRES_PRINT_SHM_SAVE_REQ
#define wres_print_shm_save_req(req) do { \
	wresource_t *resource = &(req)->resource; \
	wres_shmfault_args_t *args = (wres_shmfault_args_t *)(req->buf); \
	wres_show_resource(resource); \
	wres_show_shm_args(args); \
} while (0)
#else
#define wres_print_shm_save_req(...) do {} while (0)
#endif

#ifdef WRES_PRINT_SHM_SELECT
#define wres_print_shm_select(resource) do { \
	wres_show_resource(resource); \
	printf(", op=%s\n", wres_op2str(wres_get_op(resource))); \
} while (0)
#else
#define wres_print_shm_select(...) do {} while (0) 
#endif

#ifdef WRES_PRINT_SHM_WAKEUP
#define wres_print_shm_wakeup(resource) wres_show(resource)
#else
#define wres_print_shm_wakeup(...) do {} while (0)
#endif

#ifdef WRES_PRINT_SHM_DELIVER_EVENT
#define wres_print_shm_deliver_event(req)	do { \
	wres_shmfault_args_t *args = (wres_shmfault_args_t *)req->buf; \
	wres_show_owner(&req->resource); \
	printf("++event++"); \
	wres_show_shm_index(args->index); \
} while (0)
#else
#define wres_print_shm_deliver_event(...) do {} while (0)
#endif

#ifdef WRES_PRINT_SHM_CACHE
#define wres_print_shm_cache(resource, id, index, version) do { \
	wres_show_resource(resource); \
	wres_show_shm_cache(id, (int)version); \
	wres_show_shm_index(index); \
} while (0)
#else
#define wres_print_shm_cache(...) do {} while (0)
#endif

#ifdef WRES_PRINT_SHM_EXPIRED_REQ
#define wres_print_shm_expired_req(page, req) do { \
	wres_shmfault_args_t *args = (wres_shmfault_args_t *)(req)->buf; \
	wres_show_owner(&(req)->resource); \
	printf("find an expired request, "); \
	printf("pg <clk=%lld, ver=%lld>", (page)->clk, (page)->version); \
	wres_show_shm_args(args); \
} while (0)
#else
#define wres_print_shm_expired_req(...) do {} while (0)
#endif

#ifdef WRES_PRINT_SHM_CLOCK_UPDATE
#define wres_print_shm_clock_update(resource, from, to) do { \
	wres_show_owner(resource); \
	printf("clk %lld=>%lld\n", from, to); \
} while (0)
#else
#define wres_print_shm_clock_update(...) do {} while (0)
#endif

#ifdef WRES_PRINT_SHM_SILENT_HOLDERS
#define wres_print_shm_silent_holders(resource, page) do { \
	wres_show_owner(resource); \
	wres_show_shm_silent_holders(resource, page); \
} while (0)
#else
#define wres_print_shm_silent_holders(...) do {} while (0)
#endif

int wres_shm_request(wres_args_t *args);
int wres_shm_init(wresource_t *resource);
int wres_shm_create(wresource_t *resource);
int wres_shm_destroy(wresource_t *resource);
int wres_shm_get_args(wresource_t *resource, wres_args_t *args);
int wres_shm_check_args(wresource_t *resource, wres_args_t *args);
wres_reply_t *wres_shm_fault(wres_req_t *req, int flags);
wres_reply_t *wres_shm_shmctl(wres_req_t *req, int flags);
void wres_shm_put_args(wres_args_t *args);

#endif
