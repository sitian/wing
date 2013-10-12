/*      sem.c
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

#include "sem.h"

wres_semundo_t *wres_sem_alloc_undo(unsigned long nsems, pid_t pid)
{
	size_t size = sizeof(wres_semundo_t) + nsems * sizeof(short);
	wres_semundo_t *un = (wres_semundo_t *)calloc(1, size);
	
	if (!un) {
		wres_log("failed");
		return NULL;
	}
	
	un->nsems = nsems;
	un->pid = pid;
	return un;
}


int wres_sem_create(wresource_t *resource)
{
	int ret = 0;
	int nsems = wres_entry_val1(resource->entry);
	size_t size = wres_sma_size(nsems);
	struct wres_sem_array *sma;
	char path[WRES_PATH_MAX];
	stringio_file_t *filp;
	
	sma = (struct wres_sem_array *)calloc(1, size);
	if (!sma)
		return -ENOMEM;
	
	sma->sem_perm.mode = wres_get_flags(resource) & S_IRWXUGO;
	sma->sem_perm.__key = resource->key;
	sma->sem_nsems = nsems;
	sma->sem_ctime = time(0);
	wres_get_state_path(resource, path);
	filp = stringio_open(path, "w");
	if (!filp) {
		free(sma);
		return -ENOENT;
	}
	if (stringio_write((char *)sma, 1, size, filp) != size)
		ret = -EIO;
	stringio_close(filp);
	free(sma);
 	return ret;
}


int wres_sem_get_args(wresource_t *resource, wres_args_t *args)
{
	if (WRES_OP_SEMOP == wres_get_op(resource)) {
		char zero[sizeof(struct timespec)] = {0};
		wres_semop_args_t *semop_args = (wres_semop_args_t *)args->in;
		struct timespec *timeout = &semop_args->timeout;
		
		if (args->inlen < sizeof(wres_semop_args_t))
			return -EINVAL;
		
		if (memcmp(timeout, zero, sizeof(struct timespec)))
			args->timeout = timeout;
	}
	return 0;
}


int wres_sem_is_owner(wresource_t *resource)
{
	char path[WRES_PATH_MAX];
	
	wres_get_state_path(resource, path);
	if (!stringio_access(path, F_OK))
		return 1;
	else
		return 0;
}


struct wres_sem_array *wres_sem_check(wresource_t *resource)
{
	char *buf;
	char path[WRES_PATH_MAX];
	struct wres_sem_array *sma = NULL;
	stringio_file_t *filp;
	size_t size;
	
	wres_get_state_path(resource, path);
	filp = stringio_open(path, "r");
	if (!filp)
		return NULL;
	
	size = stringio_size(filp);
	if (size < sizeof(struct wres_sem_array))
		goto out;
	buf = malloc(size);
	if (!buf)
		goto out;
	if (stringio_read(buf, 1, size, filp) != size) {
		free(buf);
		goto out;
	}
	if (size != wres_sma_size(((struct wres_sem_array *)buf)->sem_nsems)) {
		free(buf);
		goto out;
	}
	sma = (struct wres_sem_array *)buf;
out:
	stringio_close(filp);
	return sma;
}


int wres_sem_update(wresource_t *resource, struct wres_sem_array  *sma)
{
	int ret = 0;
	char path[WRES_PATH_MAX];
	stringio_file_t *filp;
	size_t size;
	
	wres_get_state_path(resource, path);
	filp = stringio_open(path, "w");
	if (!filp)
		return -ENOENT;
	
	size = wres_sma_size(sma->sem_nsems);
	if (stringio_write((char *)sma, 1, size, filp) != size)
		ret = -EIO;
	stringio_close(filp);
 	return ret;
}


wres_semundo_t *wres_sem_undo_at(wres_semundo_t *undos, size_t pos)
{
	size_t size = wres_semundo_size(undos);
	
	return (wres_semundo_t *)((char *)undos + pos * size);
}


wres_semundo_t *wres_sem_check_undo(wresource_t *resource)
{
	int i;
	int total;
	char path[WRES_PATH_MAX];
	pid_t pid = wres_get_id(resource);
	wres_semundo_t *un = NULL;
	stringio_entry_t *entry;
	wres_semundo_t *undos;
	size_t undo_size;
	
	wres_get_action_path(resource, path);
	entry = stringio_get_entry(path, 0, STRINGIO_RDONLY | STRINGIO_AUTO_INCREMENT);
	if (!entry)
		return NULL;
	
	undos = stringio_get_desc(entry, wres_semundo_t);
	undo_size = wres_semundo_size(undos);
	total = stringio_items_count(entry, undo_size);
	for (i = 0; i < total; i++) {
		wres_semundo_t *undo = wres_sem_undo_at(undos, i);
		
		if (undo->pid == pid) {
			un = (wres_semundo_t *)malloc(undo_size);
			if (un)
				memcpy(un, undo, undo_size);
			break;
		}
	}
	stringio_put_entry(entry);
	return un;
}


void wres_sem_free_undo(wres_semundo_t *un)
{
	un->pid = -1;
}


int wres_sem_is_empy_undo(wres_semundo_t *un)
{
	if (un->pid < 0)
		return 1;
	return 0;
}


int wres_sem_update_undo(wresource_t *resource, wres_semundo_t *un)
{
	int ret = 0;
	int updated = 0;
	char path[WRES_PATH_MAX];
	stringio_entry_t *entry;
	size_t undo_size;

	if (!un) {
		wres_log("failed (empty undo)");
		return -EINVAL;
	}
	
	undo_size = wres_semundo_size(un);
	wres_get_action_path(resource, path);
	entry = stringio_get_entry(path, 0, STRINGIO_RDWR | STRINGIO_AUTO_INCREMENT);
	if (entry) {
		int i;
		int total;
		wres_semundo_t *undos = stringio_get_desc(entry, wres_semundo_t);
		
		total = stringio_items_count(entry, undo_size);
		for (i = 0; i < total; i++) {
			wres_semundo_t *undo = wres_sem_undo_at(undos, i);
			
			if (wres_sem_is_empy_undo(undo)) {
				memcpy(undo, un, undo_size);
				updated = 1;
				break;
			}
		}
	}
	if (!updated) {
		if (!entry) {
			entry = stringio_get_entry(path, 0, STRINGIO_RDWR | STRINGIO_CREAT);
			if (!entry)
				return -ENOENT;
		}
		ret = stringio_append(entry, (char *)un, undo_size);
	}
	stringio_put_entry(entry);
 	return ret;
}


void wres_sem_reset_undos(wresource_t *resource, int semnum)
{
	int i;
	int total;
	size_t undo_size;
	wres_semundo_t *undo;
	wres_semundo_t *undos;
	stringio_entry_t *entry;
	char path[WRES_PATH_MAX];
	
	wres_get_action_path(resource, path);
	entry = stringio_get_entry(path, 0, STRINGIO_RDWR | STRINGIO_AUTO_INCREMENT);
	if (!entry)
		return;
	
	undos = stringio_get_desc(entry, wres_semundo_t);
	undo_size = wres_semundo_size(undos);
	total = stringio_items_count(entry, undo_size);
	for (i = 0; i < total; i++) {
		undo = wres_sem_undo_at(undos, i);
		undo->semadj[semnum] = 0;
	}
	stringio_put_entry(entry);
}


void wres_sem_remove_undo(wresource_t *resource)
{
	int i;
	int total;
	size_t undo_size;
	wres_semundo_t *undos;
	stringio_entry_t *entry;
	char path[WRES_PATH_MAX];
	pid_t pid = wres_get_id(resource);
	
	wres_get_action_path(resource, path);
	entry = stringio_get_entry(path, 0, STRINGIO_RDWR | STRINGIO_AUTO_INCREMENT);
	if (!entry)
		return;
	
	undos = stringio_get_desc(entry, wres_semundo_t);
	undo_size = wres_semundo_size(undos);
	total = stringio_items_count(entry, undo_size);
	for (i = 0; i < total; i++) {
		wres_semundo_t *undo = wres_sem_undo_at(undos, i);
		
		if (undo->pid == pid) { 
			wres_sem_free_undo(undo);
			break;
		}
	}
	stringio_put_entry(entry);
}


int wres_sem_remove_all_undos(wresource_t *resource)
{
	char path[WRES_PATH_MAX];
	
	wres_get_action_path(resource, path);
	return stringio_remove(path);
}


wres_reply_t *wres_sem_semop(wres_req_t *req,  int flags)
{
	int max = 0;
	long ret = 0;
	int undos = 0;
	struct sembuf *sop;
	wres_reply_t *reply;
	wres_semundo_t *un = NULL;
	struct wres_sem_array *sma;
	wresource_t *resource = &req->resource;
	wres_semop_args_t *args = (wres_semop_args_t *)req->buf;
	unsigned nsops = args->nsops;
	size_t len = sizeof(wres_semop_args_t) + nsops * sizeof(struct sembuf);
	struct sembuf *sops = args->sops;
	

	if ((flags & WRES_CANCEL) || (req->length < len))
		return wres_reply_err(-EINVAL);
	
	sma = wres_sem_check(resource);
	if (!sma)
		return wres_reply_err(-EFAULT);

	for (sop = sops; sop < sops + nsops; sop++) {
		if (sop->sem_num >= max)
			max = sop->sem_num;
		if (sop->sem_flg & SEM_UNDO)
			undos = 1;
	}
	
	if (undos) {
		un = wres_sem_check_undo(resource);
		if (!un) {
			un = wres_sem_alloc_undo(sma->sem_nsems, wres_get_id(resource));
			if (!un) {
				free(sma);
				return wres_reply_err(-EFAULT);
			}
		}
	}
	
	if (max >= sma->sem_nsems) {
		wres_log("failed (invalid sem_num)");
		ret = -EFBIG;
		goto out;
	}
	
	for (sop = sops; sop < sops + nsops; sop++) {
		struct wres_sem *curr = sma->sem_base + sop->sem_num;
		int sem_op = sop->sem_op;
		int semval = curr->semval + sem_op;
		
		if ((!sem_op && curr->semval) || (semval < 0)) {
			if (!(flags & WRES_REDO) && !(sop->sem_flg & IPC_NOWAIT)) {
				wres_index_t index;
				char path[WRES_PATH_MAX];

				wres_get_record_path(resource, path);
				ret = wres_record_save(path, req, &index);
				if (!ret)
					ret = wres_index_to_err(index);
			} else 
				ret = -EAGAIN;
			goto out;
		}
		if (semval > WRES_SEMVMX) {
			wres_log("failed (invalid semval)");
			ret = -ERANGE;
			goto out;
		}
		if (sop->sem_flg & SEM_UNDO) {
			int undo = un->semadj[sop->sem_num] - sem_op;
			
			if (undo < (-WRES_SEMVMX - 1) || undo > WRES_SEMVMX) {
				wres_log("failed (invalid undo)");
				ret = -ERANGE;
				goto out;
			}
			un->semadj[sop->sem_num] -= sop->sem_op;
		}
		curr->semval = semval;
		curr->sempid =wres_get_id(resource);
		wres_print_semop(resource->key, nsops, sop->sem_num, semval, wres_get_id(resource));
	}
	sma->sem_otime = time(0);
	if (un)
		ret = wres_sem_update_undo(resource, un);
	if (ret) {
		wres_log("failed to update undo");
		ret = -EFAULT;
		goto out;
	}
	ret = wres_sem_update(resource, sma);
	if (ret) {
		wres_log("failed to update sem");
		ret = -EFAULT;
		goto out;
	}
	if (!(flags & WRES_REDO)) {
		ret = wres_redo_all(resource, 0);
		if (ret) {
			wres_log("failed to redo");
			ret = -EFAULT;
			goto out;
		}
	}
	wres_print_sem(resource);
out:
	free(sma);
	if (undos)
		free(un);
	wres_generate_reply(wres_semop_result_t, ret, reply);
	return reply;
}


// get the number of tasks waiting on semval being nonzero
static int wres_sem_getncnt(wresource_t *resource, int semnum)
{
	int ret;
	int ncnt = 0;
	wres_index_t index;
	char path[WRES_PATH_MAX];
	
	wres_get_record_path(resource, path);
	ret = wres_record_first(path, &index);
	if (-ENOENT == ret)
		return 0;
	else if (ret)
		return -EINVAL;
	
	for (;;) {
		int i;
		struct sembuf *sops;
		wres_record_t record;
		wres_semop_args_t *args;
		
		ret = wres_record_get(path, index, &record);
		if (ret)
			return ret;
		args = (wres_semop_args_t *)record.req->buf;
		sops = args->sops;
		for (i = 0; i < args->nsops; i++)
			if ((sops[i].sem_num == semnum) && (sops[i].sem_op < 0) && !(sops[i].sem_flg & IPC_NOWAIT))
				ncnt++;
		wres_record_put(&record);
		ret = wres_record_next(path, &index);
		if (-ENOENT == ret)
			break;
		else if (ret)
			return -EINVAL;
	}
	return ncnt;
}


// get the number of tasks waiting on semval being zero
static int wres_sem_getzcnt(wresource_t *resource, int semnum)
{
	int ret = 0;
	int zcnt = 0;
	wres_index_t index;
	char path[WRES_PATH_MAX];
	
	wres_get_record_path(resource, path);
	ret = wres_record_first(path, &index);
	if (-ENOENT == ret)
		return 0;
	else if (ret)
		return -EINVAL;
	
	for (;;) {
		int i;
		struct sembuf *sops;
		wres_record_t record;
		wres_semop_args_t *args;
		
		ret = wres_record_get(path, index, &record);
		if (ret)
			return ret;
		args = (wres_semop_args_t *)record.req->buf;
		sops = args->sops;
		for (i = 0; i < args->nsops; i++)
			if ((sops[i].sem_num == semnum) && (0 == sops[i].sem_op) && !(sops[i].sem_flg & IPC_NOWAIT))
				zcnt++;
		wres_record_put(&record);
		ret = wres_record_next(path, &index);
		if (-ENOENT == ret)
			break;
		else if (ret)
			return -EINVAL;
	}
	return zcnt;
}


void wres_sem_set(struct seminfo *seminfo)
{
	bzero(seminfo, sizeof(struct seminfo));
	seminfo->semmni = WRES_SEMMNI;
	seminfo->semmns = WRES_SEMMNS;
	seminfo->semmsl = WRES_SEMMSL;
	seminfo->semopm = WRES_SEMOPM;
	seminfo->semvmx = WRES_SEMVMX;
	seminfo->semmnu = WRES_SEMMNU;
	seminfo->semmap = WRES_SEMMAP;
	seminfo->semume = WRES_SEMUME;
	seminfo->semusz = WRES_SEMUSZ;
	seminfo->semaem = WRES_SEMAEM;
}


wres_reply_t *wres_sem_semctl(wres_req_t *req, int flags)
{	
	long ret = 0;
	wres_reply_t *reply = NULL;
	struct wres_sem *sem = NULL;
	wres_semctl_result_t *result;
	struct wres_sem_array *sma = NULL;
	wresource_t *resource = &req->resource;
	wres_semctl_args_t *args = (wres_semctl_args_t *)req->buf;
	int outlen = sizeof(wres_semctl_result_t);
	int semnum = args->semnum;
	int cmd = args->cmd;
	int nsems;

	if ((flags & WRES_CANCEL) || (req->length < sizeof(wres_semctl_args_t)))
		return wres_reply_err(-EINVAL);
	
	sma = wres_sem_check(resource);
	if (!sma)
		return wres_reply_err(-EFAULT);
	nsems = sma->sem_nsems;
	
	switch (cmd) {
	case IPC_INFO:
	case SEM_INFO:
		outlen += sizeof(struct seminfo);
		break;
	case IPC_STAT:
	case SEM_STAT:
		outlen += sizeof(struct semid_ds);
		break;
	case GETALL:
		outlen += sizeof(unsigned short) * nsems;
		break;
	case GETVAL:
	case GETPID:
	case SETVAL:
		sem = &sma->sem_base[semnum];
		break;
	default:
		break;
	}
	
	reply = wres_reply_alloc(outlen);
	if (!reply) {
		free(sma);
		return wres_reply_err(-ENOMEM);
	}
	result = wres_result_check(reply, wres_semctl_result_t);
	
	switch (cmd) {
	case SEM_NSEMS:
		ret = nsems;
		break;
	case IPC_SET:
		memcpy(sma, (struct semid_ds *)&args[1], sizeof(struct semid_ds));
		sma->sem_ctime = time(0);
		ret = wres_sem_update(resource, sma);
		break;
	case IPC_RMID:
		wres_redo_all(resource, WRES_CANCEL);
		wres_free(resource);
		break;
	case IPC_INFO:
	case SEM_INFO:
	{
		struct seminfo *seminfo = (struct seminfo *)&result[1];
		
		wres_sem_set(seminfo);
		if (SEM_INFO == cmd) {
			seminfo->semusz = wres_get_resource_count(WRES_CLS_SEM);
			// TODO: set seminfo->semaem
		}
		ret = wres_get_max_key(WRES_CLS_SEM);
		break;
	}
	case SEM_STAT:
		ret = resource->key;
	case IPC_STAT:
		memcpy((struct semid_ds *)&result[1], sma, sizeof(struct semid_ds)); 
		break;
	case GETALL:
	{
		int i;
		unsigned short *array = (unsigned short *)&result[1];
		
		for (i = 0; i < nsems; i++)
			array[i] = sma->sem_base[i].semval;
		break;
	}
	case SETALL:
	{
		int i;
		unsigned short *array = (unsigned short *)&args[1];
		
		for (i = 0; i < nsems; i++) {
			if (array[i] > WRES_SEMVMX) {
				ret = -ERANGE;
				goto out;
			}
		}
		wres_sem_remove_all_undos(resource);
		for (i = 0; i < nsems; i++)
			sma->sem_base[i].semval = array[i];
		sma->sem_ctime = time(0);
		ret = wres_sem_update(resource, sma);
		if (!ret)
			ret = wres_redo_all(resource, 0);
		break;
	}
	case GETVAL:
		ret = sem->semval;
		break;
	case GETPID:
		ret = sem->sempid;
		break;
	case GETNCNT:
		ret = wres_sem_getncnt(resource, semnum);
		break;
	case GETZCNT:
		ret = wres_sem_getzcnt(resource, semnum);
		break;
	case SETVAL:
		wres_sem_reset_undos(resource, semnum);
		sem->semval = *(int *)&args[1];
		sem->sempid = wres_get_id(resource);
		sma->sem_ctime = time(0);
		ret = wres_sem_update(resource, sma);
		if (!ret)
			ret = wres_redo_all(resource, 0);
		break;
	default:
		ret = -EINVAL;
		break;
	}
out:
	if (sma)
		free(sma);
	result->retval = ret;
	wres_print_sem(resource);
	return reply;
}


void wres_sem_exit(wres_req_t *req)
{
	int i;
	wres_semundo_t *un;
	struct wres_sem_array *sma;
	wresource_t *resource = &req->resource;
	
	sma = wres_sem_check(resource);
	if (!sma)
		return;
	
	un = wres_sem_check_undo(resource);
	if (!un) {
		free(sma);
		return;
	}
	
	for (i = 0; i < sma->sem_nsems; i++) {
		struct wres_sem * sem = &sma->sem_base[i];
		
		if (un->semadj[i]) {
			sem->semval += un->semadj[i];
			if (sem->semval < 0)
				sem->semval = 0;
			if (sem->semval > WRES_SEMVMX)
				sem->semval = WRES_SEMVMX;
			sem->sempid = wres_get_id(resource);
		}
	}
	sma->sem_otime = time(0);
	wres_sem_update(resource, sma);
	wres_sem_remove_undo(resource);
	free(un);
	free(sma);
}
