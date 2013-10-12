//      ckpt.c
//      
//      Copyright 2013 Yi-Wei Ci <ciyiwei@hotmail.com>
//      
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; either version 2 of the License, or
//      (at your option) any later version.
//      
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//      
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
//      MA 02110-1301, USA.

#include "ckpt.h"

static void wdump_ckpt_get_path(pid_t pid, char *path)
{
	sprintf(path, "%s%lx%s", WDUMP_CKPT_PATH, (unsigned long)pid, WDUMP_CKPT_SUFFIX);
}


void wdump_ckpt_init_header(wdump_ckpt_header_t *hdr)
{
	strncpy(hdr->signature, "WDUMP", 4);
	hdr->major = WDUMP_MAJOR;
	hdr->minor = WDUMP_MINOR;
}


int wdump_ckpt_check_header(wdump_ckpt_header_t *hdr)
{
	if (strncmp(hdr->signature, "WDUMP", 4) 
		|| (hdr->major != WDUMP_MAJOR) 
		|| (hdr->minor != WDUMP_MINOR)) {
		wdump_log("invalid checkpoint header");
		return -EINVAL;
	}
	return 0;
}


int wdump_ckpt_save(struct task_struct *tsk) 
{
	int ret;
	wdump_desc_t desc;
	wdump_ckpt_header_t hdr;
	char path[WDUMP_CKPT_PATH_MAX];
	
	wdump_ckpt_get_path(tsk->gpid, path);
	desc = wdump_open(path);
	if (IS_ERR(desc))
		return PTR_ERR(desc);
	
	wdump_ckpt_init_header(&hdr);
	if (wdump_write(desc, (void *)&hdr, sizeof(wdump_ckpt_header_t)) != sizeof(wdump_ckpt_header_t)) {
		ret = -EIO;
		goto out;
	}
	
	ret = wdump_save(desc, tsk);
	if (ret)
		goto out;
	
	ret = wdump_write(desc, NULL, 0);
out:
	wdump_close(desc);
	return ret;
}


int wdump_ckpt_restore(pid_t pid)
{
	int ret;
	wdump_desc_t desc;
	wdump_ckpt_header_t hdr;
	char path[WDUMP_CKPT_PATH_MAX];
	
	wdump_ckpt_get_path(pid, path);
	desc = wdump_open(path);
	if (IS_ERR(desc))
		return PTR_ERR(desc);
		
	if (wdump_read(desc, (void *)&hdr, sizeof(wdump_ckpt_header_t)) != sizeof(wdump_ckpt_header_t)) {
		ret = -EIO;
		goto out;
	}
	
	ret = wdump_ckpt_check_header(&hdr);
	if (ret)
		goto out;
		
	ret = wdump_restore(desc);
out:
	wdump_close(desc);
	return ret;		
}
