/*      prio.c
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

#include "prio.h"

int wres_prio_stat = 0;
int wres_prio_table[WRES_MEMBER_MAX][WRES_PRIO_NR_INTERVALS];
wres_prio_lock_group_t wres_prio_lock_group[WRES_PRIO_LOCK_GROUP_SIZE];

#ifdef MODE_EXPLICIT_PRIORITY
void wres_prio_gen()
{
	int i, j, k;
	
	srand(1);
	for (j = 0; j < WRES_PRIO_NR_INTERVALS; j++) {
		for (i = 0; i < WRES_MEMBER_MAX; i++) {
			int tmp;
			for (;;) {
				tmp = rand();
				for (k = 0; k < i; k++) {
					if (wres_prio_table[k][j] == tmp)
						break;
				}
				if (k == i)
					break;
			} 
			wres_prio_table[i][j] = tmp;
		}
	}
}
#else
void wres_prio_gen()
{
	int i, j, k;
	int tmp[WRES_MEMBER_MAX];
	
	srand(1);
	for (j = 0; j < WRES_PRIO_NR_INTERVALS; j++) {
		for (i = 0; i < WRES_MEMBER_MAX; i++)
			tmp[i] = i + 1;
		for (i = 0; i < WRES_MEMBER_MAX; i++) {
			int n = rand() % (WRES_MEMBER_MAX - i);
			wres_prio_table[i][j] = tmp[n];
			for (k = n; k < WRES_MEMBER_MAX - i - 1; k++)
				tmp[k] = tmp[k + 1];
		}
	}
}
#endif

void wres_prio_init()
{	
	int i;
	
	if (wres_prio_stat & WRES_STAT_INIT)
		return;

	wres_prio_gen();
	for (i = 0; i < WRES_PRIO_LOCK_GROUP_SIZE; i++) {
		pthread_mutex_init(&wres_prio_lock_group[i].mutex, NULL);
		wres_prio_lock_group[i].head = rbtree_create();
	}
	wres_prio_stat |= WRES_STAT_INIT;
}


int wres_prio_create(wresource_t *resource, wres_time_t off)
{
	wres_prio_t *prio;
	char path[WRES_PATH_MAX];

	wres_get_priority_path(resource, path);
	stringio_entry_t *entry = stringio_get_entry(path, sizeof(wres_prio_t), STRINGIO_RDWR | STRINGIO_CREAT);
	if (!entry) {
		wres_print_err(resource, "no entry");
		return -ENOENT;
	}
	prio = stringio_get_desc(entry, wres_prio_t);
	bzero(prio, sizeof(wres_prio_t));
	prio->t_off = off;
	prio->t_sync = get_time();
	stringio_put_entry(entry);
	wres_print_prio_create(resource, off);
	return 0;
}


static inline unsigned long wres_prio_lock_hash(wres_prio_lock_desc_t *desc)
{
	unsigned long *ent = desc->entry;
	return (ent[0] ^ ent[1] ^ ent[2]) % WRES_PRIO_LOCK_GROUP_SIZE;
}


static inline int wres_prio_lock_compare(char *l1, char *l2)
{
	wres_prio_lock_desc_t *desc1 = (wres_prio_lock_desc_t *)l1;
	wres_prio_lock_desc_t *desc2 = (wres_prio_lock_desc_t *)l2;
	return memcmp(desc1->entry, desc2->entry, sizeof(((wres_prio_lock_desc_t *)0)->entry));
}


static inline void wres_prio_lock_get_desc(wresource_t *resource, wres_prio_lock_desc_t *desc)
{
	desc->entry[0] = resource->key;
	desc->entry[1] = resource->cls;
	desc->entry[2] = resource->owner;
}


static inline wres_prio_lock_t *wres_prio_lock_alloc(wres_prio_lock_desc_t *desc)
{
	wres_prio_lock_t *lock = (wres_prio_lock_t *)malloc(sizeof(wres_prio_lock_t));
	
	if (!lock)
		return NULL;
	
	lock->count = 0;
	memcpy(&lock->desc, desc, sizeof(wres_prio_lock_desc_t));
	pthread_mutex_init(&lock->mutex, NULL);
	pthread_cond_init(&lock->cond, NULL);
	return lock;
}


static inline wres_prio_lock_t *wres_prio_lock_lookup(wres_prio_lock_group_t *group, wres_prio_lock_desc_t *desc)
{
	return rbtree_lookup(group->head, (void *)desc, wres_prio_lock_compare);
}


static inline void wres_prio_lock_insert(wres_prio_lock_group_t *group, wres_prio_lock_t *lock)
{
	rbtree_insert(group->head, (void *)&lock->desc, (void *)lock, wres_prio_lock_compare);
}


static inline wres_prio_lock_t *wres_prio_lock_get(wresource_t *resource) 
{
	wres_prio_lock_desc_t desc;
	wres_prio_lock_group_t *grp;
	wres_prio_lock_t *lock = NULL;
	
	wres_prio_lock_get_desc(resource, &desc);
	grp = &wres_prio_lock_group[wres_prio_lock_hash(&desc)];
	pthread_mutex_lock(&grp->mutex);
	lock = wres_prio_lock_lookup(grp, &desc);
	if (!lock) {
		lock = wres_prio_lock_alloc(&desc);
		if (!lock) {
			wres_print_err(resource, "no memory");
			goto out;
		}
		wres_prio_lock_insert(grp, lock);
	}
	pthread_mutex_lock(&lock->mutex);
	lock->count++;
out:
	pthread_mutex_unlock(&grp->mutex);
	return lock;
}


static void wres_prio_lock_free(wres_prio_lock_group_t *group, wres_prio_lock_t *lock)
{
	rbtree_delete(group->head, (void *)&lock->desc, wres_prio_lock_compare);
	pthread_mutex_destroy(&lock->mutex); 
	pthread_cond_destroy(&lock->cond);
	free(lock);
}


static int wres_prio_lock(wresource_t *resource)
{
	wres_prio_lock_t *lock;
	
	if (!(wres_prio_stat & WRES_STAT_INIT))
		return -EINVAL;
	
	lock = wres_prio_lock_get(resource);
	if (!lock) {
		wres_print_err(resource, "no entry");
		return -ENOENT;
	}
	
	if (lock->count > 1)
		pthread_cond_wait(&lock->cond, &lock->mutex);
	pthread_mutex_unlock(&lock->mutex);
	wres_print_prio_lock(resource);
	return 0; 
}


static void wres_prio_unlock(wresource_t *resource)
{
	int empty = 0;
	wres_prio_lock_t *lock;
	wres_prio_lock_desc_t desc;
	wres_prio_lock_group_t *grp;
	
	if (!(wres_prio_stat & WRES_STAT_INIT)) 
		return;
	
	wres_prio_lock_get_desc(resource, &desc);
	grp = &wres_prio_lock_group[wres_prio_lock_hash(&desc)];
	pthread_mutex_lock(&grp->mutex);
	lock = wres_prio_lock_lookup(grp, &desc);
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
		wres_prio_lock_free(grp, lock);
	pthread_mutex_unlock(&grp->mutex);
	wres_print_prio_unlock(resource);
}


static int wres_prio_get_time(wresource_t *resource, wres_time_t *time)
{
	wres_prio_t *prio;
	stringio_entry_t *entry;
	char path[WRES_PATH_MAX];
	
	wres_get_priority_path(resource, path);
	entry = stringio_get_entry(path, sizeof(wres_prio_t), STRINGIO_RDONLY);
	if (!entry) {
		wres_print_err(resource, "no entry");
		return -ENOENT;
	}
	prio = stringio_get_desc(entry, wres_prio_t);
	*time = wres_prio_time(prio, get_time());
	stringio_put_entry(entry);
	return 0;
}


static int wres_prio_get_waittime(wresource_t *resource, wres_time_t *waittime)
{
	wres_time_t time;
	
	if (wres_prio_get_time(resource, &time)) {
		wres_print_err(resource, "failed to get time");
		return -EINVAL;
	}
	*waittime = WRES_PRIO_PERIOD - time % WRES_PRIO_PERIOD;
	return 0;
}


static int wres_prio_convert_from_time(wresource_t *resource, wres_id_t id, wres_time_t time)
{
	int pos = wres_member_get_pos(resource, id);
	
	if (pos < 0) {
		wres_print_err(resource, "invalid id");
		return -EINVAL;
	}
	return wres_prio_table[pos][wres_prio_interval(time)];
}


#ifdef MODE_EXPLICIT_PRIORITY
static int wres_prio_is_expired(wres_prio_t *prio)
{
	return prio->t_update + prio->t_live < get_time();
}
#else
static int wres_prio_is_expired(wres_prio_t *prio)
{
	wres_time_t curr = wres_prio_time(prio, get_time());
	wres_time_t prev = wres_prio_time(prio, prio->t_update);
	
	return curr - prev > WRES_PRIO_PERIOD;
}


static int wres_prio_compare(wresource_t *resource, wres_id_t dest, wres_id_t src)
{
	int i, j;
	wres_time_t time;
	
	if (!(wres_prio_stat & WRES_STAT_INIT)) 
		return -EINVAL;

	if (wres_prio_get_time(resource, &time)) {
		wres_print_err(resource, "failed to get time");
		return -EINVAL;
	}

	i = wres_prio_convert_from_time(resource, dest, time);
	if (i < 0) {
		wres_print_err(resource, "failed to get priority");
		return -EINVAL;
	}
	j = wres_prio_convert_from_time(resource, src, time);
	if (j < 0) {
		wres_print_err(resource, "failed to get priority");
		return -EINVAL;
	}
	if (i > j)
		return 1;
	else if (i == j)
		return 0;
	else
		return -1;
}
#endif


int wres_prio_get(wresource_t *resource, wres_prio_t *prio)
{
	wres_prio_t *ptr;
	stringio_entry_t *entry;
	char path[WRES_PATH_MAX];
	
	if (!(wres_prio_stat & WRES_STAT_INIT)) 
		return -EINVAL;
	
	wres_get_priority_path(resource, path);
	entry = stringio_get_entry(path, sizeof(wres_prio_t), STRINGIO_RDONLY);
	if (!entry) {
		wres_print_err(resource, "no entry");
		return -ENOENT;		
	}
	ptr = stringio_get_desc(entry, wres_prio_t);
	memcpy(prio, ptr, sizeof(wres_prio_t));
	stringio_put_entry(entry);
	return 0;
}


int wres_prio_set(wresource_t *resource, wres_prio_t *prio)
{
	wres_prio_t *ptr;
	stringio_entry_t *entry;
	char path[WRES_PATH_MAX];
	
	if (!(wres_prio_stat & WRES_STAT_INIT)) 
		return -EINVAL;
	
	wres_get_priority_path(resource, path);
	entry = stringio_get_entry(path, sizeof(wres_prio_t), STRINGIO_RDWR);
	if (!entry) {
		wres_print_err(resource, "no entry");
		return -ENOENT;
	}
	ptr = stringio_get_desc(entry, wres_prio_t);
	memcpy(ptr, prio, sizeof(wres_prio_t));
	stringio_put_entry(entry);	
	return 0;
}


#ifdef MODE_EXPLICIT_PRIORITY
int wres_prio_check_live(wresource_t *resource, int *prio, wres_time_t *live)
{
	int ret;
	wres_time_t curr;
	wres_id_t id = wres_get_id(resource);

	if (!(wres_prio_stat & WRES_STAT_INIT)) 
		return -EINVAL;
	
	wres_prio_lock(resource);
	ret = wres_prio_get_time(resource, &curr);
	if (ret < 0) {
		wres_print_err(resource, "failed to get time");
		goto unlock;
	}
	ret = wres_prio_convert_from_time(resource, id, curr);
	if (ret < 0) {
		wres_print_err(resource, "failed to get priority");
		goto unlock;
	}
	*live = WRES_PRIO_PERIOD - curr % WRES_PRIO_PERIOD;
	*prio = ret;
	ret = 0;
unlock:
	wres_prio_unlock(resource);
	return ret;
}


int wres_prio_set_busy(wresource_t *resource)
{
	return 0;
}


int wres_prio_set_idle(wresource_t *resource)
{
	return 0;
}


static int wres_prio_update(wresource_t *resource, int val, wres_time_t live)
{
	wres_prio_t *prio;
	stringio_entry_t *entry;
	char path[WRES_PATH_MAX];

	wres_get_priority_path(resource, path);
	entry = stringio_get_entry(path, sizeof(wres_prio_t), STRINGIO_RDWR);
	if (!entry) {
		wres_print_err(resource, "no entry");
		return -ENOENT;
	}
	prio = stringio_get_desc(entry, wres_prio_t);
	prio->val = val;
	prio->t_live = live;
	prio->t_update = get_time();
	stringio_put_entry(entry);
	return 0;
}
#else
int wres_prio_set_busy(wresource_t *resource)
{
	int ret = 0;
	wres_prio_t *prio;
	stringio_entry_t *entry;
	char path[WRES_PATH_MAX];
	
	if (!(wres_prio_stat & WRES_STAT_INIT)) 
		return -EINVAL;

#ifdef WRES_PRIO_UPDATE_CONTROL
	if (wres_get_flags(resource) & WRES_REDO)
		return 0;
#endif
	wres_get_priority_path(resource, path);
	wres_prio_lock(resource);
	entry = stringio_get_entry(path, sizeof(wres_prio_t), STRINGIO_RDWR);
	if (!entry) {
		wres_print_err(resource, "no entry");
		ret = -ENOENT;
	} else {
		prio = stringio_get_desc(entry, wres_prio_t);
		prio->id = wres_get_id(resource);
		prio->t_update = get_time();
		stringio_put_entry(entry);
	}
	wres_prio_unlock(resource);
	return ret;
}


int wres_prio_set_idle(wresource_t *resource)
{
	int ret = 0;
	wres_prio_t *prio;
	stringio_entry_t *entry;
	char path[WRES_PATH_MAX];
	
	if (!(wres_prio_stat & WRES_STAT_INIT)) 
		return -EINVAL;

#ifdef WRES_PRIO_UPDATE_CONTROL
	if (wres_get_flags(resource) & WRES_REDO)
		return 0;
#endif
	wres_get_priority_path(resource, path);
	wres_prio_lock(resource);
	entry = stringio_get_entry(path, sizeof(wres_prio_t), STRINGIO_RDWR);
	if (!entry) {
		wres_print_err(resource, "no entry");
		ret = -ENOENT;
	} else {
		wres_id_t id = wres_get_id(resource);
		prio = stringio_get_desc(entry, wres_prio_t);
		if (!wres_prio_is_expired(prio) && (id == prio->id)) {
			prio->id = id;
			prio->t_update = get_time();
		}
		stringio_put_entry(entry);
	}
	wres_prio_unlock(resource);
	return ret;
}
#endif


void *wres_prio_select(void *args)
{
	wres_index_t index;
	char path[WRES_PATH_MAX];
	wresource_t *resource = (wresource_t *)args;
	
	wres_get_resource_path(resource, path);
	wres_prio_lock(resource);
	
	while (!wres_record_first(path, &index)) {
		int ret;
		wres_id_t user = 0;
		wres_index_t pos = index;
		wres_record_t record;
		wres_time_t time;
		wres_prio_t prio;
		wres_id_t id;
#if !defined(MODE_EXPLICIT_PRIORITY) && !defined(WRES_PRIO_NOWAIT)
		int curr = -1;
		int val;
#endif
retry:
		ret = wres_prio_get(resource, &prio);
		if (ret) {
			wres_print_err(resource, "failed to get priority");
			goto unlock;
		}

		if (!wres_prio_is_expired(&prio)) {
			ret = wres_prio_get_waittime(resource, &time);
			if (ret) {
				wres_print_err(resource, "failed to get wait time");
				goto unlock;
			}
			wres_prio_unlock(resource);
			wait(time);
			wres_prio_lock(resource);
			goto retry;
		}
#if !defined(MODE_EXPLICIT_PRIORITY) && !defined(WRES_PRIO_NOWAIT)
		ret = wres_prio_get_time(resource, &time);
		if (ret) {
			wres_print_err(resource, "failed to get time");
			goto unlock;
		}
		
		do {
			ret = wres_record_get(path, index, &record);
			if (ret) {
				wres_print_err(resource, "failed to get record");
				goto unlock;
			}
			id = wres_get_id(&record.req->resource);
			wres_record_put(&record);
			val = wres_prio_convert_from_time(resource, id, time);
			if (val < 0) {
				wres_print_err(resource, "failed to get priority");
				goto unlock;
			}
			if (val > curr) {
				user = id;
				curr = val;
				pos = index;
			}
		} while (!wres_record_next(path, &index));

		if (curr < 0) {
			wres_print_err(resource, "no record");
			goto unlock;
		}
#endif
		do {
			ret = wres_record_get(path, pos, &record);
			if (ret) {
				wres_print_err(resource, "failed to get record");
				goto unlock;
			}
			id = wres_get_id(&record.req->resource);
			if ((id == user) || !user) {
				wres_prio_unlock(resource);
				wres_proc(record.req, WRES_SELECT);
				wres_prio_lock(resource);
			}
			wres_record_put(&record);
			if ((id == user) || !user) {
				wres_record_remove(path, pos);
#if !defined(MODE_EXPLICIT_PRIORITY) && !defined(WRES_PRIO_NOWAIT)
				ret = wres_prio_get(resource, &prio);
				if (ret) {
					wres_print_err(resource, "failed to get priority");
					goto unlock;
				}
				if (wres_prio_is_expired(&prio) || (prio.id != user))
				    break;
#endif
			}
		} while (!wres_record_next(path, &pos));
	}
unlock:
	wres_prio_unlock(resource);
	free(args);
	return NULL;
}


void wres_prio_clear(wres_req_t *req)
{	
	wres_clear_flags(&req->resource, WRES_REDO);
}


#ifdef MODE_EXPLICIT_PRIORITY
int wres_prio_extract(wres_req_t *req, int *val, wres_time_t *live)
{
	int ret = -EINVAL;
	wresource_t *resource = &req->resource;
	wres_op_t op = wres_get_op(resource);

	switch (op) {
	case WRES_OP_SHMFAULT:
	{
		wres_shmfault_args_t *args = (wres_shmfault_args_t *)req->buf;
		
		*val = args->prio;
		*live = args->live;
		return 0;
	}
	default:
		break;
	}
	return ret;
}
#endif


int wres_prio_check(wres_req_t *req)
{
	int ret = 0;
	int empty = 0;
	int expire = 0;
	wres_prio_t prio;
	wres_index_t index;
	char path[WRES_PATH_MAX];
	wresource_t *resource = &req->resource;
#ifdef MODE_EXPLICIT_PRIORITY
	wres_time_t live;
	int val;
#endif	
	
	wres_prio_lock(resource);
	ret = wres_prio_get(resource, &prio);
	if (ret) {
		wres_print_err(resource, "failed to get priority");
		goto out;
	}
#ifdef MODE_EXPLICIT_PRIORITY
	ret = wres_prio_extract(req, &val, &live);
	if (ret) {
		wres_print_err(resource, "failed to extract priority");
		goto out;
	}
#endif 
	expire = wres_prio_is_expired(&prio);
	if (!expire) {
#ifdef MODE_EXPLICIT_PRIORITY
		if (val >= prio.val)
			goto out;
#else
		wres_id_t id = wres_get_id(resource);
		
		if (wres_prio_compare(resource, id, prio.id) >= 0)
			goto out;
#endif
	}
	
	wres_get_resource_path(resource, path);
	empty = wres_record_empty(path);
	if (empty < 0) {
		wres_print_err(resource, "failed to check record");
		ret = -EINVAL;
		goto out;
	}
	if (expire && empty)
		goto out;

	ret = wres_record_save(path, req, &index);
	if (ret) {
		wres_print_err(resource, "failed to save record");
		goto out;
	}
	
	if (empty) {
		pthread_t tid;
		wresource_t *args;
		pthread_attr_t attr;
		
		args = (wresource_t *)malloc(sizeof(wresource_t));
		if (!args) {
			wres_print_err(resource, "no memory");
			ret = -ENOMEM;
			goto out;
		}
		memcpy(args, resource, sizeof(wresource_t));
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
		ret = pthread_create(&tid, &attr, wres_prio_select, args);
		pthread_attr_destroy(&attr);
		if (ret) {
			wres_print_err(resource, "failed to create thread");
			free(args);
			goto out;
		}
	}
	wres_prio_unlock(resource);
	wres_print_prio_check(req);
	return -EAGAIN;
out:
#ifdef MODE_EXPLICIT_PRIORITY
	if (!ret)
		ret = wres_prio_update(resource, val, live);
#endif
	wres_prio_unlock(resource);
	wres_print_prio_check(req);
	return ret;
}


int wres_prio_sync_time(wresource_t *resource)
{
	int ret;
	wres_prio_t prio;
	wres_time_t time;
	
	wres_prio_lock(resource);
	ret = wres_prio_get(resource, &prio);
	if (ret) {
		wres_print_err(resource, "failed to get priority");
		goto out;
	}
	time = get_time();
	if (time - prio.t_sync > WRES_PRIO_SYNC_INTERVAL) {
		wres_time_t off;
		
		ret = wres_synctime_request(resource, &off);
		if (ret) {
			wres_print_err(resource, "failed send request");
			goto out;
		}
		prio.t_sync = time;
		prio.t_off = off;
		ret = wres_prio_set(resource, &prio);
		if (ret) {
			wres_print_err(resource, "failed to save priority");
			goto out;
		}
		wres_print_prio_sync_time(resource, off);
	}
out:
	wres_prio_unlock(resource);
	return ret;
}

