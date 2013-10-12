/*      tsk.c
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

#include "tsk.h"

int wres_tsk_is_removable(wres_id_t owner)
{
	wresource_t res;
	char path[WRES_PATH_MAX] = {0};

	bzero(&res, sizeof(wresource_t));
	res.owner = owner;
	res.cls = WRES_CLS_SHM;
	wres_get_cls_path(&res, path);
	if (stringio_isdir(path))
		return 0;
	
	res.cls = WRES_CLS_SEM;
	wres_get_cls_path(&res, path);
	if (stringio_isdir(path))
		if (stringio_dir_iter(path, wres_has_owner_path))
			return 0;

	res.cls = WRES_CLS_MSG;
	wres_get_cls_path(&res, path);
	if (stringio_isdir(path))
		if (stringio_dir_iter(path, wres_has_owner_path))
			return 0;

	return 1;
}


int wres_tskget(wresource_t *resource, int *gpid)
{
	int i;
	int ret = 0;
	const int retry_max = 256;
	wres_id_t id = wres_get_id(resource);

	if (!gpid || wres_has_path(resource))
		return -EINVAL;
	
	for (i = 0; i < retry_max; i++) {
		wres_lock(resource);
		ret = wres_new(resource);
		if (ret) {
			wres_unlock(resource);
			if (ret == -EEXIST) {
				if (id < WRES_ID_MAX)
					id += 1;
				else 
					id = 1;
				resource->key = id;	
				resource->owner = id;
				wres_set_id(resource, id);
				continue;
			}
		}
		break;
	}
	if (ret) {
		wres_log("failed to get ID");
		return ret;
	}
	ret = wres_check_path(resource);
	if (ret) {
		wres_log("faild to create path");
		wres_free(resource);
	} else
		*gpid = id;
	wres_unlock(resource);
	return ret;
}


int wres_tskput(wresource_t *resource)
{
	wresource_t res;

	res.cls = WRES_CLS_TSK;
	res.key = resource->owner;
	res.owner = resource->owner;
	wres_lock(&res);
	if (wres_tsk_is_removable(res.owner)) {
		char path[WRES_PATH_MAX];

		wres_free(&res);
		wres_get_root_path(&res, path);
		stringio_rmdir(path);
		wres_print_tskput(path);
	}
	wres_unlock(&res);
	return 0;
}
