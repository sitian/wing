#ifndef _WLFS_H
#define _WLFS_H

#define FUSE_USE_VERSION 26
#ifdef ZOOKEEPER
#include <zookeeper.h>
#endif
#include <pthread.h>
#include <unistd.h>
#include <fuse.h>
#include <wres.h>
#include "wres/trace.h"
#include "wlnd.h"
#include "wln.h"

//#define WLFS_DEBUG
#define WLFS_STAT_INIT			1
#define WLFS_LOCK_GROUP_SIZE	1024
#define WLFS_MOUNT_PATH			"/wing/mnt/wlfs/"

#ifdef WLFS_DEBUG
#define WLFS_PRINT_LOCK
#define WLFS_PRINT_UNLOCK
#endif

typedef struct wlfs_lock_desc {
	unsigned long entry[4];
} wlfs_lock_desc_t;

typedef struct wlfs_lock_group {
	pthread_mutex_t mutex;
	rbtree head;
} wlfs_lock_group_t;

typedef struct wlfs_lock {
	wlfs_lock_desc_t desc;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int count;
} wlfs_lock_t;

#ifdef WLFS_PRINT_LOCK
#define wlfs_print_lock wres_show
#else
#define wlfs_print_lock(...) do {} while (0)
#endif

#ifdef WLFS_PRINT_UNLOCK
#define wlfs_print_unlock wres_show
#else
#define wlfs_print_unlock(...) do {} while (0)
#endif

static inline int wlfs_getattr(const char *path, struct stat *stbuf)
{
	bzero(stbuf, sizeof(struct stat));
	if (!strcmp(path, "/")) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else {
		stbuf->st_mode = S_IFREG | 0777;
		stbuf->st_nlink = 1;
		stbuf->st_size = 0x7fffffff;
	}
	return 0;
}

static int wlfs_open(const char *path, struct fuse_file_info *fi);
static struct fuse_operations wlfs_oper = {
	.getattr	= wlfs_getattr,
	.open		= wlfs_open,
};

#endif
