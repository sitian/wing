/*      msg.c
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

#include "msg.h"

int wres_msg_create(wresource_t *resource)
{
	int ret = 0;
	struct msqid_ds msq;
	stringio_file_t *filp;
	char path[WRES_PATH_MAX];
	
	bzero(&msq, sizeof(struct msqid_ds));
	msq.msg_perm.mode = wres_get_flags(resource) & S_IRWXUGO;
	msq.msg_perm.__key = resource->key;
	msq.msg_ctime = time(0);
	msq.msg_qbytes = WRES_MSGMNB;
	wres_get_state_path(resource, path);
	filp = stringio_open(path, "w");
	if (!filp)
		return -ENOENT;
	if (stringio_write(&msq, sizeof(struct msqid_ds), 1, filp) != 1)
		ret = -EIO;
	stringio_close(filp);
	return ret;
}


int wres_msg_check(wresource_t *resource, struct msqid_ds *msq)
{
	int ret = 0;
	stringio_file_t *filp;
	char path[WRES_PATH_MAX];

	wres_get_state_path(resource, path);
	filp = stringio_open(path, "r");
	if (!filp)
		return -ENOENT;
	if (stringio_read(msq, sizeof(struct msqid_ds), 1, filp) != 1)
		ret = -EIO;
	stringio_close(filp);
	return ret;
}


int wres_msg_update(wresource_t *resource, struct msqid_ds *msq)
{
	int ret = 0;
	stringio_file_t *filp;
	char path[WRES_PATH_MAX];
	
	wres_get_state_path(resource, path);
	filp = stringio_open(path, "w");
	if (!filp)
		return -ENOENT;
	
	if (stringio_write(msq, sizeof(struct msqid_ds), 1, filp) != 1)
		ret = -EIO;
	stringio_close(filp);
	return ret;
}


wres_reply_t *wres_msg_msgsnd(wres_req_t *req, int flags)
{
	int mode;
	int msgflg;
	long msgtyp;
	long ret = 0;
	struct msqid_ds msq;
	char path[WRES_PATH_MAX];
	wres_msgrcv_args_t *msgrcv_args = NULL;
	wres_msgsnd_args_t *msgsnd_args = NULL;
	wresource_t *resource = &req->resource;
	wresource_t res = req->resource;
	wres_reply_t *reply = NULL;
	wres_index_t index = -1;
	wres_record_t record;
	wres_index_t start;
	size_t msgsz;
	
	if (flags & WRES_CANCEL) {
		ret = -EINVAL;
		goto out;
	}

	msgsnd_args = (wres_msgsnd_args_t *)req->buf;
	if (req->length < msgsnd_args->msgsz + sizeof(wres_msgsnd_args_t) + sizeof(long)) {
		ret = -EINVAL;
		goto out;
	}
	msgsz = msgsnd_args->msgsz;
	msgtyp = msgsnd_args->msgtyp;
	msgflg = msgsnd_args->msgflg;
	
	// check mesage queue
	ret = wres_msg_check(resource, &msq);
	if (ret) {
		ret = -EFAULT;
		goto out;
	}
	if ((msq.msg_cbytes + msgsnd_args->msgsz > msq.msg_qbytes) || (1 + msq.msg_qnum > msq.msg_qbytes)) {
		if (msgflg & IPC_NOWAIT)
			ret = -EAGAIN;
		else {
			wres_get_record_path(resource, path);
			ret = wres_record_save(path, req, &index);
			if (!ret)
				ret = wres_index_to_err(index);
		}
		goto out;
	}
	
	wres_set_op(&res, WRES_OP_MSGRCV);
	wres_get_record_path(&res, path);
	ret = wres_record_first(path, &start);
	if (!ret) {
		// check the waiting list
		for (;;) {
			ret = wres_record_get(path, start, &record);
			if (ret)
				goto out;
			msgrcv_args = (wres_msgrcv_args_t *)record.req->buf;
			mode = wres_msg_convert_mode(&msgrcv_args->msgtyp, msgrcv_args->msgflg);
			if (wres_msgtyp_cmp(msgtyp, msgrcv_args->msgtyp, mode)) {
				index = start;
				break;
			}
			wres_record_put(&record);
			ret = wres_record_next(path, &start);
			if (ret)
				break;
		}
	}
	
	if (ret && ret != -ENOENT)
		goto out;
	
	if (index >= 0) {
		wres_id_t src;
		res = record.req->resource;
		src = wres_get_id(&res);
		wres_set_off(&res, index);
		wres_set_op(&res, WRES_OP_REPLY);
		
		if ((msgrcv_args->msgsz >= msgsz) || (msgrcv_args->msgflg & MSG_NOERROR)) {
			size_t size = msgrcv_args->msgsz > msgsz ? msgsz : msgrcv_args->msgsz;
			size_t outlen = size + sizeof(long) + sizeof(wres_msgrcv_result_t);
			char *output = malloc(outlen);
			
			if (!output) {
				ret = -ENOMEM;
				goto release;
			}
			msq.msg_stime = time(0);
			msq.msg_rtime = msq.msg_stime;
			msq.msg_lspid = wres_get_id(resource);
			msq.msg_lrpid = src;
			ret = wres_msg_update(resource, &msq);
release:
			wres_record_put(&record);
			wres_record_remove(path, index);
			if (!ret) {
				wres_msgrcv_result_t *msgrcv_result = (wres_msgrcv_result_t *)output;
				msgrcv_result->retval = size;
				memcpy(&msgrcv_result[1], (struct msgbuf *)&msgsnd_args[1], size + sizeof(long));
				wres_ioctl(&res, output, outlen, NULL, 0, &src);
			} else {
				wres_msgrcv_result_t msgrcv_result;
				msgrcv_result.retval = ret;
				wres_ioctl(&res, (char *)&msgrcv_result, sizeof(msgrcv_result), NULL, 0, &src);
			}
			if (output)
				free(output);
		} else {
			wres_msgrcv_result_t msgrcv_result;
			wres_record_put(&record);
			msgrcv_result.retval = -E2BIG;
			wres_ioctl(&res, (char *)&msgrcv_result, sizeof(msgrcv_result), NULL, 0, &src);
		}
	} else { // no receiver
		msq.msg_lspid = wres_get_id(resource);
		msq.msg_stime = time(0);
		msq.msg_cbytes += msgsz;
		msq.msg_qnum++;

		wres_get_record_path(resource, path);
		ret = wres_record_save(path, req, &index);
		if (!ret) {
			ret = wres_msg_update(resource, &msq);
			if (ret) {
				wres_record_remove(path, index);
				ret = -EFAULT;
				goto out;
			}
		}
	}
out:
	wres_generate_reply(wres_msgsnd_result_t, ret, reply);
	wres_print_msg(resource);
	return reply;
}


wres_reply_t *wres_msg_msgrcv(wres_req_t *req, int flags)
{
	int mode;
	int msgflg;
	long msgtyp;
	long ret = 0;
	char path[WRES_PATH_MAX];
	wres_msgrcv_args_t *msgrcv_args = NULL;
	wres_msgsnd_args_t *msgsnd_args = NULL;
	wresource_t *resource = &req->resource;
	wresource_t dest = req->resource;
	wres_reply_t *reply = NULL;
	wres_index_t index = -1;
	wres_record_t record;
	wres_index_t start;
	size_t msgsz;

	if (flags & WRES_CANCEL) {
		ret = -EINVAL;
		goto out;
	}
	
	if (req->length < sizeof(wres_msgrcv_args_t)) {
		ret = -EINVAL;
		goto out;
	}
	
	msgrcv_args = (wres_msgrcv_args_t *)req->buf;
	msgflg = msgrcv_args->msgflg;
	msgsz = msgrcv_args->msgsz;
	msgtyp = msgrcv_args->msgtyp;
	mode = wres_msg_convert_mode(&msgtyp, msgflg);
	
	wres_set_op(&dest, WRES_OP_MSGSND);
	wres_get_record_path(&dest, path); 
	ret = wres_record_first(path, &start);
	if (!ret) {
		// check the waiting senders
		for (;;) {
			ret = wres_record_get(path, start, &record);
			if (ret)
				goto out;
			msgsnd_args = (wres_msgsnd_args_t *)record.req->buf;
			if (wres_msgtyp_cmp(msgsnd_args->msgtyp, msgtyp, mode)) {
				index = start;
				if ((SEARCH_LESSEQUAL == mode) && (msgsnd_args->msgtyp != 1))
					msgtyp = msgsnd_args->msgtyp - 1;
				else
					break;
			}
			wres_record_put(&record);
			ret = wres_record_next(path, &start);
			if (ret)
				break;
		}
	}
	if (-ENOENT == ret) {
		if (index >= 0) {
			ret = wres_record_get(path, index, &record);
			if (ret)
				goto out;
		}
	} else if (ret)
		goto out;
	if (index >= 0) {
		msgsnd_args = (wres_msgsnd_args_t *)record.req->buf;
		if ((msgsnd_args->msgsz <= msgsz) || (msgflg & MSG_NOERROR)) {
			int outlen;
			int bytes_read;
			struct msqid_ds msq;
			wres_msgrcv_result_t *result;
			
			ret = wres_msg_check(resource, &msq);
			if (ret) {
				wres_record_put(&record);
				goto out;
			}
			if ((record.req->length > WRES_BUF_MAX) || (record.req->length < 0)) {
				ret = -EINVAL;
				goto release;
			}
			bytes_read = sizeof(long) + (msgsnd_args->msgsz > msgsz ? msgsz : msgsnd_args->msgsz);
			outlen = sizeof(wres_msgrcv_result_t) + bytes_read;
			reply = wres_reply_alloc(outlen);
			if (!reply) {
				ret = -ENOMEM;
				goto release;
			}
			result = wres_result_check(reply, wres_msgrcv_result_t);
			result->retval = bytes_read - sizeof(long);
			memcpy(&result[1], &record.req->buf[sizeof(wres_msgsnd_args_t)], bytes_read);
			msq.msg_qnum--;
			msq.msg_rtime = time(0);
			msq.msg_lrpid = wres_get_id(resource);
			msq.msg_cbytes -= msgsnd_args->msgsz;
			ret = wres_msg_update(resource, &msq);
release:
			wres_record_put(&record);
			wres_record_remove(path, index);
			if (ret && reply) {
				free(reply);
				reply = NULL;
			}
		} else {
			ret = -E2BIG;
			wres_record_put(&record);
		}
	} else if (msgflg & IPC_NOWAIT) 
		ret = -ENOMSG;
	else {
		wres_get_record_path(resource, path);
		ret = wres_record_save(path, req, &index);
		if (!ret)
			ret = wres_index_to_err(index);
	}
out:
	if (!reply)
		wres_generate_reply(wres_msgrcv_result_t, ret, reply);
	wres_print_msg(resource);
	return reply;
}


wres_reply_t *wres_msg_msgctl(wres_req_t *req, int flags)
{
	long ret = 0;
	wres_msgctl_result_t *result;
	wresource_t *resource = &req->resource;
	wres_msgctl_args_t *args = (wres_msgctl_args_t *)req->buf;
	int outlen = sizeof(wres_msgctl_result_t);
	int cmd = args->cmd;
	struct msqid_ds msq;
	wres_reply_t *reply;
	
	if ((flags & WRES_CANCEL) || (req->length < sizeof(wres_msgctl_args_t))) 
		return wres_reply_err(-EINVAL);

	switch (cmd) {
	case IPC_INFO:
	case MSG_INFO:
		outlen += sizeof(struct msginfo);
		break;
	case IPC_STAT:
	case MSG_STAT:
		outlen += sizeof(struct msqid_ds);
	case IPC_SET:
		ret = wres_msg_check(resource, &msq);
		break;
	}
	if (!ret) {
		reply = wres_reply_alloc(outlen);
		if (!reply)
			ret = -ENOMEM;
	}
	if (ret)
		return wres_reply_err(ret);
	result = wres_result_check(reply, wres_msgctl_result_t);
	switch (cmd) {
	case IPC_INFO:
	case MSG_INFO:
	{
		struct msginfo *msginfo = (struct msginfo *)&result[1];
		
		bzero(msginfo, sizeof(struct msginfo));
		msginfo->msgmni = WRES_MSGMNI;
		msginfo->msgmax = WRES_MSGMAX;
		msginfo->msgmnb = WRES_MSGMNB;
		msginfo->msgssz = WRES_MSGSSZ;
		msginfo->msgseg = WRES_MSGSEG;
		msginfo->msgmap = WRES_MSGMAP;
		msginfo->msgpool = WRES_MSGPOOL;
		msginfo->msgtql = WRES_MSGTQL;
		
		if (MSG_INFO == cmd) {
			msginfo->msgpool = wres_get_resource_count(WRES_CLS_MSG);
			//TODO: set msginfo->msgmap
			//TODO: set msginfo->msgtql
		}
		ret = wres_get_max_key(WRES_CLS_MSG);
		break;
	}
	case MSG_STAT:
		ret = resource->key;
	case IPC_STAT:
		memcpy((struct msqid_ds *)&result[1], &msq, sizeof(struct msqid_ds));	
		break;
	case IPC_SET:
	{
		struct msqid_ds *buf = (struct msqid_ds *)&args[1];
		
		memcpy(&msq.msg_perm, &buf->msg_perm, sizeof(sizeof(struct ipc_perm)));
		msq.msg_qbytes = buf->msg_qbytes;
		msq.msg_ctime = time(0);
		ret = wres_msg_update(resource, &msq);
		break;
	}
	case IPC_RMID:
		wres_redo_all(resource, WRES_CANCEL);
		wres_free(resource);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	result->retval = ret;
	return reply;
}
