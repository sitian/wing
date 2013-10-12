/*      page.c
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

#include "page.h"

int wres_page_lock_stat = 0;
wres_page_lock_group_t wres_page_lock_group[WRES_PAGE_LOCK_GROUP_SIZE];

void wres_page_lock_init()
{
	int i;
	
	if (wres_page_lock_stat & WRES_STAT_INIT)
		return;
	
	for (i = 0; i < WRES_PAGE_LOCK_GROUP_SIZE; i++) {
		pthread_mutex_init(&wres_page_lock_group[i].mutex, NULL);
		wres_page_lock_group[i].head = rbtree_create();
	}
	wres_page_lock_stat |= WRES_STAT_INIT;
}


static inline unsigned long wres_page_lock_hash(wres_page_lock_desc_t *desc)
{
	unsigned long *ent = desc->entry;
	
	return (ent[0] ^ ent[1]) % WRES_PAGE_LOCK_GROUP_SIZE;
}


static inline int wres_page_lock_compare(char *l1, char *l2)
{
	wres_page_lock_desc_t *desc1 = (wres_page_lock_desc_t *)l1;
	wres_page_lock_desc_t *desc2 = (wres_page_lock_desc_t *)l2;
	
	return memcmp(desc1->entry, desc2->entry, sizeof(((wres_page_lock_desc_t *)0)->entry));
}


static inline void wres_page_lock_get_desc(wresource_t *resource, wres_page_lock_desc_t *desc)
{
	desc->entry[0] = resource->key;
	desc->entry[1] = wres_get_off(resource);
}


static inline wres_page_lock_t *wres_page_lock_alloc(wres_page_lock_desc_t *desc)
{
	wres_page_lock_t *lock = (wres_page_lock_t *)malloc(sizeof(wres_page_lock_t));
	
	if (!lock)
		return NULL;
	
	lock->count = 0;
	memcpy(&lock->desc, desc, sizeof(wres_page_lock_desc_t));
	pthread_mutex_init(&lock->mutex, NULL);
	pthread_cond_init(&lock->cond, NULL);
	return lock;
}


static inline wres_page_lock_t *wres_page_lock_lookup(wres_page_lock_group_t *group, wres_page_lock_desc_t *desc)
{
	return rbtree_lookup(group->head, (void *)desc, wres_page_lock_compare);
}


static inline void wres_page_lock_insert(wres_page_lock_group_t *group, wres_page_lock_t *lock)
{
	rbtree_insert(group->head, (void *)&lock->desc, (void *)lock, wres_page_lock_compare);
}


static inline wres_page_lock_t *wres_page_lock_get(wresource_t *resource) 
{
	wres_page_lock_desc_t desc;
	wres_page_lock_group_t *grp;
	wres_page_lock_t *lock = NULL;
	
	wres_page_lock_get_desc(resource, &desc);
	grp = &wres_page_lock_group[wres_page_lock_hash(&desc)];
	pthread_mutex_lock(&grp->mutex);
	lock = wres_page_lock_lookup(grp, &desc);
	if (!lock) {
		lock = wres_page_lock_alloc(&desc);
		if (!lock) {
			wres_print_err(resource, "no memory");
			goto out;
		}
		wres_page_lock_insert(grp, lock);
	}
	pthread_mutex_lock(&lock->mutex);
	lock->count++;
out:
	pthread_mutex_unlock(&grp->mutex);
	return lock;
}


static void wres_page_lock_free(wres_page_lock_group_t *group, wres_page_lock_t *lock)
{
	rbtree_delete(group->head, (void *)&lock->desc, wres_page_lock_compare);
	pthread_mutex_destroy(&lock->mutex); 
	pthread_cond_destroy(&lock->cond);
	free(lock);
}


static int wres_page_lock(wresource_t *resource)
{
	int ret = 0;
	wres_page_lock_t *lock;
	
	if (!(wres_page_lock_stat & WRES_STAT_INIT))
		return -EINVAL;
	
	lock = wres_page_lock_get(resource);
	if (!lock) {
		wres_print_err(resource, "no entry");
		return -ENOENT;
	}
	if (lock->count > 1)
		pthread_cond_wait(&lock->cond, &lock->mutex);
	pthread_mutex_unlock(&lock->mutex);
	wres_print_page_lock(resource);
	return ret; 
}


static void wres_page_unlock(wresource_t *resource)
{
	int empty = 0;
	wres_page_lock_t *lock;
	wres_page_lock_desc_t desc;
	wres_page_lock_group_t *grp;
	
	if (!(wres_page_lock_stat & WRES_STAT_INIT)) 
		return;
	
	wres_page_lock_get_desc(resource, &desc);
	grp = &wres_page_lock_group[wres_page_lock_hash(&desc)];
	pthread_mutex_lock(&grp->mutex);
	lock = wres_page_lock_lookup(grp, &desc);
	if (!lock) {
		wres_print_err(resource, "cannot find the lock");
		pthread_mutex_unlock(&grp->mutex);
		return;
	}
	pthread_mutex_lock(&lock->mutex);
	if (lock->count > 0) {
		lock->count--;
		if (lock->count > 0)
			pthread_cond_signal(&lock->cond);
		else
			empty = 1;
	}
	pthread_mutex_unlock(&lock->mutex);
	if (empty)
		wres_page_lock_free(grp, lock);
	pthread_mutex_unlock(&grp->mutex);
	wres_print_page_unlock(resource);
}


void *wres_page_get(wresource_t *resource, wres_page_t **page, int flags)
{
	stringio_entry_t *entry;
	char path[WRES_PATH_MAX];
	
	wres_get_data_path(resource, path);
	wres_page_lock(resource);
	entry = stringio_get_entry(path, sizeof(wres_page_t), flags & (WRES_RDWR | WRES_RDONLY | WRES_CREAT));
	if (!entry) {
		wres_page_unlock(resource);
		return NULL;
	}
	*page = stringio_get_desc(entry, wres_page_t);
	wres_print_page_get(resource, path);
	return entry;
}


void wres_page_put(wresource_t *resource, void *entry)
{
	stringio_put_entry((stringio_entry_t *)entry);
	wres_page_unlock(resource);
	wres_print_page_put(resource);
}


static int wres_page_update(wres_page_t *page, char *buf)
{
	int i, j;
	int ret = 0;
	int dirty = 0;
	int diff[WRES_LINE_MAX];
	wres_digest_t digest[WRES_LINE_MAX];
	
	for (i = 0; i < WRES_LINE_MAX; i++)
		digest[i] = wres_line_get_digest(&buf[i * WRES_LINE_SIZE]);
	
	for (i = 0; i < WRES_LINE_MAX; i++) {
		j = i * WRES_LINE_SIZE;
		if (page->digest[i] != digest[i]) {
			dirty = 1;
			diff[i] = 1;
			memcpy(&page->buf[j], &buf[j], WRES_LINE_SIZE);
		} else {
			if (!memcmp(&page->buf[j], &buf[j], WRES_LINE_SIZE))
				diff[i] = 0;
			else {
				dirty = 1;
				diff[i] = 1;
				memcpy(&page->buf[j], &buf[j], WRES_LINE_SIZE);
			}
		}
	}
	
	if (dirty) {
		for (i = WRES_PAGE_NR_VERSIONS - 1; i > 0; i--)
			for (j = 0; j < WRES_LINE_MAX; j++)
				page->diff[i][j] |= page->diff[i - 1][j] | diff[j];

		for (j = 0; j < WRES_LINE_MAX; j++) {
			page->diff[0][j] = diff[j];
			page->digest[j] = digest[j];
		}
	}
	return ret;
}


wres_index_t wres_page_update_index(wres_page_t *page)
{
	page->index++;
	if (WRES_INDEX_MAX == page->index)
		page->index = 1;
	return page->index;
}


void wres_page_update_candidates(wresource_t *resource, wres_page_t *page, wres_id_t *holders, int nr_holders)
{
	int i, j, k = 0;
	int total = page->nr_candidates;
	wres_id_t nu[WRES_PAGE_NR_HOLDERS];
	wres_member_t *cand = page->candidates;
	
	for (i = 0; i < nr_holders; i++) {
		if (holders[i] == resource->owner)
			continue;
		for (j = 0; j < total; j++) {
			if (holders[i] == cand[j].id) {
				if (cand[j].count < WRES_PAGE_NR_HITS) {
					cand[j].count++;
					while ((j > 0) && (cand[j].count > cand[j - 1].count)) {
						wres_member_t tmp = cand[j - 1];
						cand[j - 1] = cand[j];
						cand[j] = tmp;
						j--;
					}
				}
				break;
			}
		}
		if (j == total) {
			nu[k] = holders[i];
			k++;
		}
	}
	if (cand[0].count == WRES_PAGE_NR_HITS) {
		for (i = 0; i < total; i++) {
			if (cand[i].count > 0)
				cand[i].count--;
			else
				break;
		}
	}
	if (k > 0) {
		if (!total || ((cand[total - 1].count > 0) && (total < WRES_PAGE_NR_HOLDERS)))
			j = total;
		else {
			if (cand[total - 1].count > 0)
				for (j = 0; j < total; j++)
					cand[j].count--;
			
			if (0 == cand[total - 1].count) {
				for (j = total - 1; j > 0; j--)
					if (cand[j - 1].count > 0)
						break;
			} else
				return;
		}
		if (j + k > total)
			page->nr_candidates = min(j + k, WRES_PAGE_NR_CANDIDATES);
		for (i = 0; (i < k) && (j < page->nr_candidates); i++, j++) {
			cand[j].id = nu[i];
			cand[j].count = 1;
		}
	}
}


int wres_page_get_diff(wres_page_t *page, wres_version_t version, int *diff)
{
	int interval = 0;
	const size_t size = WRES_LINE_MAX * sizeof(int);
	
	if (page->version >= version)
		interval = page->version - version;
	else
		return -EINVAL;
	
	if (0 == interval)
		memset(diff, 0, size);
	else if (interval <= WRES_PAGE_NR_VERSIONS)
		memcpy(diff, &page->diff[interval - 1], size);
	else {
		int i;
		
		for (i = 0; i < WRES_LINE_MAX; i++)
			diff[i] = 1;
	}
	return 0;
}


int wres_page_calc_holders(wres_page_t *page)
{
	return page->nr_holders + page->nr_silent_holders;
}


int wres_page_search_holder_list(wresource_t *resource, wres_page_t *page, wres_id_t id)
{
	int i;
	int nr_holders = page->nr_holders;
	int nr_silent_holders = page->nr_silent_holders;
	
	for (i = 0; i < nr_holders; i++)
		if (page->holders[i] == id)
			return 1;

	if (nr_silent_holders > 0) {
		stringio_entry_t *entry;
		char path[WRES_PATH_MAX];
		
		wres_get_holder_path(resource, path);
		entry = stringio_get_entry(path, nr_silent_holders * sizeof(wres_id_t), STRINGIO_RDONLY);
		if (entry) {
			int ret = 0;
			wres_id_t *holders;
			
			holders = stringio_get_desc(entry, wres_id_t);
			for (i = 0; i < nr_silent_holders; i++) {
				if (holders[i] == id) {
					ret = 1;
					break;
				}
			}
			stringio_put_entry(entry);
			return ret;
		}
	}
	return 0;
}


int wres_page_update_holder_list(wresource_t *resource, wres_page_t *page, wres_id_t *holders, int nr_holders)
{
	int total;
	int ret = 0;
	int nr_silent_holders;
	
	if (!nr_holders)
		return 0;
	
	total = page->nr_holders + nr_holders;
	nr_silent_holders = total > WRES_PAGE_NR_HOLDERS ? total - WRES_PAGE_NR_HOLDERS : 0;
	nr_holders -= nr_silent_holders;
	
	if (nr_holders > 0)
		memcpy(&page->holders[page->nr_holders], holders, nr_holders * sizeof(wres_id_t));

	if (nr_silent_holders > 0) {
		stringio_file_t *filp;
		char path[WRES_PATH_MAX];
		
		wres_get_holder_path(resource, path);
		filp = stringio_open(path, "r+");
		if (!filp) {
			filp = stringio_open(path, "w");
			if (!filp) {
				wres_print_err(resource, "no entry");
				return -ENOENT;
			}
		}
		if (stringio_size(filp) / sizeof(wres_id_t) != page->nr_silent_holders) {
			wres_print_err(resource, "invalid file length");
			ret = -EINVAL;
			goto out;
		}
		stringio_seek(filp, 0, SEEK_END);
		if (stringio_write(&holders[nr_holders], sizeof(wres_id_t), nr_silent_holders, filp) != nr_silent_holders) {
			wres_print_err(resource, "failed to update holder list");
			ret = -EIO;
		}
out:
		stringio_close(filp);
	}
	if (!ret) {
		page->nr_holders += nr_holders;
		page->nr_silent_holders += nr_silent_holders;
	}
	wres_page_update_candidates(resource, page, holders, nr_holders);
	return ret;
}


int wres_page_add_holder(wresource_t *resource, wres_page_t *page, wres_id_t id)
{
	return wres_page_update_holder_list(resource, page, &id, 1);
}


void wres_page_clear_holder_list(wresource_t *resource, wres_page_t *page)
{
	page->nr_holders = 0;
	if (page->nr_silent_holders > 0) {
		stringio_file_t *filp;
		char path[WRES_PATH_MAX];
		
		wres_get_holder_path(resource, path);
		filp = stringio_open(path, "r+");
		if (filp) {
			stringio_truncate(filp, 0);
			stringio_close(filp);
		}
		page->nr_silent_holders = 0;
	}
}


int wres_page_get_hid(wres_page_t *page, wres_id_t id)
{
	int i;
	
	for (i = 0; i < page->nr_holders; i++)
		if (id == page->holders[i])
			return i + 1;
	return 0;
}


static int wres_page_wrprotect(wresource_t *resource, wres_page_t *page)
{	
	int ret;
	char *buf;
	
	if (!wres_pg_active(page) || !wres_pg_write(page))
		return 0;

	buf = malloc(PAGE_SIZE);
	if (!buf) {
		wres_print_err(resource, "no memory");
		return -ENOMEM;
	}
	ret = sys_shm_wrprotect(resource->key, wres_get_off(resource), buf);
	if (ret) {
		if (ENOENT == errno) {
			ret = 0;
			goto protect;
		} else
			goto out;
	}
	ret = wres_page_update(page, buf);
	if (ret) {
		wres_print_err(resource, "failed to update");
		goto out;
	}
protect:
	wres_pg_wrprotect(page);
out:
	free(buf);
	return ret;
}


static int wres_page_rdprotect(wresource_t *resource, wres_page_t *page)
{
	int ret = 0;
	char *buf = NULL;
	
	if (!wres_pg_active(page) || !wres_pg_access(page))
		return 0;

	if (wres_pg_wait(page))
		goto protect;

	if (wres_pg_write(page)) {
		buf = malloc(PAGE_SIZE);
		if (!buf) {
			wres_print_err(resource, "no memory");
			return -ENOMEM;
		}
	}
	ret = sys_shm_rdprotect(resource->key, wres_get_off(resource), buf);
	if (ret) {
		if (ENOENT == errno) {
			ret = 0;
			goto protect;
		} else {
			wres_print_err(resource, "failed to protect (%d)", errno);
			goto out;
		}
	}
	if (buf) {
		ret = wres_page_update(page, buf);
		if (ret) {
			wres_print_err(resource, "failed to update");
			goto out;
		}
	}
protect:
	wres_pg_clractive(page);
	wres_pg_rdprotect(page);
out:
	if (buf)
		free(buf);
	return ret;
}


int wres_page_protect(wresource_t *resource, wres_page_t *page)
{
	int flags = wres_get_flags(resource);
	
	if (flags & WRES_RDONLY)
		return wres_page_wrprotect(resource, page);
	else if (flags & WRES_RDWR)
		return wres_page_rdprotect(resource, page);
	return 0;
}
