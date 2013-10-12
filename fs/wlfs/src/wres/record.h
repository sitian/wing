#ifndef _RECORD_H
#define _RECORD_H

#include "resource.h"
#include "stringio.h"
#include "trace.h"

#define WRES_RECORD_NEW				0x00000001
#define WRES_RECORD_FREE			0x00000002
#define WRES_RECORD_CANCEL			0x00000004

#ifdef DEBUG_RECORD
#define WRES_PRINT_RECORD
#endif

typedef struct wres_record_header {
	wres_flg_t flg;
} wres_rec_hdr_t;

typedef struct wres_record {
	stringio_entry_t *entry;
	wres_req_t *req;
} wres_record_t;

#define wres_show_record(req, path) printf(", rec <len=%d, path=%s, op=%s>\n", \
		(req)->length, path, wres_op2str(wres_get_op(&(req)->resource)))

#ifdef WRES_PRINT_RECORD
#define wres_print_record(req, path) do { \
	wres_show_resource(&(req)->resource); \
	wres_show_record(req, path); \
} while (0)
#else
#define wres_print_record(...) do {} while (0)
#endif

int wres_record_empty(char *path);
int wres_record_first(char *path, wres_index_t *index);
int wres_record_next(char *path, wres_index_t *index);
int wres_record_remove(char *path, wres_index_t index);
int wres_record_save(char *path, wres_req_t *req, wres_index_t *index);
int wres_record_get(char *path, wres_index_t index, wres_record_t *record);
void wres_record_put(wres_record_t *record);

#endif
