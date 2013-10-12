#ifndef _STRINGIO_H
#define _STRINGIO_H

#include <sys/mman.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <wres.h>
#include "list.h"
#include "rbtree.h"

#define STRINGIO_RDONLY			WRES_RDONLY
#define STRINGIO_RDWR			WRES_RDWR
#define STRINGIO_CREAT			WRES_CREAT
#define STRINGIO_AUTO_INCREMENT	0x00000010

//#define STRINGIO_CHECK_PATH

#define STRINGIO_STAT_INIT		1		
#define STRINGIO_PATH_MAX 		80
#define STRINGIO_BUF_SIZE		(1 << 16)

#define STRINGIO_SEPARATOR		'/'
#define STRINGIO_ROOT_PATH		"/"

#define stringio_get_desc(entry, type) ((type *)*((entry)->desc))
#define stringio_items_count(entry, item_size) (stringio_size((entry)->file) / item_size)

typedef char ** stringio_desc_t;

typedef struct stringio_file {
	unsigned long f_pos;
	char f_path[STRINGIO_PATH_MAX];
	char *f_buf;
	size_t f_size;
} stringio_file_t;

typedef struct stringio_dir {
	pthread_rwlock_t d_lock;
	char d_path[STRINGIO_PATH_MAX];
	int d_count;
	rbtree d_tree;
} stringio_dir_t;

typedef struct stringio_name {
	char name[STRINGIO_PATH_MAX];
} stringio_name_t;

typedef struct stringio_entry {
	stringio_desc_t desc;
	stringio_file_t *file;
	size_t size;
} stringio_entry_t;

typedef int (*stringio_callback)(char *path);

#ifdef DEBUG_STRINGIO
#define stringio_log(fmt, ...) printf("%s: " fmt "\n", __func__, ##__VA_ARGS__)
#else
#define stringio_log(...) do {} while (0)
#endif

int stringio_isdir(char *path);
int stringio_rmdir(char *path);
int stringio_mkdir(char *path);
int stringio_remove(char *path);
int stringio_is_empty_dir(char *path);
int stringio_size(stringio_file_t *filp);
int stringio_access(char *path, int mode);
int stringio_munmap(void *start, size_t length);
int stringio_dirname(char *path, char *dir_name);
int stringio_dir_iter(char *path, stringio_callback func);
int stringio_truncate(stringio_file_t *filp, off_t length);
int stringio_append(stringio_entry_t *entry, void *buf, size_t size);
int stringio_seek(stringio_file_t *filp, long int offset, int origin);
int stringio_read(void *ptr, size_t size, size_t count, stringio_file_t *filp);
int stringio_write(const void *ptr, size_t size, size_t count, stringio_file_t *filp);

void stringio_init();
void stringio_lsdir(char *path);
void stringio_close(stringio_file_t *filp);
void stringio_put_entry(stringio_entry_t *entry);
void *stringio_mmap(void *start, size_t length, int prot, int flags, stringio_file_t *filp, off_t offset);

stringio_file_t *stringio_open(char *path, const char *mode);
stringio_entry_t *stringio_get_entry(char *path, size_t size, int flags);

#endif
