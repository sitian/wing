#ifndef _PATH_H
#define _PATH_H

#include <wres.h>

#define WRES_PATH_ACTION		"a"
#define WRES_PATH_STATE			"s"
#define WRES_PATH_MEMBER		"m"
#define WRES_PATH_PRIORITY		"p"
#define WRES_PATH_CACHE			"c"
#define WRES_PATH_DATA			"d"
#define WRES_PATH_CHECKIN		"i"
#define WRES_PATH_CHECKOUT		"o"
#define WRES_PATH_HOLDER		"h"
#define WRES_PATH_UPDATE		"u"
#define WRES_PATH_MFU			"f"
#define WRES_PATH_SEPERATOR		"/"

#define wres_path_append_owner(path, owner) sprintf(path + strlen(path), "%lx", (unsigned long)owner)	//1st level
#define wres_path_append_cls(path, cls) sprintf(path + strlen(path), "%lx", (unsigned long)cls)			//2nd level
#define wres_path_append_key(path, key) sprintf(path + strlen(path), "%lx", (unsigned long)key)			//3rd level
#define wres_path_append_queue(path, queue) sprintf(path + strlen(path), "%lx.", (unsigned long)queue)	//4th level
#define wres_path_append_index(path, index) sprintf(path + strlen(path), "%ld", (unsigned long)index)
#define wres_path_append_state(path) strcat(path, WRES_PATH_STATE)
#define wres_path_append_member(path) strcat(path, WRES_PATH_MEMBER)
#define wres_path_append_action(path) strcat(path, WRES_PATH_ACTION)
#define wres_path_append_cache(path) strcat(path, WRES_PATH_CACHE)
#define wres_path_append_data(path) strcat(path, WRES_PATH_DATA)
#define wres_path_append_checkin(path) strcat(path, WRES_PATH_CHECKIN)
#define wres_path_append_checkout(path) strcat(path, WRES_PATH_CHECKOUT)
#define wres_path_append_holder(path) strcat(path, WRES_PATH_HOLDER)
#define wres_path_append_priority(path) strcat(path, WRES_PATH_PRIORITY)
#define wres_path_append_update(path) strcat(path, WRES_PATH_UPDATE)
#define wres_path_append_mfu(path) strcat(path, WRES_PATH_MFU)

void wres_resource_to_path(wresource_t *resource, char *path);
void wres_get_root_path(wresource_t *resource, char *path);
void wres_get_cls_path(wresource_t *resource, char *path);
void wres_get_resource_path(wresource_t *resource, char *path);
void wres_get_member_path(wresource_t *resource, char *path);
void wres_get_state_path(wresource_t *resource, char *path);
void wres_get_action_path(wresource_t *resource, char *path);
void wres_get_mfu_path(wresource_t *resource, char *path);
void wres_get_priority_path(wresource_t *resource, char *path);
void wres_get_record_path(wresource_t *resource, char *path);
void wres_get_data_path(wresource_t *resource, char *path);
void wres_get_cache_path(wresource_t *resource, char *path);
void wres_get_holder_path(wresource_t *resource, char *path);
void wres_get_update_path(wresource_t *resource, char *path);

#endif