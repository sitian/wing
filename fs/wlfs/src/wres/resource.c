/*      resource.c
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

#include "resource.h"

void wres_init()
{
	stringio_init();
	wres_event_init();
	wres_lock_init();
	wres_rwlock_init();
	wres_page_lock_init();
	wres_cache_init();
	wres_prio_init();
}


int wres_has_path(wresource_t *resource)
{
	char path[WRES_PATH_MAX] = {0};

	wres_get_resource_path(resource, path);
	return stringio_isdir(path);
}


int wres_check_path(wresource_t *resource)
{
	int ret;
	char path[WRES_PATH_MAX] = {0};

	wres_get_root_path(resource, path);
	if (!stringio_isdir(path)) {
		ret = stringio_mkdir(path);
		if (ret) {
			wres_print_err(resource, "failed to create path");
			return ret;
		}
	}
	wres_get_cls_path(resource, path);
	if (!stringio_isdir(path)) {
		ret = stringio_mkdir(path);
		if (ret) {
			wres_print_err(resource, "failed to create path");
			return ret;
		}
	}
	wres_get_resource_path(resource, path);
	if (!stringio_isdir(path)) {
		ret = stringio_mkdir(path);
		if (ret) {
			wres_print_err(resource, "failed to create path");
			return ret;
		}
	}
	return 0;
}


void wres_clear_path(wresource_t *resource)
{
	char path[WRES_PATH_MAX] = {0};

	wres_get_resource_path(resource, path);
	if (stringio_isdir(path)) {
		if (!stringio_is_empty_dir(path))
			return;
		stringio_rmdir(path);
	}
	
	wres_get_cls_path(resource, path);
	if (stringio_isdir(path)) {
		if (!stringio_is_empty_dir(path))
			return;
		stringio_rmdir(path);
	}

	wres_get_root_path(resource, path);
	if (stringio_isdir(path)) {
		if (!stringio_is_empty_dir(path))
			return;
		stringio_rmdir(path);
	}
}


int wres_check_resource(wresource_t *resource)
{
	wresource_t res = *resource;
	char path[WRES_PATH_MAX] = {0};

	if (WRES_OP_REPLY == wres_get_op(resource)) {
		res.cls = WRES_CLS_TSK;
		res.key = wres_get_id(resource);
	}
	wres_get_resource_path(&res, path);	
	if (stringio_isdir(path))
		return 0;
	else {
		wres_print_err(resource, "no owner");
		return -ENOOWNER;
	}
}


int wres_is_owner(wresource_t *resource)
{
	char path[WRES_PATH_MAX] = {0};
	
	wres_get_state_path(resource, path);
	if (stringio_access(path, F_OK))
		return 0;
	return 1;
}


int wres_has_owner_path(char *path)
{
	char name[WRES_PATH_MAX] = {0};

	strcpy(name, path);
	strcat(name, WRES_PATH_SEPERATOR);
	wres_path_append_state(name);
	if (stringio_access(name, F_OK))
		return 0;
	return 1;
}


int wres_get(wresource_t *resource, wres_desc_t *desc)
{
	char path[WRES_PATH_MAX] = {0};
	
	wres_resource_to_path(resource, path);
	return wres_mds_read(path, (char *)desc, sizeof(wres_desc_t));
}


int wres_save_peer(wres_desc_t *desc)
{
	wresource_t resource;

	if (!desc || (desc->id <= 0)) {
		wres_log("invalid peer");
		return -EINVAL;
	}
	resource.key = desc->id;
	resource.cls = WRES_CLS_TSK;
	return wres_cache_write(&resource, (char *)desc, sizeof(wres_desc_t));
}


int wres_get_peer(wres_id_t id, wres_desc_t *desc)
{
	wresource_t res;

	res.key = id;
	res.cls = WRES_CLS_TSK;
	return wres_lookup(&res, desc);
}


void wres_init_desc(wresource_t *resource, wres_desc_t *desc)
{
	desc->address = local_address;
	desc->id = wres_get_id(resource);
}


int wres_lookup(wresource_t *resource, wres_desc_t *desc)
{
	int ret = wres_cache_read(resource, (char *)desc, sizeof(wres_desc_t));
	
	if (-ENOENT == ret) {
		ret = wres_get(resource, desc);
		if (!ret)
			ret = wres_cache_write(resource, (char *)desc, sizeof(wres_desc_t));
	}
	return ret;
}


int wres_access(wresource_t *resource)
{
	char path[WRES_PATH_MAX] = {0};
	
	wres_resource_to_path(resource, path);
	return wres_mds_access(path);
}


int wres_new(wresource_t *resource)
{
	int ret;
	wres_desc_t desc;
	char path[WRES_PATH_MAX] = {0};
	
	wres_init_desc(resource, &desc);
	wres_resource_to_path(resource, path);
	ret = wres_mds_write(path, (char *)&desc, sizeof(wres_desc_t));
	if (!ret)
		ret = wres_cache_write(resource, (char *)&desc, sizeof(wres_desc_t));
	return ret;
}


int wres_free(wresource_t *resource)
{
	int ret;
	char path[WRES_PATH_MAX] = {0};

	if (wres_is_owner(resource)) {
		wres_resource_to_path(resource, path);
		ret = wres_mds_delete(path);
		if (ret) {
			wres_print_err(resource, "failed to free");
			return ret;
		}
	}
	wres_cache_flush(resource);
	wres_get_resource_path(resource, path);
	stringio_rmdir(path);
	wres_get_cls_path(resource, path);
	if (stringio_is_empty_dir(path))
		stringio_rmdir(path);
	wres_print_free(path);
	return 0;
}


int wres_get_max_key(wres_cls_t cls)
{
	char path[WRES_PATH_MAX] = {0};
	
	wres_path_append_cls(path, cls);
	return wres_mds_get_max_key(path);
}


int wres_get_resource_count(wres_cls_t cls)
{
	char path[WRES_PATH_MAX] = {0};
	
	wres_path_append_cls(path, cls);
	return wres_mds_count(path);
}


void wres_release()
{
	wres_mds_delete("/");
	wres_cache_release();
}
