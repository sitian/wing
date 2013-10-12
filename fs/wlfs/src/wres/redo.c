/*      redo.c
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

#include "redo.h"

int wres_redo_req(wres_req_t *req, wres_index_t index, int flags)
{
	int ret = 0;
	wres_reply_t *reply;
	wresource_t *resource = &req->resource;
	
	reply = wres_proc(req, flags | WRES_REDO);
	if (reply) {
		if (wres_has_err(reply)) {
			//wres_log("failed to process");
			ret = wres_get_err(reply);
		} else {
			wresource_t res = *resource;
			wres_id_t src = wres_get_id(resource);
			
			wres_set_op(&res, WRES_OP_REPLY);
			wres_set_off(&res, index);
			wres_ioctl(&res, reply->buf, reply->length, NULL, 0, &src);
		}
		free(reply);
	}
	wres_print_redo(resource, req->buf, index);
	return ret;
}


int wres_redo(wresource_t *resource, int flags)
{
	int ret;
	wres_index_t index;
	wres_record_t record;
	char path[WRES_PATH_MAX];

	wres_get_record_path(resource, path);
	ret = wres_record_first(path, &index);
	while (!ret) {
		ret = wres_record_get(path, index, &record);
		if (ret) {
			wres_print_err(resource, "failed to get record");
			return ret;
		}
		ret = wres_redo_req(record.req, index, flags);
		wres_record_put(&record);
		if (!ret)
			wres_record_remove(path, index);
		ret = wres_record_next(path, &index);
	}
	return 0;
}


int wres_redo_all(wresource_t *resource, int flags)
{
	int i;
	int ret;
	int nr_queues;
	wres_index_t index;
	wres_record_t record;
	char path[WRES_PATH_MAX];
	char *pend;

	wres_get_resource_path(resource, path);
	if (!stringio_isdir(path))
		return -ENOENT;
	pend = path + strlen(path);
	nr_queues = wres_get_nr_queues(resource->cls);
	for (i = 0; i < nr_queues; i++) {
		wres_path_append_queue(path, i);
		ret = wres_record_first(path, &index);
		while (!ret) {
			ret = wres_record_get(path, index, &record);
			if (ret) {
				wres_print_err(resource, "failed to get record");
				return ret;
			}
			ret = wres_redo_req(record.req, index, flags);
			wres_record_put(&record);
			if (!ret)
				wres_record_remove(path, index);
			ret = wres_record_next(path, &index);
		}
		pend[0] = '\0';
	}
	return 0;
}
