#ifndef _TSK_H
#define _TSK_H

#include "resource.h"
#include "stringio.h"

#define WRES_PRINT_TSKPUT

#ifdef WRES_PRINT_TSKPUT
#define wres_print_tskput(path) do { stringio_lsdir(path); } while (0)
#else
#define wres_print_tskput(...) do {} while (0) 
#endif

int wres_tskput(wresource_t *resource);
int wres_tskget(wresource_t *resource, int *gpid);

#endif