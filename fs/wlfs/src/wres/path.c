/*      path.c
 *      
 *      Copyright (C) 2013 Yi-Wei Ci <ciyiwei@hotmail.com>
 *      
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#include "path.h"

static inline unsigned long wres_get_queue(wres_entry_t *entry)
{
	unsigned long queue;
	wres_op_t op = wres_entry_op(entry);
	
	switch(op) {
	case WRES_OP_MSGRCV:
		queue = 1;
		break;
	case WRES_OP_SHMFAULT:
		queue = wres_entry_off(entry);
		break;
	default:
		queue = 0;
	}
	return queue;
}


void wres_resource_to_path(wresource_t *resource, char *path)
{
	strcpy(path, WRES_PATH_SEPERATOR);
	wres_path_append_cls(path, resource->cls);
	strcat(path, WRES_PATH_SEPERATOR);
	wres_path_append_key(path, resource->key);
}


void wres_get_root_path(wresource_t *resource, char *path)
{
	strcpy(path, WRES_PATH_SEPERATOR);
	wres_path_append_owner(path, resource->owner);
}


void wres_get_cls_path(wresource_t *resource, char *path)
{
	wres_get_root_path(resource, path);
	strcat(path, WRES_PATH_SEPERATOR);
	wres_path_append_cls(path, resource->cls);
}


void wres_get_resource_path(wresource_t *resource, char *path)
{
	wres_get_root_path(resource, path);
	wres_resource_to_path(resource, path + strlen(path));
	strcat(path, WRES_PATH_SEPERATOR);
}


void wres_get_member_path(wresource_t *resource, char *path)
{
	wres_get_resource_path(resource, path);
	wres_path_append_member(path);
}


void wres_get_state_path(wresource_t *resource, char *path)
{
	wres_get_resource_path(resource, path);
	wres_path_append_state(path);
}


void wres_get_action_path(wresource_t *resource, char *path)
{
	wres_get_resource_path(resource, path);
	wres_path_append_action(path);
}


void wres_get_mfu_path(wresource_t *resource, char *path)
{
	wres_get_resource_path(resource, path);
	wres_path_append_mfu(path);
}


void wres_get_priority_path(wresource_t *resource, char *path)
{
	wres_get_resource_path(resource, path);
	wres_path_append_priority(path);
}


void wres_get_record_path(wresource_t *resource, char *path)
{
	wres_get_resource_path(resource, path);
	wres_path_append_queue(path, wres_get_queue(resource->entry));
}


void wres_get_data_path(wresource_t *resource, char *path)
{
	wres_get_record_path(resource, path);
	wres_path_append_data(path);
}


void wres_get_cache_path(wresource_t *resource, char *path)
{
	wres_get_record_path(resource, path);
	wres_path_append_cache(path);
}


void wres_get_holder_path(wresource_t *resource, char *path)
{
	wres_get_record_path(resource, path);
	wres_path_append_holder(path);
}


void wres_get_update_path(wresource_t *resource, char *path)
{
	wres_get_record_path(resource, path);
	wres_path_append_update(path);
}
