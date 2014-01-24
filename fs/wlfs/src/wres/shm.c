/*      shm.c
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

#include "shm.h"

int wres_shm_htab[WRES_PAGE_NR_HOLDERS][WRES_LINE_MAX];

static inline int wres_shm_calc_htab(int *htab, size_t length, int nr_holders)
{
	int i, j;
	
	if (nr_holders <= 0) {
		wres_log("failed, nr_holders=%d", nr_holders);
		return -EINVAL;
	}
	
	if (nr_holders <= length) {
		for (i = 1; i <= nr_holders; i++)
			for (j = 0; j < length; j++)
				if (j % i == 0)
					htab[j] = i;
	} else {
		for (j = 0; j < length; j++)
			htab[j] = j + 1;
	}
	return 0;
}


void wres_shm_init_htab()
{
	int i;
	static int init = 0;
	
	if (init)
		return;
	else
		init = 1;
	
	for (i = 0; i < WRES_PAGE_NR_HOLDERS; i++)
		wres_shm_calc_htab(wres_shm_htab[i], WRES_LINE_MAX, i + 1);
}


int wres_shm_check(wresource_t *resource, struct shmid_ds *shp)
{
	int ret = 0;
	stringio_file_t *filp;
	char path[WRES_PATH_MAX];
	
	wres_get_state_path(resource, path);
	filp = stringio_open(path, "r");
	if (!filp)
		return -ENOENT;
	
	if (stringio_read(shp, sizeof(struct shmid_ds), 1, filp) != 1)
		ret = -EIO;
	
	stringio_close(filp);
	return ret;
}


int wres_shm_init(wresource_t *resource)
{
	int ret = 0;
	wres_time_t off = 0;

	wres_shm_init_htab();
	if (!wres_is_owner(resource)) {
		ret = wres_synctime_request(resource, &off);
		if (ret) {
			wres_print_err(resource, "failed to sync time");
			return ret;
		}
	}
	ret = wres_prio_create(resource, off);
	if (ret) {
		wres_print_err(resource, "failed to create prio");
		return ret;
	}
	return 0;
}


int wres_shm_create(wresource_t *resource)
{
	stringio_file_t *filp;
	struct shmid_ds shmid_ds;
	char path[WRES_PATH_MAX];
	size_t size = wres_entry_val1(resource->entry);
	
	if ((size < WRES_SHMMIN) || (size > WRES_SHMMAX))
		return -EINVAL;

	bzero(&shmid_ds, sizeof(struct shmid_ds));
	shmid_ds.shm_perm.__key = resource->key;
	shmid_ds.shm_perm.mode = wres_get_flags(resource) & S_IRWXUGO;
	shmid_ds.shm_cpid = wres_get_id(resource);
	shmid_ds.shm_ctime = time(0);
	shmid_ds.shm_segsz = size;

	wres_get_state_path(resource, path);
	filp = stringio_open(path, "w");
	if (!filp) {
		wres_print_err(resource, "failed to create shm");
		return -ENOENT;
	}
	if (stringio_write((char *)&shmid_ds, sizeof(struct shmid_ds), 1, filp) != 1) {
		stringio_close(filp);
		return -EIO;
	}
	stringio_close(filp);
	return 0;
}


static inline void wres_shm_pack_diff(int (*diff)[WRES_LINE_MAX], char *out)
{
	int i, j;
	int ver = 0, line = 0;
	
	for (i = 0; i < WRES_PAGE_DIFF_SIZE; i++) {
		char ch = 0;
		
		for (j = 0; j < BITS_PER_BYTE; j++) {
			ch = ch << 1 | diff[ver][line]; 
			line++;
			if (WRES_LINE_MAX == line) {
				line = 0;
				ver++;
			}
		}
		out[i] = ch;
	}
}


static inline void wres_shm_unpack_diff(int (*dest)[WRES_LINE_MAX], char *src)
{
	int i, j;
	int ver = 0, line = 0;
	
	for (i = 0; i < WRES_PAGE_DIFF_SIZE; i++) {
		for (j = BITS_PER_BYTE - 1; j >= 0; j--) {
			dest[ver][line] = (src[i] >> j) & 1;
			line++;
			if (WRES_LINE_MAX == line) {
				line = 0;
				ver++;
			}
		}
	}
}


int wres_shm_save_req(wres_page_t *page, wres_req_t *req)
{
	int ret;
	wres_index_t index = 0;
	char path[WRES_PATH_MAX];
	wresource_t *resource = &req->resource;

	wres_get_record_path(resource, path);
	ret = wres_record_save(path, req, &index);
	if (ret) {
		wres_print_err(resource, "failed to save request");
		return ret;
	}
	wres_pg_mkredo(page);
	wres_print_shm_save_req(req);
	return 0;
}


int *wres_shm_get_htab(wres_page_t *page)
{
	if (page->nr_holders <= 0)
		return NULL;
	
	return wres_shm_htab[page->nr_holders - 1];
}


void wres_shm_info(struct shminfo *shminfo)
{
	bzero(shminfo, sizeof(struct shminfo));
	shminfo->shmmax = WRES_SHMMAX;
	shminfo->shmmin = WRES_SHMMIN;
	shminfo->shmmni = WRES_SHMMNI;
	shminfo->shmseg = WRES_SHMSEG;
	shminfo->shmall = WRES_SHMALL;
}


int wres_shm_update(wresource_t *resource, struct shmid_ds *shp)
{
	int ret = 0;
	char path[WRES_PATH_MAX];
	stringio_file_t *filp;
	
	wres_get_state_path(resource, path);
	filp = stringio_open(path, "w");
	if (!filp)
		return -ENOENT;
	
	if (stringio_write(shp, sizeof(struct shmid_ds), 1, filp) != 1)
		ret = -EIO;
	
	stringio_close(filp);
	return ret;
}


static inline int wres_shm_get_peer_info(wresource_t *resource, wres_page_t *page, wres_shm_peer_info_t *info)
{	
	int flags = wres_get_flags(resource);
	
	if (flags & WRES_RDWR) {
		wres_id_t src = wres_get_id(resource);
		int overlap = wres_page_search_holder_list(resource, page, src);
		
		info->total = wres_page_calc_holders(page) - overlap;
	} else if (flags & WRES_RDONLY) {
		int i;
		
		for (i = 0; i < page->nr_holders; i++) { 
			int ret = wres_get_peer(page->holders[i], &info->list[i]);
			
			if (ret)
				return ret;
		}
		info->total = page->nr_holders;
	}
	return wres_get_peer(page->owner, &info->owner);
}


#ifdef MODE_FAST_REPLY
static inline int wres_shm_get_peer_info_fast(wresource_t *resource, wres_page_t *page, wres_shm_peer_info_t *info)
{	
	int flags = wres_get_flags(resource);
	int ret = wres_shm_get_peer_info(resource, page, info);
	
	if (!ret && (flags & WRES_RDWR))
		if (!wres_page_search_holder_list(resource, page, resource->owner))
			info->total++;
	return ret;
}


static inline int wres_shm_cache_add(wres_page_t *page, wres_req_t *req)
{
	int ret = 0;
	int updated = 0;
	char path[WRES_PATH_MAX];
	wres_shmfault_args_t *args = (wres_shmfault_args_t *)req->buf;
	wresource_t *resource = &req->resource;
	wres_version_t version = page->version;
	wres_id_t id = wres_get_id(resource);
	wres_index_t index = args->index;
	stringio_entry_t *entry;
	
	wres_get_cache_path(resource, path);
	entry = stringio_get_entry(path, 0, STRINGIO_RDWR | STRINGIO_AUTO_INCREMENT);
	if (entry) {
		int total = stringio_items_count(entry, sizeof(wres_shm_cache_t));
		
		if (total > 0) {
			int i;
			wres_shm_cache_t *caches = stringio_get_desc(entry, wres_shm_cache_t);
			
			for (i = 0; i < total; i++) {
				if (caches[i].id == id) {
					updated = 1;
					caches[i].index = index;
					caches[i].version = version;
					break;
				}
			}
		}
	}
	if (!updated) {
		wres_shm_cache_t cache;
		
		if (!entry) {
			entry = stringio_get_entry(path, 0, STRINGIO_RDWR | STRINGIO_CREAT);
			if (!entry) {
				wres_print_err(resource, "no entry");
				return -ENOENT;
			}
		}
		cache.index = index;
		cache.version = version;
		cache.id = id;
		ret = stringio_append(entry, &cache, sizeof(wres_shm_cache_t));
	}
	stringio_put_entry(entry);
	wres_print_shm_cache(resource, id, index, version);
	return ret;
}


static inline int wres_shm_cache_hit(wres_page_t *page, wres_req_t *req)
{
	int i;
	int total;
	int ret = 0;
	char path[WRES_PATH_MAX];
	wres_shmfault_args_t *args = (wres_shmfault_args_t *)req->buf;
	wresource_t *resource = &req->resource;
	wres_version_t version = page->version;
	wres_id_t id = wres_get_id(resource);
	wres_index_t index = args->index;
	wres_shm_cache_t *caches;
	stringio_entry_t *entry;
		
	wres_get_cache_path(resource, path);
	entry = stringio_get_entry(path, 0, STRINGIO_RDONLY | STRINGIO_AUTO_INCREMENT);
	if (!entry)
		return 0;
	
	caches = stringio_get_desc(entry, wres_shm_cache_t);
	total = stringio_items_count(entry, sizeof(wres_shm_cache_t));
	for (i = 0; i < total; i++) {
		if ((caches[i].id == id) && (caches[i].index == index) && (caches[i].version >= version)) {
			wres_print_shm_cache(resource, id, index, caches[i].version);
			ret = 1;
			break;
		}
	}
	stringio_put_entry(entry);
	return ret;
}


inline int wres_shm_check_fast_reply(wres_page_t *page, wres_req_t *req, int *hid, int *nr_peers)
{
	int i, j;
	int cnt = 0;
	wres_id_t peers[WRES_PAGE_NR_HOLDERS];
	wresource_t *resource = &req->resource;
	wres_shmfault_args_t *args = (wres_shmfault_args_t *)req->buf;

	if (page->nr_holders < 2)
		return -EAGAIN;
	
	*hid = 0;
	*nr_peers = 0;
	for (i = 0; i < page->nr_holders; i++) {
		for (j = 0; j < args->peers.total; j++) {
			if (args->peers.list[j] == page->holders[i]) {
				peers[cnt] = args->peers.list[j];
				cnt++;
				break;
			}
		}
	}
	
	if (cnt < 1)
		return -EAGAIN;

	*nr_peers = cnt;
	for (i = 0; i < cnt; i++) {
		if (peers[i] == resource->owner) {
			*hid = i + 1;
			break;
		}
	}
	wres_print_shm_check_fast_reply(req, *hid, *nr_peers);
	return 0;
}


int wres_shm_fast_reply(wres_page_t *page, wres_req_t *req)
{
	int i;
	int clk;
	int hid = 0;
	int ret = 0;
	int head = 0;
	int tail = 0;
	int total = -1;
	int nr_lines = 0;
	int nr_peers = 0;
	int diff[WRES_LINE_MAX];
	int htab[WRES_LINE_MAX];
	wresource_t *resource = &req->resource;
	wres_shmfault_args_t *args = (wres_shmfault_args_t *)req->buf;
	int notify = WRES_SHM_NOTIFY_HOLDER == args->cmd;
	int flags = wres_get_flags(resource);
	int reply = notify && (flags & WRES_RDWR);
	
	if (!wres_pg_own(page)) {
		if (!wres_pg_active(page))
			return 0;
		clk = page->version;	
	} else {
		tail = 1;
		clk = page->oclk;
	}
	
	ret = wres_shm_check_fast_reply(page, req, &hid, &nr_peers);
	if (ret)
		goto cache;
	
	if (wres_pg_active(page) && (hid > 0)) {
		bzero(diff, sizeof(diff));
		if (wres_page_get_diff(page, args->version, diff)) {
			wres_print_err(resource, "failed to get diff");
			return -EINVAL;
		}
		if (wres_shm_calc_htab(htab, WRES_LINE_MAX, nr_peers)) {
			wres_print_err(resource, "failed to calculate htab");
			return -EINVAL;
		}
		total = 0;
		for (i = 0; i < WRES_LINE_MAX; i++) {
			if (diff[i] != 0) {
				total++;
				if (htab[i] == hid) {
					nr_lines++;
					if (1 == total)
						head = 1;
				}
			}
		}
		if (!total && !reply)
			reply = hid == 1;
	}
	
	if ((nr_lines > 0) || reply || head || tail) {
		char *ptr;
		wresource_t res = *resource;
		wres_id_t src = wres_get_id(resource);		
		wres_shm_notify_proposer_args_t *new_args;
		size_t size = sizeof(wres_shm_notify_proposer_args_t) + nr_lines * sizeof(wres_line_t);
		
#ifdef MODE_SELF_CHECK
		if (wres_page_self_check(page)) {
			wres_print_err(resource, "self check failed");
			return -EINVAL;
		}
#endif
		if (head)
			size += WRES_PAGE_DIFF_SIZE;
		if (tail) {
			size += sizeof(wres_shm_peer_info_t);
			if (flags & WRES_RDONLY)
				size += page->nr_holders * sizeof(wres_desc_t);
		}
		new_args = (wres_shm_notify_proposer_args_t *)calloc(1, size);
		if (!new_args) {
			wres_print_err(resource, "no memory");
			return -ENOMEM;
		}
		if (nr_lines > 0) {
			int j = 0;
			
			for (i = 0; i < WRES_LINE_MAX; i++) {
				if (diff[i] && (htab[i] == hid)) {
					wres_line_t *line = &new_args->lines[j];
					line->digest = page->digest[i];
					line->num = i;
					memcpy(line->buf, &page->buf[i * WRES_LINE_SIZE], WRES_LINE_SIZE);
					j++;
				}
			}
		}
		memcpy(&new_args->args, args, sizeof(wres_shmfault_args_t));
		new_args->args.cmd = WRES_SHM_NOTIFY_PROPOSER;
		new_args->args.clk = clk;
		new_args->nr_lines = nr_lines;
		new_args->total = total;

		ptr = (char *)&new_args[1] + nr_lines * sizeof(wres_line_t);
		if (head) {
			wres_shm_pack_diff(page->diff, ptr);
			ptr += WRES_PAGE_DIFF_SIZE;
			wres_set_flags(&res, WRES_DIFF);
		}
		if (tail) {
			ret = wres_shm_get_peer_info_fast(resource, page, (wres_shm_peer_info_t *)ptr);
			if (ret) {
				wres_print_err(resource, "failed to get peer info");
				goto out;
			}
			new_args->args.owner = page->owner;
			wres_set_flags(&res, WRES_PEER);
		}
		if (wres_ioctl(&res, (char *)new_args, size, NULL, 0, &src)) {
			wres_print_err(resource, "failed to send request");
			ret = -EFAULT;
		}
out:
		wres_print_lines(resource, new_args->lines, nr_lines, total);
		free(new_args);
		if (ret)
			return ret;
	}
	wres_print_shm_fast_reply(resource, hid, nr_peers);
cache:
	if (wres_pg_own(page) || (hid > 0) || notify) {
		if (wres_shm_cache_add(page, req)) {
			wres_print_err(resource, "failed to cache");
			ret = -EFAULT;
		}
	}
	return ret;
}
#endif


int wres_shm_send_to_holders(wres_page_t *page, wres_req_t *req)
{
	int i;
	int ret = 0;
	int count = 0;
	pthread_t *thread = NULL;
	wres_shmfault_args_t *args = (wres_shmfault_args_t *)req->buf;
	wresource_t *resource = &req->resource;
	wres_id_t src = wres_get_id(resource);
	int cmd = args->cmd;
	
	if (req->length > WRES_IO_MAX) {
		wres_print_err(resource, "too long request");
		return -EINVAL;
	}
	
	// Note that: the clk piggybacked on the request has to be updated.
	args->clk = page->oclk;
	args->owner = resource->owner;
	thread = malloc(page->nr_holders * sizeof(pthread_t));
	if (!thread) {
		wres_print_err(resource, "no memory");
		return -ENOMEM;
	}
	args->cmd = WRES_SHM_NOTIFY_HOLDER;
	for (i = 0; i < page->nr_holders; i++) {
		if ((page->holders[i] != src) && (page->holders[i] != resource->owner)) {
			ret = wres_ioctl_async(resource, req->buf, req->length, NULL, 0, &page->holders[i], &thread[count]);
			if (ret) {
				wres_print_err(resource, "failed to send");
				break;
			}
			count++;
		}
	}
	for (i = 0; i < count; i++)
		pthread_join(thread[i], NULL);
	args->cmd = cmd;
	free(thread);
	return ret;
}


int wres_shm_update_holder(wres_page_t *page, wres_req_t *req)
{
	int ret;
	wresource_t *resource = &req->resource;
	wres_id_t src = wres_get_id(resource);
	int flags = wres_get_flags(resource);
	
	if (flags & WRES_RDONLY) {
		ret = wres_page_add_holder(resource, page, src);
		if (ret) {
			wres_print_err(resource, "failed to add holder");
			return ret;
		}
	} else if (flags & WRES_RDWR) {
		wres_page_clear_holder_list(resource, page);
		
		if (wres_pg_own(page)) {
			ret = wres_page_add_holder(resource, page, src);
			if (ret) {
				wres_print_err(resource, "failed to add holder");
				return ret;
			}
			page->oclk += 1;
		}
	}
	ret = wres_page_protect(resource, page);
	if (ret) {
		wres_print_err(resource, "failed to protect");
		return ret;
	}
#ifdef MODE_DYNAMIC_OWNER
	if (wres_get_flags(resource) & WRES_OWN) {
		if (wres_pg_own(page)) {
			wres_pg_clrown(page);
			if (!wres_pg_active(page))
				wres_page_clear_holder_list(resource, page);
		}
		page->owner = src;
	} else if (!wres_pg_own(page)) {
		wres_shmfault_args_t *args = (wres_shmfault_args_t *)req->buf;
		page->owner = args->owner;
	}
#endif
	wres_print_shm_update_holder(resource, page);
	return 0;
}


int wres_shm_send_to_silent_holders(wres_page_t *page, wres_req_t *req)
{
	int ret = 0;
	wresource_t *resource = &req->resource;
	wres_shmfault_args_t *args = (wres_shmfault_args_t *)req->buf;
	int flags = wres_get_flags(resource);
	
	if (req->length > WRES_IO_MAX) {
		wres_print_err(resource, "too long request");
		return -EINVAL;
	}
	
	if ((flags & WRES_RDWR) && !wres_shm_is_silent_holder(page) && wres_pg_active(page)) {
		if (page->nr_silent_holders > 0) {
			int i;
			int count = 0;
			int *htab = NULL;
			int hid = page->hid;
			int cmd = args->cmd;
			int nr_sources = page->nr_holders;
			int nr_silent_holders = page->nr_silent_holders;
			wres_id_t src = wres_get_id(resource);
			wres_id_t *silent_holders;
			char path[WRES_PATH_MAX];
			stringio_entry_t *entry;
			pthread_t *thread;
			
			wres_get_holder_path(resource, path);
			entry = stringio_get_entry(path, nr_silent_holders * sizeof(wres_id_t), STRINGIO_RDONLY);
			if (!entry) {
				wres_print_err(resource, "no entry");
				return -ENOENT;
			}
			silent_holders = stringio_get_desc(entry, wres_id_t);
			htab = (int *)malloc(sizeof(int) * nr_silent_holders);
			if (!htab) {
				wres_print_err(resource, "no memory");
				stringio_put_entry(entry);
				return -ENOMEM;
			}
			for (i = 0; i < nr_sources; i++) {
				if (page->holders[i] == src) {
					nr_sources -= 1;
					if (hid > i + 1)
						hid -= 1;
					break;
				}
			}
			ret = wres_shm_calc_htab(htab, nr_silent_holders, nr_sources);
			if (ret) {
				wres_print_err(resource, "failed to calculate htab");
				goto out;
			}
			thread = malloc(nr_silent_holders * sizeof(pthread_t));
			if (!thread) {
				wres_print_err(resource, "no memory");
				ret = -ENOMEM;
				goto out;
			}
			args->cmd = WRES_SHM_NOTIFY_HOLDER;
			for (i = 0; i < nr_silent_holders; i++) {
				if ((htab[i] == hid) && (silent_holders[i] != src) && (silent_holders[i] != args->owner)) {
					ret = wres_ioctl_async(resource, req->buf, req->length, NULL, 0, &silent_holders[i], &thread[count]);
					if (ret) {
						wres_print_err(resource, "failed to create process");
						break;
					}
					count++;
				}
			}
			for (i = 0; i < count; i++)
				pthread_join(thread[i], NULL);
			args->cmd = cmd;
			free(thread);
out:
			free(htab);
			stringio_put_entry(entry);
		}
	}
	return ret;
}


static inline int wres_shm_can_cover(wres_page_t *page, int line)
{
	int n = page->nr_holders - 1;
	
	if ((n >= 0) && (n < WRES_PAGE_NR_HOLDERS) 
	    && (line >= 0) && (line < WRES_LINE_MAX))
		return wres_shm_htab[n][line] == page->hid;
	return 0;
}


int wres_shm_do_check_holder(wres_page_t *page, wres_req_t *req)
{
	int i;
	int ret = 0;
	int tail = 0;
	int head = 0;
	int total = 0;
	int nr_lines = 0;
	int diff[WRES_LINE_MAX];
	wres_shmfault_args_t *args = (wres_shmfault_args_t *)req->buf;
	wresource_t *resource = &req->resource;
	wres_id_t src = wres_get_id(resource);	
	int flags = wres_get_flags(resource);
	int reply = flags & WRES_RDWR;

	if (!wres_pg_active(page))
		return 0;
	
	ret = wres_page_protect(resource, page);
	if (ret) {
		wres_print_err(resource, "failed to protect");
		return ret;
	}
	
	if (wres_page_get_diff(page, args->version, diff)) {
		wres_print_err(resource, "failed to get diff");
		return -EFAULT;
	}
	
	for (i = 0; i < WRES_LINE_MAX; i++) {
		if (diff[i] != 0) {
			total++;
			if (wres_shm_can_cover(page, i)) {
				nr_lines++;
				if (1 == total)
					head = 1;
			}
		}
	}
	
	if (src != page->owner) {
		if (!total) {
			if (page->holders[0] != src)
				tail = page->hid == 1;
			else
				tail = page->hid == 2;
		} else
			tail = head;
	} else if (!total && !reply) {
		if (page->holders[0] != src)
			reply = page->hid == 1;
		else
			reply = page->hid == 2;
	}
	
	if ((nr_lines > 0) || reply || head || tail) {
		char *ptr;
		wresource_t res = *resource;
		wres_shm_notify_proposer_args_t *new_args;
		size_t size = sizeof(wres_shm_notify_proposer_args_t) + nr_lines * sizeof(wres_line_t);

#ifdef MODE_SELF_CHECK
		if (wres_page_self_check(page)) {
			wres_print_err(resource, "self check failed");
			return -EINVAL;
		}
#endif
		if (head)
			size += WRES_PAGE_DIFF_SIZE;
		if (tail) {
			size += sizeof(wres_shm_peer_info_t);
			if (flags & WRES_RDONLY)
				size += page->nr_holders * sizeof(wres_desc_t);
		}
		new_args = (wres_shm_notify_proposer_args_t *)calloc(1, size);
		if (!new_args) {
			wres_print_err(resource, "no memory");
			return -ENOMEM;
		}
		if (nr_lines > 0) {
			int j = 0;
			
			for (i = 0; i < WRES_LINE_MAX; i++) {
				if (diff[i] & wres_shm_can_cover(page, i)) {
					wres_line_t *line = &new_args->lines[j];
					line->digest = page->digest[i];
					line->num = i;
					memcpy(line->buf, &page->buf[i * WRES_LINE_SIZE], WRES_LINE_SIZE);
					j++;
				}
			}
		}
		memcpy(&new_args->args, args, sizeof(wres_shmfault_args_t));
		new_args->args.cmd = WRES_SHM_NOTIFY_PROPOSER;
		new_args->args.clk = page->version;
		new_args->nr_lines = nr_lines;
		new_args->total = total;

		ptr = (char *)&new_args[1] + nr_lines * sizeof(wres_line_t);
		if (head) {
			wres_shm_pack_diff(page->diff, ptr);
			ptr += WRES_PAGE_DIFF_SIZE;
			wres_set_flags(&res, WRES_DIFF);
		}
		if (tail) {
			ret = wres_shm_get_peer_info(resource, page, (wres_shm_peer_info_t *)ptr);
			if (ret) {
				wres_print_err(resource, "failed to get peer info");
				goto out;
			}
			new_args->args.owner = page->owner;
			wres_set_flags(&res, WRES_PEER);
		}
		if (wres_ioctl(&res, (char *)new_args, size, NULL, 0, &src)) {
			wres_print_err(resource, "failed to send request");
			ret = -EFAULT;
		}
out:
		wres_print_lines(resource, new_args->lines, nr_lines, total);
		free(new_args);
	}
	return ret;
}


#ifdef MODE_CHECK_PRIORITY
static inline int wres_shm_need_priority(wresource_t *resource, wres_page_t *page)
{	
	int flags = wres_get_flags(resource);

	if (wres_pg_own(page) && !(flags & WRES_OWN) && (wres_pg_write(page) || (flags & WRES_RDWR)))
		return 1;
	else
		return 0;
}
#endif


int wres_shm_do_check_owner(wres_page_t *page, wres_req_t *req)
{
	int ret = 0;
	wresource_t *resource = &req->resource;
#ifdef MODE_CHECK_PRIORITY
	int need_priority = 0;

	if (wres_shm_need_priority(resource, page)) {
		ret = wres_prio_set_busy(resource);
		if (ret) {
			wres_print_err(resource, "failed to update priority");
			return ret;
		}
		need_priority = 1;
	}
#endif
	if (wres_pg_own(page)) {	
		ret = wres_shm_send_to_holders(page, req);
		if (ret)
			return ret;
		
		ret = wres_shm_send_to_silent_holders(page, req);
		if (ret)
			return ret;
	}
#ifdef MODE_FAST_REPLY
	ret = wres_shm_fast_reply(page, req);
	if (-EAGAIN == ret)
#endif
	if (wres_pg_own(page))
		ret = wres_shm_do_check_holder(page, req);
	if (ret && (ret != -EAGAIN))
		return ret;
	if (wres_pg_own(page)) {
		ret = wres_shm_update_holder(page, req);
		if (ret) {
			wres_print_err(resource, "failed to update holder");
			return ret;
		}
	}
#ifdef MODE_CHECK_PRIORITY
	if (need_priority) {
		ret = wres_prio_set_idle(resource);
		if (ret) {
			wres_print_err(resource, "failed to update priority");
			return ret;
		}
	}
#endif
	return -EAGAIN == ret ? 0 : ret;
}


void wres_shm_clear_updates(wresource_t *resource, wres_page_t *page)
{
	stringio_file_t *filp;
	char path[WRES_PATH_MAX];

	page->count = 0;
	if (!wres_pg_update(page))
		return;

	wres_get_update_path(resource, path);
	filp = stringio_open(path, "r+");
	if (filp) {
		stringio_truncate(filp, 0);
		stringio_close(filp);
	}
	bzero(page->collect, sizeof(page->collect));
	wres_pg_clrupdate(page);
}


int wres_shm_save_updates(wresource_t *resource, wres_page_t *page, wres_line_t *lines, int nr_lines)
{
	int i;
	int ret = 0;
	char path[WRES_PATH_MAX];
	stringio_entry_t *entry;
	
	if (!lines || (nr_lines <= 0)) {
		wres_print_err(resource, "no content");
		return -EINVAL;
	}
	
	wres_get_update_path(resource, path);
	entry = stringio_get_entry(path, 0, STRINGIO_RDWR | STRINGIO_CREAT);
	if (!entry) {
		wres_print_err(resource, "no entry");
		return -ENOENT;
	}
	
	ret = stringio_append(entry, lines, sizeof(wres_line_t) * nr_lines);
	stringio_put_entry(entry);
	if (ret) {
		wres_print_err(resource, "failed");
		return -EFAULT;
	}
	
	for (i = 0; i < nr_lines; i++) {
		int n = lines[i].num;
		page->collect[n] = 1;
	}
	
	for (i = 0; i < WRES_LINE_MAX; i++)
		if (page->collect[i])
			ret++;
	
	wres_pg_mkupdate(page);
	wres_print_shm_save_updates(resource, nr_lines);
	return ret;
}


void wres_shm_load_updates(wresource_t *resource, wres_page_t *page)
{
	int i;
	int total;
	char path[WRES_PATH_MAX];
	stringio_entry_t *entry;
	wres_line_t *lines;

	if (!wres_pg_update(page))
		return;
	
	wres_get_update_path(resource, path);
	entry = stringio_get_entry(path, 0, STRINGIO_RDONLY | STRINGIO_AUTO_INCREMENT);
	if (!entry)
		return;
	
	lines = stringio_get_desc(entry, wres_line_t);
	total = stringio_items_count(entry, sizeof(wres_line_t));
	for (i = 0; i < total; i++) {
		wres_line_t *line = &lines[i];
		int pos = line->num * WRES_LINE_SIZE;
		
		page->digest[line->num] = line->digest;
		memcpy(&page->buf[pos], line->buf, WRES_LINE_SIZE);
	}
	stringio_put_entry(entry);
}


#ifdef MODE_DYNAMIC_OWNER
static inline int wres_shm_change_owner(wres_page_t *page, wres_req_t *req)
{
	int ret;
	int update = 0;
	wres_member_t member;
	int active_member_count = 0;
	wresource_t *resource = &req->resource;
	wres_id_t src = wres_get_id(resource);
	int flags = wres_get_flags(resource);

	ret = wres_member_lookup(resource, &member, &active_member_count);
	if (ret) {
		wres_print_err(resource, "invalid member");
		return ret;
	}
	if (((flags & WRES_RDWR) || (page->nr_holders < WRES_PAGE_NR_HOLDERS))
	    && ((member.count > 0) || (active_member_count < WRES_SHM_NR_AREAS - 1))) {
		if (WRES_SHM_NR_VISITS > 0) {
			int i;
			int total = page->nr_candidates;
			wres_member_t *cand = page->candidates;
			
			for (i = 0; i < total; i++) {
				if (cand[i].id == src) {
					if (cand[i].count >= WRES_SHM_NR_VISITS)
						update = 1;
					break;
				}
			}
		} else
			update = 1;
	}
	if (update) {
		member.count++;
		ret = wres_member_update(resource, &member);
		if (ret) {
			wres_print_err(resource, "failed to update member");
			return ret;
		}
		wres_set_flags(resource, WRES_OWN);
	}
	return 0;
}


static inline int wres_shm_send_to_owner(wres_page_t *page, wres_req_t *req, wres_id_t *to)
{
	int ret;
	wres_shmfault_args_t *args = (wres_shmfault_args_t *)req->buf;
	int cmd = args->cmd;

	wres_prio_clear(req);
	args->cmd = WRES_SHM_NOTIFY_OWNER;
	ret = wres_ioctl(&req->resource, req->buf, req->length, NULL, 0, to);
	args->cmd = cmd;
	return ret;
}


static inline int wres_shm_is_mismatched_owner(wres_page_t *page, wres_req_t *req)
{
	wresource_t *resource = &req->resource;
	wres_shmfault_args_t *args = (wres_shmfault_args_t *)req->buf;
	wres_peers_t *peers = &args->peers;

	return !wres_pg_own(page) && (peers->list[0] == resource->owner);
}
#endif


int wres_shm_check_holder(wres_req_t *req)
{
	int ret = 0;
	wresource_t *resource = &req->resource;
	wres_shmfault_args_t *args = (wres_shmfault_args_t *)req->buf;
	wres_clk_t clk = args->clk;
	wres_page_t *page;
	void *entry;
	
	entry = wres_page_get(resource, &page, WRES_PAGE_RDWR);
	if (!entry) {
		wres_print_err(resource, "no entry");
		return -ENOENT;
	}
	if (!wres_pg_active(page) || ((clk > page->version) && wres_pg_wait(page))) {
		ret = wres_shm_save_req(page, req);
		goto out;
	}
	ret = wres_shm_send_to_silent_holders(page, req);
	if (ret) {
		wres_print_err(resource, "failed to send to silent holders");
		goto out;
	}
	ret = wres_shm_do_check_holder(page, req);
	if (ret) {
		wres_print_err(resource, "failed to check holder");
		goto out;
	}
	ret = wres_shm_update_holder(page, req);
	if (ret) {
		wres_print_err(resource, "failed to update holder");
		goto out;
	}
	wres_print_shm_check_holder(page, req);
out:
	wres_page_put(resource, entry);
	return ret;
}


int wres_shm_notify_holder(wres_req_t *req)
{
	int ret = 0;
	wresource_t *resource = &req->resource;
	wres_shmfault_args_t *args = (wres_shmfault_args_t *)req->buf;
	wres_clk_t clk = args->clk;
	wres_page_t *page;
	void *entry;

	entry = wres_page_get(resource, &page, WRES_PAGE_RDWR);
	if (!entry) {
		wres_print_err(resource, "no entry");
		return -ENOENT;
	}
	if (clk < page->version) {
		wres_print_shm_expired_req(page, req);
		goto out;
	}
	if (!wres_pg_active(page) || ((clk > page->version) && wres_pg_wait(page))) {
		ret = wres_shm_save_req(page, req);
		goto out;
	}
	ret = wres_shm_send_to_silent_holders(page, req);
	if (ret) {
		wres_print_err(resource, "failed to send to silent holders");
		goto out;
	}
#ifdef MODE_FAST_REPLY
	if (!wres_shm_cache_hit(page, req))
		ret = wres_shm_fast_reply(page, req);
	if (-EAGAIN == ret)
#endif		
		ret = wres_shm_do_check_holder(page, req);
	if (ret) {
		wres_print_err(resource, "failed to check holder");
		goto out;
	}
	ret = wres_shm_update_holder(page, req);
	if (ret) {
		wres_print_err(resource, "failed to update holder");
		goto out;
	}
	wres_print_shm_notify_holder(page, req);
out:
	wres_page_put(resource, entry);
	return ret;
}


int wres_shm_wakeup(wresource_t *resource, wres_page_t *page)
{
	int ret;
	wres_shmfault_result_t result;
		
	result.retval = 0;
	memcpy(result.buf, page->buf, PAGE_SIZE);
	ret = wres_event_set(resource, (char *)&result, sizeof(wres_shmfault_result_t));
	if (ret) {
		wres_print_err(resource, "failed to set event");
		return ret;
	}
	do {
		wait(WRES_SHM_CHECK_INTERVAL);
	} while (!sys_shm_present(resource->key, resource->owner, 
	                          wres_get_off(resource), wres_get_flags(resource)));
	wres_print_shm_wakeup(resource);
	return ret;
}


int wres_shm_deliver(wres_page_t *page, wres_req_t *req)
{
	int ret;
	wres_shmfault_args_t *args = (wres_shmfault_args_t *)req->buf;
	wresource_t *resource = &req->resource;
	wres_id_t src = wres_get_id(resource);
	int flags = wres_get_flags(resource);
	wres_clk_t clk = args->clk;

	if (src != resource->owner) {
		wres_print_err(resource, "invalid src, src=%d", src);
		return -EINVAL;
	}
	wres_shm_load_updates(resource, page);
	ret = wres_shm_wakeup(resource, page);
	if (ret) {
		wres_print_err(resource, "failed to wake up");
		return ret;
	}
	if (flags & WRES_RDWR) {
		wres_pg_mkwrite(page);
		page->version = clk + 1;
		wres_page_clear_holder_list(resource, page);
	} else if (flags & WRES_RDONLY) {	
		wres_pg_mkread(page);
		page->version = clk;
	}
	if (page->nr_holders < WRES_PAGE_NR_HOLDERS) {
		ret = wres_page_add_holder(resource, page, src);
		if (ret) {
			wres_print_err(resource, "failed to add holder");
			return ret;
		}
	}
	wres_pg_clrwait(page);
	wres_pg_clrready(page);
	wres_pg_clruptodate(page);
	wres_shm_clear_updates(resource, page);
	page->hid = wres_page_get_hid(page, src);
	page->oclk = page->version;
	page->clk = page->version;
	if (wres_pg_cand(page)) {
		wres_pg_clrcand(page);
		wres_pg_mkown(page);
	}
	wres_pg_mkactive(page);
#ifdef MODE_SYNC_TIME
	ret = wres_prio_sync_time(resource);
	if (ret) {
		wres_print_err(resource, "failed to sync time");
		return ret;
	}
#endif
	return 0;
}


int wres_shm_update_peers(wresource_t *resource, wres_page_t *page, wres_shm_peer_info_t *info)
{
	int ret;
	int flags = wres_get_flags(resource);

	if (flags & WRES_RDONLY) {
		int i;
		
		if ((info->total > WRES_PAGE_NR_HOLDERS) || (info->total < 0)) {
			wres_print_err(resource, "invalid peer info");
			return -EINVAL;
		}		
		for (i = 0; i < info->total; i++) {
			ret = wres_page_add_holder(resource, page, info->list[i].id);
			if (ret) {
				wres_print_err(resource, "failed to add holder");
				return ret;
			}
			ret = wres_save_peer(&info->list[i]);
			if (ret) {
				wres_print_err(resource, "failed to save addr");
				return ret;
			}
		}	
	}
	page->owner = info->owner.id;
	ret = wres_save_peer(&info->owner);
	if (ret) {
		wres_print_err(resource, "failed to save addr");
		return ret;
	}
#ifdef MODE_DYNAMIC_OWNER
	if (flags & WRES_OWN) {
		wres_pg_mkcand(page);
		page->owner = resource->owner;
	}
#endif
	return 0;
}


int wres_shm_notify_proposer(wres_req_t *req)
{
	int ret = 0;
	int total = 0;
	int ready = 0;
	wres_page_t *page;
	wres_shm_notify_proposer_args_t *args = (wres_shm_notify_proposer_args_t *)req->buf;
	wresource_t *resource = &req->resource;
	int flags = wres_get_flags(resource);
	wres_clk_t clk = args->args.clk;
	int nr_lines = args->nr_lines;
	void *entry;
	char *ptr;
	
	entry = wres_page_get(resource, &page, WRES_PAGE_RDWR);
	if (!entry) {
		wres_print_err(resource, "no entry");
		return -ENOENT;
	}
	
	if (!wres_pg_wait(page) || (args->args.index != page->index))
		goto out;
	
	if (clk > page->clk) {
		if (!wres_pg_own(page)) {
			wres_pg_clrready(page);
			wres_pg_clruptodate(page);
			wres_shm_clear_updates(resource, page);
		}
		wres_print_shm_clock_update(resource, page->clk, clk);
		page->clk = clk;
	} else if (clk < page->clk) {
		wres_print_shm_expired_req(page, req);
		goto out;
	}
	
	ptr = ((char *)&args[1]) + sizeof(wres_line_t) * nr_lines; 
	if (flags & WRES_DIFF) {
		wres_shm_unpack_diff(page->diff, ptr);
		ptr += WRES_PAGE_DIFF_SIZE;
		wres_print_page_diff(page->diff);
	}
	
	if (flags & WRES_PEER) {
		wres_shm_peer_info_t *info = (wres_shm_peer_info_t *)ptr;
		
		ret = wres_shm_update_peers(resource, page, info);
		if (ret) {
			wres_print_err(resource, "failed to update peers");
			goto out;
		}
		if (flags & WRES_RDWR)
			page->count -= info->total;
		else if (flags & WRES_RDONLY) {		
			if (wres_pg_uptodate(page))
				ready = 1;
			else
				wres_pg_mkready(page);
		}
	}
	
	if (nr_lines > 0) {
		wres_line_t *lines = args->lines;
		
		total = wres_shm_save_updates(resource, page, lines, nr_lines);
		if (total < 0) {
			wres_print_err(resource, "failed to save updates");
			ret = -EINVAL;
			goto out;
		}
	}
	
	if (flags & WRES_RDWR) {
		page->count++;
		if (0 == page->count)
			ready = 1;
	} else if (flags & WRES_RDONLY) {
		if (total == args->total) {
			if (wres_pg_ready(page))
				ready = 1;
			else
				wres_pg_mkuptodate(page);
		}
	} else {
		wres_print_err(resource, "invalid flag");
		ret = -EINVAL;
		goto out;
	}

	if (ready) {
		wres_print_shm_deliver_event(req);
		ret = wres_shm_deliver(page, req);
		if (ret)
			goto out;
		if (wres_pg_redo(page)) {
			wres_print_shm_notify_proposer(page, req);
			wres_pg_clrredo(page);
			wres_page_put(resource, entry);
			return wres_redo(resource, 0);
		}
	}
out:
	wres_print_shm_notify_proposer(page, req);
	wres_page_put(resource, entry);
	return ret;
}


int wres_shm_request(wres_args_t *args)
{
	int ret;
	wresource_t *resource = &args->resource;
	stringio_entry_t *entry = (stringio_entry_t *)args->entry;
#ifdef MODE_CHECK_PRIORITY
	wres_page_t *page = stringio_get_desc(entry, wres_page_t);
	int need_priority = 0;
	
	if (wres_shm_need_priority(resource, page)) {
		ret = wres_prio_set_busy(resource);
		if (ret) {
			wres_print_err(resource, "failed to update priority");
			goto out;
		}
		need_priority = 1;
	}
#endif
	ret = wres_broadcast_request(args);
	if (ret) {
		wres_print_err(resource, "failed to broadcast");
		goto out;
	}
#ifdef MODE_CHECK_PRIORITY
	if (need_priority) {
		ret = wres_prio_set_idle(resource);
		if (ret) {
			wres_print_err(resource, "failed to update priority");
			goto out;
		}
	}
#endif
out:
	wres_page_put(resource, entry);
	return ret;
}


int wres_shm_handle_zeropage(wresource_t *resource, wres_page_t *page)
{
	int ret;
	int flags = wres_get_flags(resource);
	wres_id_t src = wres_get_id(resource);

	bzero(page, sizeof(wres_page_t));
	page->owner = resource->owner;
	wres_pg_mkown(page);
	if (flags & WRES_RDWR)
		page->oclk = 1;
	ret = wres_page_add_holder(resource, page, src);
	if (ret) {
		wres_print_err(resource, "failed to add holder");
		return ret;
	}
	
	if (resource->owner == src) {
		if (flags & WRES_RDWR) {
			page->clk = 1;
			page->version = 1;
			wres_pg_mkwrite(page);
		} else
			wres_pg_mkread(page);
		page->hid = wres_page_get_hid(page, src);
		wres_pg_mkactive(page);
	} else {
		wres_shm_peer_info_t *info;
		wresource_t res = *resource;
		wres_shm_notify_proposer_args_t *args;
		size_t size = sizeof(wres_shm_notify_proposer_args_t) + sizeof(wres_shm_peer_info_t);

		args = (wres_shm_notify_proposer_args_t *)calloc(1, size);
		if (!args) {
			wres_print_err(resource, "no memory");
			return -ENOMEM;
		}
		args->args.index = 1;
		args->args.owner = page->owner;
		args->args.cmd = WRES_SHM_NOTIFY_PROPOSER;
		info = (wres_shm_peer_info_t *)&args[1];	
		if (wres_get_peer(page->owner, &info->owner)) {
			wres_print_err(resource, "failed to get addr");
			ret = -EFAULT;
			goto out;
		}
		if (flags & WRES_RDWR)
			info->total = 1;
		wres_set_flags(&res, WRES_PEER);
		if (wres_ioctl(&res, (char *)args, size, NULL, 0, &src)) {
			wres_print_err(resource, "failed to send request");
			ret = -EFAULT;
		}
out:
		free(args);
	}
	wres_print_shm_handle_zeropage(resource, page);
	return ret;
}


int wres_shm_check_owner(wres_req_t *req, int flags)
{
	void *entry;
	int ret = 0;
	int head = 0;
	wres_page_t *page;
	struct shmid_ds shmid_ds;
	wresource_t *resource = &req->resource;

	entry = wres_page_get(resource, &page, WRES_PAGE_RDWR);
	if (!wres_shm_check(resource, &shmid_ds)) {
		if (!entry) {
			entry = wres_page_get(resource, &page, WRES_PAGE_RDWR | WRES_PAGE_CREAT);
			if (!entry) {
				wres_print_err(resource, "no entry");
				return -ENOENT;
			}
			ret = wres_shm_handle_zeropage(resource, page);
			goto out;
		}
		head = 1;
	} else if (!entry)
		return 0;
#ifdef MODE_FAST_REPLY	
	if (wres_shm_cache_hit(page, req))
		goto out;
#endif
#ifdef MODE_CHECK_PRIORITY
	if (wres_shm_need_priority(resource, page) && !(flags & (WRES_SELECT | WRES_REDO))) {
		ret = wres_prio_check(req);
		if (ret)
			goto out;
	}
#endif
	if (wres_pg_own(page)) {
		if (wres_pg_wait(page)) {
			if (!(flags & WRES_REDO)) {
				ret = wres_shm_save_req(page, req);
				goto out;
			} else {
				wres_pg_mkredo(page);
				wres_page_put(resource, entry);
				return -EAGAIN;
			}
		}
#ifdef MODE_DYNAMIC_OWNER
		if (head) {
			ret = wres_shm_change_owner(page, req);
			if (ret) {
				wres_print_err(resource, "failed to change owner");
				goto out;
			}
		}
#endif
	}
#ifdef MODE_DYNAMIC_OWNER
	if (wres_shm_is_mismatched_owner(page, req)) {
		wres_id_t *to = head ? &page->owner : NULL;
		ret = wres_shm_send_to_owner(page, req, to);
		if (ret) {
			wres_print_err(resource, "failed to send to owner");
			goto out;
		}
	}
#endif
	ret = wres_shm_do_check_owner(page, req);
	wres_print_shm_check_owner(page, req);
out:
	wres_page_put(resource, entry);
	return -EAGAIN == ret ? 0 : ret;
}


int wres_shm_notify_owner(wres_req_t *req, int flags)
{
	int ret = 0;
	void *entry;
	wres_page_t *page;
	wresource_t *resource = &req->resource;
	
	entry = wres_page_get(resource, &page, WRES_PAGE_RDWR | WRES_PAGE_CREAT);
	if (!entry) {
		wres_print_err(resource, "no entry");
		return -ENOENT;
	}
#ifdef MODE_FAST_REPLY	
	if (wres_shm_cache_hit(page, req))
		goto out;
#endif
#ifdef MODE_CHECK_PRIORITY
	if (wres_shm_need_priority(resource, page) && !(flags & (WRES_SELECT | WRES_REDO))) {
		ret = wres_prio_check(req);
		if (ret)
			goto out;
	}
#endif
	if (wres_pg_own(page)) {
		if (wres_pg_wait(page)) {
			if (!(flags & WRES_REDO)) {
				ret = wres_shm_save_req(page, req);
				goto out;
			} else {
				wres_pg_mkredo(page);
				wres_page_put(resource, entry);
				return -EAGAIN;
			}
		}
	}
#ifdef MODE_DYNAMIC_OWNER
	else {
		wres_id_t *to = wres_is_owner(resource) ? &page->owner : NULL;
		ret = wres_shm_send_to_owner(page, req, to);
		goto out;
	}
#endif
	ret = wres_shm_do_check_owner(page, req);
	wres_print_shm_notify_owner(page, req);
out:
	wres_page_put(resource, entry);
	return -EAGAIN == ret ? 0 : ret;
}


static inline void wres_shm_handle_err(wresource_t *resource, int err)
{
	wresource_t res = *resource;
	wres_shmfault_result_t result;
	wres_id_t src = wres_get_id(resource);
		
	result.retval = err;
	wres_set_op(&res, WRES_OP_REPLY);
	wres_ioctl(&res, (char *)&result, sizeof(wres_shmfault_result_t), NULL, 0, &src);
}


int wres_shm_propose(wres_req_t *req)
{
	int ret;
	wres_args_t args;
	wresource_t *resource = &req->resource;
	
	bzero(&args, sizeof(wres_args_t));
	args.resource = *resource;
	ret = wres_shm_get_args(resource, &args);
	if (ret) {
		wres_print_err(resource, "failed to get args");
		return ret;
	}
	ret = wres_shm_request(&args);
	wres_shm_put_args(&args);
	return ret;
}


#ifdef MODE_CHECK_TTL
static inline int wres_shm_check_ttl(wres_req_t *req)
{
	wresource_t *resource = &req->resource;
	wres_shmfault_args_t *args = (wres_shmfault_args_t *)req->buf;

	args->ttl += 1;
	if (args->ttl > WRES_SHM_TTL_MAX) {
		wres_print_err(resource, "op=%s, TTL is out of range", wres_shmfault2str(args->cmd));
		return -EINVAL;
	}
	return 0;
}
#endif


wres_reply_t *wres_shm_fault(wres_req_t *req, int flags)
{
	int ret = 0;
	wres_shmfault_args_t *args = (wres_shmfault_args_t *)req->buf;
	wresource_t *resource = &req->resource;

#ifdef MODE_CHECK_TTL
	ret = wres_shm_check_ttl(req);
	if (ret) {
		wres_shm_handle_err(resource, ret);
		return NULL;
	}
#endif
	switch (args->cmd) {
	case WRES_SHM_PROPOSE:
		ret = wres_shm_propose(req);
		break;
	case WRES_SHM_CHK_OWNER:
		ret = wres_shm_check_owner(req, flags);
		break;
	case WRES_SHM_CHK_HOLDER:
		ret = wres_shm_check_holder(req);
		break;
	case WRES_SHM_NOTIFY_OWNER:
		ret = wres_shm_notify_owner(req, flags);
		break;
	case WRES_SHM_NOTIFY_HOLDER:
		ret = wres_shm_notify_holder(req);
		break;
	case WRES_SHM_NOTIFY_PROPOSER:
		ret = wres_shm_notify_proposer(req);
		break;
	default:
		break;
	}
	if (ret) {
		if ((flags & WRES_REDO) && (-EAGAIN == ret))
			return wres_reply_err(ret);
		else {
			wres_shm_handle_err(resource, ret);
			wres_print_err(resource, "op=%s, failed", wres_shmfault2str(args->cmd));
		}
	}
	return NULL;
}


int wres_shm_rmid(wres_req_t *req)
{
	int active;
	int ret = 0;
	wresource_t *resource = &req->resource;
	
	if (wres_is_owner(resource)) {
		ret = wres_member_notify(req);
		if (ret) {
			wres_print_err(resource, "failed to notify members");
			return ret;
		}
	}
	active = wres_member_is_active(resource);
	wres_free(resource);
	if (!active)
		ret = wres_tskput(resource);
	return ret;
}


int wres_shm_destroy(wresource_t *resource)
{
	int ret;
	char buf[sizeof(wres_req_t) + sizeof(wres_shmctl_args_t)];
	wres_req_t *req = (wres_req_t *)buf;
	wres_shmctl_args_t *args = (wres_shmctl_args_t *)req->buf;

	args->cmd = IPC_RMID;
	req->resource = *resource;
	req->length = sizeof(wres_shmctl_args_t);
	wres_set_op(&req->resource, WRES_OP_SHMCTL);
	wres_set_id(&req->resource, resource->owner);
	ret = wres_get_peer(resource->owner, &req->src);
	if (ret)
		return ret;
	return wres_shm_rmid(req);
}


wres_reply_t *wres_shm_shmctl(wres_req_t *req, int flags)
{
	int ret = 0;
	int outlen = sizeof(wres_shmctl_result_t);
	wres_shmctl_args_t *args = (wres_shmctl_args_t *)req->buf;
	wresource_t *resource = &req->resource;
	wres_shmctl_result_t *result = NULL;
	wres_reply_t *reply = NULL;
	int cmd = args->cmd;
	
	switch (cmd) {
	case IPC_INFO:
		outlen += sizeof(struct shminfo);
		break;
	case SHM_INFO:
		outlen += sizeof(struct shm_info);
		break;
	case SHM_STAT:
	case IPC_STAT:
		outlen += sizeof(struct shmid_ds);
		break;
	case IPC_RMID:
		outlen = 0;
		break;
	default:
		break;
	}
	if (outlen) {
		reply = wres_reply_alloc(outlen);
		if (!reply)
			return wres_reply_err(-ENOMEM);
		result = wres_result_check(reply, wres_shmctl_result_t);
	}
	switch (cmd) {
	case IPC_INFO:
		wres_shm_info((struct shminfo *)&result[1]);
		break;
	case SHM_INFO:
	{	
		//Fixme: global information of shm is not available
		struct shmid_ds shmid;
		struct shm_info *shm_info;
		
		ret = wres_shm_check(resource, &shmid);
		if (ret)
			goto out;
		shm_info = (struct shm_info *)&result[1];
		bzero(shm_info, sizeof(struct shm_info));
		shm_info->used_ids = 1;
		shm_info->shm_rss = (shmid.shm_segsz + PAGE_SIZE - 1) / PAGE_SIZE;
		shm_info->shm_tot = shm_info->shm_rss;
		break;
	}
	case SHM_STAT:
	case IPC_STAT:
	{
		struct shmid_ds shmid_ds;
		
		ret = wres_shm_check(resource, &shmid_ds);
		if (ret)
			goto out;
		memcpy((struct shmid_ds *)&result[1], &shmid_ds, sizeof(struct shmid_ds));
		if (SHM_STAT == cmd)
			ret = resource->key;
		break;
	}
	case SHM_LOCK:
	case SHM_UNLOCK:
		break;
	case IPC_RMID:
		ret = wres_shm_rmid(req);
		if (ret)
			return wres_reply_err(-ERMID);
		break;
	case IPC_SET:
	{
		struct shmid_ds shmid;
		struct shmid_ds *buf = (struct shmid_ds *)&args[1];
		
		ret = wres_shm_check(resource, &shmid);
		if (ret)
			goto out;
		memcpy(&shmid.shm_perm, &buf->shm_perm, sizeof(struct ipc_perm));
		shmid.shm_ctime = time(0);
		ret = wres_shm_update(resource, &shmid);
		break;
	}
	default:
		ret = -EINVAL;
		break;
	}
out:
	if (result)
		result->retval = ret;
	return reply;
}


int wres_shm_get_peers(wresource_t *resource, wres_page_t *page, wres_peers_t *peers)
{
	int i;
	int cnt = 1;
	
	if (!wres_pg_own(page)) {
		wres_id_t page_owner = page->owner;
		
		if (!page_owner) {
			wres_desc_t desc;
			
			if (wres_lookup(resource, &desc)) {
				wres_print_err(resource, "failed to get addr");
				return -EINVAL;
			}
			page->owner = desc.id;
			page_owner = desc.id;
		}
#ifdef MODE_FAST_REPLY
		if (wres_get_flags(resource) & WRES_RDONLY) {
			int total = min(page->nr_candidates, WRES_SHM_NR_POSSIBLE_HOLERS);
			
			for (i = 0; i < total; i++) {
				wres_id_t id = page->candidates[i].id;
				if ((id != resource->owner) && (id != page_owner)) {
					peers->list[cnt] = id;
					cnt++;
				}
			}
		}
#endif
		peers->list[0] = page_owner;
	} else {
		for (i = 0, cnt = 0; i < page->nr_holders; i++) {
			if (page->holders[i] != resource->owner) {
				peers->list[cnt] = page->holders[i];
				cnt++;
			}
		}
	}
	peers->total = cnt;
	return 0;
}


int wres_shm_passthrough(wresource_t *resource, wres_page_t *page)
{
	int flags = wres_get_flags(resource);
	wres_id_t src = wres_get_id(resource);
	
	if (!wres_pg_active(page))
		return 0;

	if (((1 == page->nr_holders) && (page->holders[0] == src)) || (flags & WRES_RDONLY)
	    || ((flags & WRES_RDWR) && wres_pg_write(page))) {
		if (flags & WRES_RDWR)
			wres_pg_mkwrite(page);
		wres_print_shm_passthrough(resource);
		return -EOK;
	}
	return 0;
}


void wres_shm_mkwait(wresource_t *resource, wres_page_t *page)
{	
	if (wres_pg_own(page)) {
		if (wres_get_flags(resource) & WRES_RDWR) {	
			int overlap;

			overlap = wres_page_search_holder_list(resource, page, wres_get_id(resource));
			page->count = overlap - wres_page_calc_holders(page);
		} else
			wres_pg_mkready(page);
	}
	wres_pg_mkwait(page);
}


#ifdef MODE_CHECK_PRIORITY
wres_req_t *wres_shm_get_req(wresource_t *resource)
{
	wres_req_t *req;
	wres_shmfault_args_t *args;
	size_t size = sizeof(wres_req_t) + sizeof(wres_shmfault_args_t);
	
	req = (wres_req_t *)calloc(1, size);
	if (!req) {
		wres_print_err(resource, "no memory");
		return NULL;
	}
	req->resource = *resource;
	req->length = sizeof(wres_shmfault_args_t);
	args = (wres_shmfault_args_t *)req->buf;
	args->cmd = WRES_SHM_PROPOSE;
#ifdef MODE_EXPLICIT_PRIORITY
	if (wres_prio_check_live(resource, &args->prio, &args->live)) {
		wres_print_err(resource, "failed to get live time");
		free(req);
		return NULL;
	}
#endif
	return req;
}

void wres_shm_put_req(wres_req_t *req)
{
	free(req);
}
#endif


int wres_shm_check_args(wresource_t *resource, wres_args_t *args)
{
	int ret = 0;
	void *entry;
	wres_page_t *page;

	if (wres_get_op(resource) != WRES_OP_SHMFAULT)
		return 0;
	
	entry = wres_page_get(resource, &page, WRES_PAGE_RDWR | WRES_PAGE_CREAT);
	if (!entry) {
		wres_print_err(resource, "no entry");
		return -ENOENT;
	}
	args->index = wres_get_off(resource);
	args->in = NULL;
	if (wres_is_owner(resource) && (0 == page->owner)) {
		ret = wres_shm_handle_zeropage(resource, page);
		if (!ret)
			ret = -EOK;
		goto out;
	}
	ret = wres_shm_passthrough(resource, page);
	if (ret)
		goto out;
#ifdef MODE_CHECK_PRIORITY
	if (wres_shm_need_priority(resource, page)) {
		wres_req_t *req = wres_shm_get_req(resource);
		
		if (!req) {
			wres_print_err(resource, "failed to get request");
			ret = -EFAULT;
			goto out;
		}
		ret = wres_prio_check(req);
		wres_shm_put_req(req);
	}
#endif
	wres_print_shm_check_args(resource);
out:
	if (ret) {
		if (-EOK == ret) {
			wres_shmfault_result_t *result = (wres_shmfault_result_t *)args->out;
			
			result->retval = 0;
		}
		wres_page_put(resource, entry);
		return ret;
	}
	args->entry = entry;
	return 0;
}


int wres_shm_get_args(wresource_t *resource, wres_args_t *args)
{
	int ret = 0;
	size_t size;
	wres_page_t *page;
	wres_shmfault_args_t *shmfault_args = NULL;

	if (wres_get_op(resource) != WRES_OP_SHMFAULT)
		return 0;

	if (!args->entry) {
		args->entry = wres_page_get(resource, &page, WRES_PAGE_RDWR | WRES_PAGE_CREAT);
		if (!args->entry) {
			wres_print_err(resource, "no entry");
			return -ENOENT;
		}
	} else
		page = stringio_get_desc((stringio_entry_t *)args->entry, wres_page_t);
	
	size = sizeof(wres_shmfault_args_t) + WRES_PAGE_NR_HOLDERS * sizeof(wres_id_t);
	shmfault_args = (wres_shmfault_args_t *)calloc(1, size);
	if (!shmfault_args) {
		wres_print_err(resource, "no memory");
		ret = -ENOMEM;
		goto out;
	}
	shmfault_args->version = page->version;
	shmfault_args->index = wres_page_update_index(page);
	if (wres_pg_own(page)) {
		shmfault_args->cmd = WRES_SHM_CHK_HOLDER;
		shmfault_args->clk = page->oclk;
	} else {
		shmfault_args->cmd = WRES_SHM_CHK_OWNER;
		shmfault_args->clk = page->clk;
	}
	ret = wres_shm_get_peers(resource, page, &shmfault_args->peers);
	if (ret) {
		wres_print_err(resource, "failed to get peers");
		goto out;
	}
	shmfault_args->owner = page->owner;
#ifdef MODE_EXPLICIT_PRIORITY
	if (!wres_pg_own(page)) {
		ret = wres_prio_check_live(resource, &shmfault_args->prio, &shmfault_args->live);
		if (ret) {
			wres_print_err(resource, "failed to get priority");
			goto out;
		}
	}
#endif
	wres_shm_mkwait(resource, page);
	args->inlen = size;
	args->in = (char *)shmfault_args;
	if (shmfault_args->peers.total > 0)
		args->peers = &shmfault_args->peers;
	wres_print_shm_get_args(args);
out:
	if (ret) {
		if (shmfault_args)
			free(shmfault_args);
		wres_page_put(resource, args->entry);
	}
	return ret;
}

	
void wres_shm_put_args(wres_args_t *args)
{
	wresource_t *resource = &args->resource;

	if (WRES_OP_SHMFAULT == wres_get_op(resource)) {
		if (args->in) {
			free(args->in);
			args->in = NULL;
		}
	}
}
