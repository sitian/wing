#ifndef _CONN_H
#define _CONN_H

#include "dump.h"
#include "util.h"
#include "wcfs.h"

#ifdef DEBUG_CONN
#define WDUMP_PRINT_CONN_SET
#define WDUMP_PRINT_CONN_ATTACH
#endif

#define WDUMP_CONN_FLAGS	O_RDONLY | O_LARGEFILE
#define WDUMP_CONN_MODE		S_IRWXU | S_IRWXG | S_IRWXO
#define WDUMP_CONN_SIZE		WCFS_ATTR_AREA_SIZE

#ifdef WDUMP_PRINT_CONN_SET
#define wdump_print_conn_set wdump_log
#else
#define wdump_print_conn_set(...) do {} while (0)
#endif

#ifdef WDUMP_PRINT_CONN_ATTACH
#define wdump_print_conn_attach wdump_log
#else
#define wdump_print_conn_attach(...) do {} while (0)
#endif

int wdump_conn_attach(unsigned long id, unsigned long area, size_t size, int prot, int flags);
int wdump_conn_set(unsigned long id, unsigned long area, char *name, void *value, size_t size);
int wdump_conn_update(unsigned long id, unsigned long area, unsigned long address, void *buf, size_t len);

#endif
