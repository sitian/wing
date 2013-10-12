#ifndef _REDO_H
#define _REDO_H

#include "resource.h"
#include "stringio.h"
#include "trace.h"

#ifdef DEBUG_REDO
#define WRES_PRINT_REDO
#endif

#ifdef WRES_PRINT_REDO
#define wres_show_redo(resource, in, index) do { \
	printf(", rec.index=%d, ", index); \
	if (WRES_OP_SHMFAULT == wres_get_op(resource)) { \
		wres_shmfault_args_t *args = (wres_shmfault_args_t *)in; \
		if (args && (args->cmd < WRES_SHM_NR_OPERATIONS)) { \
			printf("%s\n", wres_shmfault2str(args->cmd)); \
			break; \
		} \
	} \
	printf("%s\n", wres_op2str(wres_get_op(resource))); \
} while (0)

#define wres_print_redo(resource, in, index) do { \
	wres_show_resource(resource); \
	wres_show_redo(resource, in, index); \
} while (0)
#else
#define wres_print_redo(...) do {} while (0)
#endif

int wres_redo(wresource_t *resource, int flags);
int wres_redo_all(wresource_t *resource, int flags);

#endif