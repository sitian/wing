#ifndef _WCFS_H
#define _WCFS_H

#define FUSE_USE_VERSION 26
#include <fuse.h>
#include "file.h"

#define WCFS_DEBUG
#define WCFS_ATTR_AREA_SIZE		"ATTR_AREA_SIZE"
#define WCFS_ATTR_LEN  32

#ifdef WCFS_DEBUG
#define wcfs_log(fmt, ...) printf("%s: " fmt "\n", __func__, ##__VA_ARGS__)
#else
#define wcfs_log(fmt, ...) do {} while()
#endif

#endif
