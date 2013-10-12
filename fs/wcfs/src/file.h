#ifndef _FILE_H
#define _FILE_H

#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "util.h"
#include "wcfs.h"

#define WCFS_READ		0x00000001
#define WCFS_WRITE		0x00000002

typedef struct wcfs_file {
	unsigned long id;
	unsigned long area;
	unsigned long off;
	int flags;
	int desc;
} wcfs_file_t;

typedef struct wcfs_file_header {
	unsigned long ver;
	size_t length;
	int desc;
} wcfs_file_header_t;

void wcfs_file_close(wcfs_file_t *file);
int wcfs_file_setlen(const char *path, size_t length);
int wcfs_file_read(wcfs_file_t *file, void *buf, size_t len);
int wcfs_file_write(wcfs_file_t *file, void *buf, size_t len);
int wcfs_file_update(const char *path, unsigned long off, void *buf, size_t len);
int wcfs_file_open(const char *path, unsigned long off, int flags, wcfs_file_t *file);

#endif
