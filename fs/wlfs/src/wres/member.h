#ifndef _MEMBER_H
#define _MEMBER_H

#include "resource.h"
#include "trace.h"

#ifdef DEBUG_MEMBER
#define WRES_PRINT_MEMBER_GET
#define WRES_PRINT_MEMBER_PUT
#endif

typedef struct wres_members {
	int total;
	wres_member_t list[0];
} wres_members_t;


#ifdef WRES_PRINT_MEMBER_GET
#define wres_print_member_get(resource, path) do { \
	wres_show_resource(resource); \
	printf(", path=%s\n", path); \
} while (0)
#else
#define wres_print_member_get(...) do {} while (0)
#endif

#ifdef WRES_PRINT_MEMBER_PUT
#define wres_print_member_put wres_show
#else
#define wres_print_member_put(...) do {} while (0)
#endif

int wres_member_notify(wres_req_t *req);
int wres_member_new(wresource_t *resource);
int wres_member_delete(wresource_t *resource);
int wres_member_is_public(wresource_t *resource);
int wres_member_is_active(wresource_t *resource);
int wres_member_get_pos(wresource_t *resource, wres_id_t id);
int wres_member_list(wresource_t *resource, wres_id_t *list);
int wres_member_update(wresource_t *resource, wres_member_t *member);
int wres_member_save(wresource_t *resource, wres_id_t *list, int total);
int wres_member_lookup(wresource_t *resource, wres_member_t *member, int *active_member_count);

#endif
