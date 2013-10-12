/*      member.c
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

#include "member.h"

int wres_member_is_public(wresource_t *resource)
{
	return resource->cls == WRES_CLS_SHM;
}


wres_members_t *wres_member_check(void *entry)
{
	stringio_entry_t *ent = (stringio_entry_t *)entry;
	
	return stringio_get_desc(ent, wres_members_t);
}


void *wres_member_get(wresource_t *resource)
{
	stringio_entry_t *entry;
	char path[WRES_PATH_MAX];

	wres_get_member_path(resource, path);
	entry = stringio_get_entry(path, sizeof(wres_members_t), STRINGIO_RDWR | STRINGIO_AUTO_INCREMENT);
	if (!entry)
		entry = stringio_get_entry(path, sizeof(wres_members_t), STRINGIO_RDWR | STRINGIO_CREAT);
	wres_print_member_get(resource);
	return entry;
}


void wres_member_put(void *entry)
{
	stringio_put_entry((stringio_entry_t *)entry);
	wres_print_member_put(resource);
}


int wres_member_append(void *entry, wres_member_t *member)
{
	return stringio_append(entry, member, sizeof(wres_member_t));
}


int wres_member_get_size(void *entry)
{
	stringio_file_t *filp = ((stringio_entry_t *)entry)->file;

	return stringio_size(filp);
}


int wres_member_new(wresource_t *resource)
{
	int i;
	int total;
	int ret = 0;
	void *entry;
	wres_member_t member;
	wres_members_t *members;
	wres_id_t id = wres_get_id(resource);

	entry = wres_member_get(resource);
	if (!entry) {
		wres_print_err(resource, "no entry");
		return -ENOENT;
	}
	members = wres_member_check(entry);
	total = members->total;
	for (i = 0; i < total; i++) {
		if (members->list[i].id == id) {
			wres_print_err(resource, "already exist");
			ret = -EEXIST;
			goto out;
		}
	}
	if ((total >= WRES_MEMBER_MAX) || (total < 0)) {
		wres_print_err(resource, "out of range");
		ret = -EINVAL;
		goto out;
	}
	total++;
	members->total = total;
	bzero(&member, sizeof(wres_member_t));
	member.id = id;
	ret = wres_member_append(entry, &member);
	if (ret)
		wres_print_err(resource, "failed to append");
out:
	wres_member_put(entry);
	return ret ? ret : total;
}


int wres_member_delete(wresource_t *resource)
{
	int i;
	int ret = 0;
	void *entry;
	wres_members_t *members;
	wres_member_t *member = NULL;
	wres_id_t id = wres_get_id(resource);

	entry = wres_member_get(resource);
	if (!entry) {
		wres_print_err(resource, "no entry");
		return -ENOENT;
	}

	members = wres_member_check(entry);
	if (members->total < 1) {
		wres_print_err(resource, "no entry");
		ret = -ENOENT;
		goto out;
	}
	
	for (i = 0; i < members->total; i++) {
		if (members->list[i].id == id) {
			member = &members->list[i];
			break;
		}
	}
	if (member) {
		member->count = -1;
		members->total--;
	}
out:
	wres_member_put(entry);
	return ret;
}


int wres_member_list(wresource_t *resource, wres_id_t *list)
{
	int i;
	void *entry;
	wres_members_t *members;

	entry = wres_member_get(resource);
	if (!entry) {
		wres_print_err(resource, "no entry");
		return -ENOENT;
	}
	members = wres_member_check(entry);
	for (i = 0; i < members->total; i++)
		list[i] = members->list[i].id;
	wres_member_put(entry);
	return 0;
}


int wres_member_get_pos(wresource_t *resource, wres_id_t id)
{
	int i;
	void *entry;
	int pos = -ENOENT;
	wres_members_t *members;

	wres_rwlock_rdlock(resource);
	entry = wres_member_get(resource);
	if (!entry) {
		wres_print_err(resource, "no entry");
		goto out;
	}
	members = wres_member_check(entry);
	for (i = 0; i < members->total; i++) {
		if (members->list[i].id == id) {
			pos = i;
			break;
		}
	}
	wres_member_put(entry);
out:
	wres_rwlock_unlock(resource);
	return pos;
}


int wres_member_notify(wres_req_t *req)
{
	int i;
	int ret = 0;
	void *entry;
	wres_members_t *members;
	wresource_t *resource = &req->resource;
	wres_id_t src = wres_get_id(resource);

	entry = wres_member_get(resource);
	if (!entry) {
		wres_print_err(resource, "no entry");
		return -ENOENT;
	}
	members = wres_member_check(entry);
	for (i = 0; i < members->total; i++) {
		wres_id_t id = members->list[i].id;
		
		if ((id != resource->owner) && (id != src)) {
			ret = wres_ioctl(resource, req->buf, req->length, NULL, 0, &id);
			if (ret) {
				wres_print_err(resource, "failed to send");
				break;
			}
		}
	}
	wres_member_put(entry);
	return ret;
}


int wres_member_save(wresource_t *resource, wres_id_t *list, int total)
{
	int i;
	int ret = 0;
	void *entry;
	wres_member_t member;
	wres_members_t *members;
	
	if ((total > WRES_MEMBER_MAX) || (total <= 0)) {
		wres_print_err(resource, "invalid member list");
		return -EINVAL;
	}
	entry = wres_member_get(resource);
	if (!entry) {
		wres_print_err(resource, "no entry");
		return -ENOENT;
	}
	members = wres_member_check(entry);
	if (wres_member_get_size(entry) != sizeof(wres_members_t)) {
		wres_print_err(resource, "invalid size");
		ret = -EINVAL;
		goto out;
	}
	members->total = total;
	bzero(&member, sizeof(wres_member_t));
	for (i = 0; i < total; i++) {
		member.id = list[i];
		ret = wres_member_append(entry, &member);
		if (ret)
			break;
	}
out:
	wres_member_put(entry);
	return ret;
}


int wres_member_is_active(wresource_t *resource)
{
	int i;
	int ret = 0;
	void *entry;
	wres_members_t *members;
	wres_id_t src = wres_get_id(resource);

	entry = wres_member_get(resource);
	if (!entry)
		return ret;
	
	members = wres_member_check(entry);
	for (i = 0; i < members->total; i++) {
		wres_member_t *member = &members->list[i];
		
		if (member->id == src) {
			if (member->count >= 0)
				ret = 1;
			break;
		}
	}
	wres_member_put(entry);
	return ret;
}


int wres_member_lookup(wresource_t *resource, wres_member_t *member, int *active_member_count)
{
	int i;
	void *entry;
	int count = 0;
	int ret = -ENOENT;
	wres_members_t *members;
	wres_id_t src = wres_get_id(resource);

	wres_rwlock_rdlock(resource);
	entry = wres_member_get(resource);
	if (!entry) {
		wres_print_err(resource, "no entry");
		goto out;
	}
	members = wres_member_check(entry);
	for (i = 0; i < members->total; i++)
		if (members->list[i].count > 0)
			count++;
	for (i = 0; i < members->total; i++) {
		if (members->list[i].id == src) {
			memcpy(member, &members->list[i], sizeof(wres_member_t));
			ret = 0;
			break;
		}
	}
	wres_member_put(entry);
out:
	wres_rwlock_unlock(resource);
	return ret;
}


int wres_member_update(wresource_t *resource, wres_member_t *member)
{
	int i;
	void *entry;
	int ret = -ENOENT;
	wres_members_t *members;
	wres_id_t src = wres_get_id(resource);

	wres_rwlock_wrlock(resource);
	entry = wres_member_get(resource);
	if (!entry) {
		wres_print_err(resource, "no entry");
		goto out;
	}
	members = wres_member_check(entry);
	for (i = 0; i < members->total; i++) {
		if (members->list[i].id == src) {
			memcpy(&members->list[i], member, sizeof(wres_member_t));
			ret = 0;
			break;
		}
	}
	wres_member_put(entry);
out:
	wres_rwlock_unlock(resource);
	return ret;
}
