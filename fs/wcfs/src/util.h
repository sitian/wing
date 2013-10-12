#ifndef _UTIL_H
#define _UTIL_H

#include <stdio.h>
#include <stdlib.h>

#define WCFS_PAGE_SHIFT	12
#define WCFS_PAGE_SIZE	4096

#define wcfs_strip(val) ((val) >> WCFS_PAGE_SHIFT)
#define wcfs_page_align(addr) (((addr) >> WCFS_PAGE_SHIFT) << WCFS_PAGE_SHIFT)

unsigned long wcfs_strtoul(const char *str);
int wcfs_is_addr(const char *name);

#endif
