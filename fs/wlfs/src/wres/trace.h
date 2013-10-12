#ifndef _TRACE_H
#define _TRACE_H

#include <arpa/inet.h>
#include "resource.h"
#include "shm.h"

#define WRES_TRACE_INTERVAL	256

#define wres_show_owner(resource) printf("%s@%d: ", __func__, (resource)->owner)

#define wres_show_resource(resource) printf("%s@%d: key=%d, src=%d, off=%lu, flg=%x", \
	__func__, (resource)->owner, (resource)->key, \
	wres_get_id(resource), wres_get_off(resource), wres_get_flags(resource))

#define wres_show(resource) do {\
	wres_show_resource(resource); \
	printf("\n");\
} while (0)

#define wres_show_dir(resource) do { \
	char path[WRES_PATH_MAX]; \
	wres_get_resource_path(resource, path); \
	printf("%s@%d: ", __func__, (resource)->owner); \
	stringio_dir_print(path); \
	printf("\n"); \
} while (0)

#define wres_show_shmfault_args(resource, in) do { \
	if (WRES_OP_SHMFAULT == wres_get_op(resource)) { \
		wres_shmfault_args_t *args = (wres_shmfault_args_t *)in; \
		if (args) { \
			printf(", args <clk=%d, ", (int)args->clk); \
			printf("ver=%d, ", (int)args->version); \
			printf("%s> ", wres_shmfault2str(args->cmd)); \
			printf("[%d]", args->index); \
		} \
	} \
	printf("\n"); \
} while (0)

#define wres_show_op(resource) printf(", op=%s", wres_op2str(wres_get_op(resource)))

#define wres_show_args(resource, in) do { \
	wres_show_resource(resource); \
	wres_show_op(resource); \
	wres_show_shmfault_args(resource, in); \
} while (0)

#define wres_show_dest(dest) do { \
	if (dest) \
		printf("dest=%d", (dest)->id); \
	else \
		printf("dest=NULL"); \
} while (0)

#define wres_show_req(resource, in, dest) do { \
	wres_show_resource(resource); \
	wres_show_op(resource); \
	wres_show_dest(dest); \
	wres_show_shmfault_args(resource, in); \
} while (0)

char *wres_op2str(wres_op_t op);
char *wres_shmfault2str(wres_op_t op);
void wres_trace(wres_req_t *req);

#endif
