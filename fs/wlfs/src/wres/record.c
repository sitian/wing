/*      record.c
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

#include "record.h"

int wres_record_check(char *path, wres_index_t index, wres_rec_hdr_t *hdr)
{
	int ret = 0;
	stringio_file_t *filp;
	char name[WRES_PATH_MAX];
	
	strcpy(name, path);
	wres_path_append_index(name, index);
	filp = stringio_open(name, "r");
	if (!filp)
		return -ENOENT;
	if (stringio_read(hdr, sizeof(wres_rec_hdr_t), 1, filp) != 1)
		ret = -EIO;
	stringio_close(filp);
	return ret;
}


int wres_record_get(char *path, wres_index_t index, wres_record_t *record)
{
	wres_rec_hdr_t *hdr;
	stringio_entry_t *entry;
	char name[WRES_PATH_MAX];
	
	strcpy(name, path);
	wres_path_append_index(name, index);
	entry = stringio_get_entry(name, 0, STRINGIO_RDONLY);
	if (!entry)
		return -ENOENT;
	hdr = stringio_get_desc(entry, wres_rec_hdr_t);
	record->entry = entry;
	record->req = (wres_req_t *)&hdr[1];
	return 0;
}


void wres_record_put(wres_record_t *record)
{
	stringio_put_entry(record->entry);
}


int wres_record_save(char *path, wres_req_t *req, wres_index_t *index)
{
	int *curr;
	size_t size;
	char name[WRES_PATH_MAX];
	stringio_entry_t *record;
	stringio_entry_t *checkin;

	strcpy(name, path);
	wres_path_append_checkin(name);
	checkin = stringio_get_entry(name, sizeof(int), STRINGIO_RDWR | STRINGIO_CREAT);
	if (!checkin)
		return -ENOENT;
	curr = stringio_get_desc(checkin, int);
	*index = *curr;

	// record saving
	size = sizeof(wres_rec_hdr_t) + sizeof(wres_req_t) + req->length;
	strcpy(name, path);
	wres_path_append_index(name, *index);
	record = stringio_get_entry(name, size, STRINGIO_RDWR | STRINGIO_CREAT);
	if (!record) {
		wres_log("failed to create");
		stringio_put_entry(checkin);
		return -ENOENT;
	} else {
		wres_rec_hdr_t *hdr = stringio_get_desc(record, wres_rec_hdr_t);
		wres_req_t *preq = (wres_req_t *)&hdr[1];
		hdr->flg = WRES_RECORD_NEW;
		memcpy(preq, req, sizeof(wres_req_t));
		if (req->length > 0)
			memcpy(&preq[1], req->buf, req->length);
	}

	// update index
	if (*curr + 1 >= WRES_INDEX_MAX)
		*curr = 0;
	else
		*curr = *curr + 1;
	
	stringio_put_entry(record);
	stringio_put_entry(checkin);
	wres_print_record(req, name);
	return 0;
}


int wres_record_update(char *path, wres_index_t index, wres_rec_hdr_t *hdr)
{
	int ret = 0;
	char name[WRES_PATH_MAX];
	stringio_file_t *filp;
	
	strcpy(name, path);
	wres_path_append_index(name, index);
	filp = stringio_open(name, "r+");
	if (!filp)
		return -ENOENT;
	if (stringio_write(hdr, sizeof(wres_rec_hdr_t), 1, filp) != 1)
		ret = -EIO;
	stringio_close(filp);
	return ret;
}


int wres_record_first(char *path, wres_index_t *index)
{
	int ret;
	int *curr;
	wres_rec_hdr_t hdr;
	stringio_entry_t *entry;
	char name[WRES_PATH_MAX];
	
	strcpy(name, path);
	wres_path_append_checkout(name);
	entry = stringio_get_entry(name, sizeof(int), STRINGIO_RDONLY | STRINGIO_CREAT);
	if (!entry)
		return -ENOENT;
	curr = stringio_get_desc(entry, int);
	ret = wres_record_check(path, *curr, &hdr);
	if (!ret) {
		if (!(hdr.flg & WRES_RECORD_NEW))
			ret = -EFAULT;
		else
			*index = *curr;
	}
	stringio_put_entry(entry);
	return ret;
}


int wres_record_next(char *path, wres_index_t *index)
{
	int next = *index;
	
	if (*index >= WRES_INDEX_MAX) 
		return -EINVAL;
	do {
		int ret;
		wres_rec_hdr_t hdr;
		if (++next >= WRES_INDEX_MAX)
			next = 0;
		ret = wres_record_check(path, next, &hdr);
		if (ret)
			return ret;	
		if(hdr.flg & WRES_RECORD_NEW) {
			*index = (wres_index_t)next;
			return 0;
		}
	} while (next != *index);
	return -ENOENT;
}


int wres_record_remove(char *path, wres_index_t index)
{
	int *curr;
	int ret = 0;
	wres_rec_hdr_t hdr;
	stringio_entry_t *entry;
	char name[WRES_PATH_MAX];
	
	strcpy(name, path);
	wres_path_append_checkout(name);
	entry = stringio_get_entry(name, sizeof(int), STRINGIO_RDONLY);
	if (!entry) 
		return -ENOENT;
	curr = stringio_get_desc(entry, int);
	
	if (*curr != index) {
		hdr.flg = WRES_RECORD_FREE;
		ret = wres_record_update(path, index, &hdr);
	} else {
		int next = *curr;
		strcpy(name, path);
		wres_path_append_index(name, next);
		stringio_remove(name);
		do {
			if (++next >= WRES_INDEX_MAX)
				next = 0;
			ret = wres_record_check(path, next, &hdr);
			if (ret) {
				if (-ENOENT == ret)
					ret = 0;
				break;
			}
			if (!(hdr.flg & WRES_RECORD_NEW)) {
				strcpy(name, path);
				wres_path_append_index(name, next);
				stringio_remove(name);
			} else
				break;
		} while (next != index);
		*curr = next;
	}
	stringio_put_entry(entry);
	return ret;
}


int wres_record_empty(char *path)
{
	int ret;
	wres_index_t index;
	
	ret = wres_record_first(path, &index);
	if (ret) {
		if (-ENOENT == ret)
			return 1;
		return -EINVAL;
	} else
		return 0;
}

