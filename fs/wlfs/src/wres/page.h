#ifndef _PAGE_H
#define _PAGE_H

#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include "line.h"
#include "util.h"
#include "rbtree.h"
#include "resource.h"
#include "stringio.h"

#define WRES_PAGE_RDONLY				WRES_RDONLY
#define WRES_PAGE_RDWR					WRES_RDWR
#define WRES_PAGE_CREAT					WRES_CREAT
#define WRES_PAGE_OWN					WRES_OWN
#define WRES_PAGE_CAND					WRES_CAND
#define WRES_PAGE_REDO					WRES_REDO
#define WRES_PAGE_READY					0x00010000
#define WRES_PAGE_ACTIVE				0x00020000
#define WRES_PAGE_WAIT					0x00040000
#define WRES_PAGE_UPDATE				0x00080000
#define WRES_PAGE_EXCL					0x00100000
#define WRES_PAGE_UPTODATE				0x00200000

#define WRES_PAGE_LOCK_GROUP_SIZE		1024
#define WRES_PAGE_DIFF_SIZE				((WRES_PAGE_NR_VERSIONS * WRES_LINE_MAX + BITS_PER_BYTE - 1) / BITS_PER_BYTE)

#define WRES_PAGE_NR_VERSIONS			16
#define WRES_PAGE_NR_CANDIDATES			16
#define WRES_PAGE_NR_HOLDERS			8
#define WRES_PAGE_NR_HITS				4

#define WRES_PAGE_CHECK_INTERVAL		1000 	// usec	

#ifdef DEBUG_PAGE
#define WRES_PRINT_PAGE_LOCK
#define WRES_PRINT_PAGE_UNLOCK

#ifdef DEBUG_SHOW_MORE
#define WRES_PRINT_PAGE_GET
#define WRES_PRINT_PAGE_PUT
#define WRES_PRINT_PAGE_DIFF
#endif
#endif

typedef struct wres_page_lock_desc {
	unsigned long entry[2];
} wres_page_lock_desc_t;

typedef struct wres_page_lock_group {
	pthread_mutex_t mutex;
	rbtree head;
} wres_page_lock_group_t;

typedef struct wres_page_lock {
	wres_page_lock_desc_t desc;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int count;
} wres_page_lock_t;

typedef struct wres_page {
	wres_version_t version;
	wres_digest_t digest[WRES_LINE_MAX];
	wres_index_t index;
	wres_clk_t clk;
	wres_clk_t oclk;	// the clock of owner
	int hid;			// the id of holder
	int flags;
	int count;
	int nr_holders;
	int nr_candidates;
	int nr_silent_holders;
	int diff[WRES_PAGE_NR_VERSIONS][WRES_LINE_MAX];
	int collect[WRES_LINE_MAX];
	char buf[PAGE_SIZE];
	wres_id_t owner;
	wres_id_t holders[WRES_PAGE_NR_HOLDERS];
	wres_member_t candidates[WRES_PAGE_NR_CANDIDATES];
} wres_page_t;

static inline void wres_pg_mkready(wres_page_t *page)
{
	page->flags |= WRES_PAGE_READY;
}

static inline void wres_pg_mkactive(wres_page_t *page)
{
	page->flags |= WRES_PAGE_ACTIVE;
}

static inline void wres_pg_mkwait(wres_page_t *page)
{
	page->flags |= WRES_PAGE_WAIT;
}

static inline void wres_pg_mkwrite(wres_page_t *page)
{
	page->flags &= ~WRES_PAGE_RDONLY;
	page->flags |= WRES_PAGE_RDWR;
}

static inline void wres_pg_mkread(wres_page_t *page)
{
	page->flags &= ~WRES_PAGE_RDWR;
	page->flags |= WRES_PAGE_RDONLY;
}

static inline void wres_pg_mkredo(wres_page_t *page)
{
	page->flags |= WRES_PAGE_REDO;
}

static inline void wres_pg_mkown(wres_page_t *page)
{
	page->flags |= WRES_PAGE_OWN;
}

static inline void wres_pg_mkcand(wres_page_t *page)
{
	page->flags |= WRES_PAGE_CAND;
}

static inline void wres_pg_mkupdate(wres_page_t *page)
{
	page->flags |= WRES_PAGE_UPDATE;
}

static inline void wres_pg_mkuptodate(wres_page_t *page)
{
	page->flags |= WRES_PAGE_UPTODATE;
}

static inline void wres_pg_clrready(wres_page_t *page)
{
	page->flags &= ~WRES_PAGE_READY;
}

static inline void wres_pg_clractive(wres_page_t *page)
{
	page->flags &= ~WRES_PAGE_ACTIVE;
}

static inline void wres_pg_clrwait(wres_page_t *page)
{
	page->flags &= ~WRES_PAGE_WAIT;
}

static inline void wres_pg_clrown(wres_page_t *page)
{
	page->flags &= ~WRES_PAGE_OWN;
}

static inline void wres_pg_clrcand(wres_page_t *page)
{
	page->flags &= ~WRES_PAGE_CAND;
}

static inline void wres_pg_clrredo(wres_page_t *page)
{
	page->flags &= ~WRES_PAGE_REDO;
}

static inline void wres_pg_clrupdate(wres_page_t *page)
{
	page->flags &= ~WRES_PAGE_UPDATE;
}

static inline void wres_pg_clruptodate(wres_page_t *page)
{
	page->flags &= ~WRES_PAGE_UPTODATE;
}

static inline void wres_pg_rdprotect(wres_page_t *page)
{
	page->flags &= ~WRES_PAGE_RDWR;
	page->flags &= ~WRES_PAGE_RDONLY;
}

static inline void wres_pg_wrprotect(wres_page_t *page)
{
	page->flags &= ~WRES_PAGE_RDWR;
	page->flags |= WRES_PAGE_RDONLY;
}

static inline int wres_pg_read(wres_page_t *page)
{
	return page->flags & WRES_PAGE_RDONLY;
}

static inline int wres_pg_write(wres_page_t *page)
{
	return page->flags & WRES_PAGE_RDWR;
}

static inline int wres_pg_access(wres_page_t *page)
{
	return wres_pg_read(page) || wres_pg_write(page);
}

static inline int wres_pg_redo(wres_page_t *page)
{
	return page->flags & WRES_PAGE_REDO;
}

static inline int wres_pg_wait(wres_page_t *page)
{
	return page->flags & WRES_PAGE_WAIT;
}

static inline int wres_pg_active(wres_page_t *page)
{
	return page->flags & WRES_PAGE_ACTIVE;
}

static inline int wres_pg_ready(wres_page_t *page)
{
	return page->flags & WRES_PAGE_READY;
}

static inline int wres_pg_own(wres_page_t *page)
{
	return page->flags & WRES_PAGE_OWN;
}

static inline int wres_pg_cand(wres_page_t *page)
{
	return page->flags & WRES_PAGE_CAND;
}

static inline int wres_pg_update(wres_page_t *page)
{
	return page->flags & WRES_PAGE_UPDATE;
}

static inline int wres_pg_uptodate(wres_page_t *page)
{
	return page->flags & WRES_PAGE_UPTODATE;
}

#ifdef WRES_PRINT_PAGE_DIFF
#define wres_print_page_diff(diff) do { \
	int i, j; \
	for (i = 0; i < WRES_PAGE_NR_VERSIONS; i++) { \
		for (j = 0; j < WRES_LINE_MAX; j++) \
			printf("%d ", diff[i][j]); \
		printf("\n"); \
	} \
} while (0)
#else
#define wres_print_page_diff(...) do {} while (0)
#endif

#ifdef WRES_PRINT_PAGE_GET
#define wres_print_page_get(resource, path) do { \
	wres_show_resource(resource); \
	printf(", path=%s\n", path); \
} while (0)
#else
#define wres_print_page_get(...) do {} while (0)
#endif

#ifdef WRES_PRINT_PAGE_PUT
#define wres_print_page_put wres_show
#else
#define wres_print_page_put(...) do {} while (0)
#endif

#ifdef WRES_PRINT_PAGE_LOCK
#define wres_print_page_lock wres_show
#else
#define wres_print_page_lock(...) do {} while (0)
#endif

#ifdef WRES_PRINT_PAGE_UNLOCK
#define wres_print_page_unlock wres_show
#else
#define wres_print_page_unlock(...) do {} while (0)
#endif

int wres_page_calc_holders(wres_page_t *page);
int wres_page_get_hid(wres_page_t *page, wres_id_t id);
int wres_page_protect(wresource_t *resource, wres_page_t *page);
int wres_page_get_diff(wres_page_t *page, wres_version_t version, int *diff);
int wres_page_add_holder(wresource_t *resource, wres_page_t *page, wres_id_t id);
int wres_page_search_holder_list(wresource_t *resource, wres_page_t *page, wres_id_t id);
int wres_page_update_holder_list(wresource_t *resource, wres_page_t *page, wres_id_t *holders, int nr_holders);

void wres_page_lock_init();
void wres_page_put(wresource_t *resource, void *entry);
void *wres_page_get(wresource_t *resource, wres_page_t **page, int flags);
void wres_page_clear_holder_list(wresource_t *resource, wres_page_t *page);

wres_index_t wres_page_update_index(wres_page_t *page);

#endif
