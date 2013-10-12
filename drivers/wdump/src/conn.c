//      conn.c
//      
//      Copyright 2013 Yi-Wei <ciyiwei@hotmail.com>
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

#include "conn.h"

static inline void wdump_conn_get_path(unsigned long id, unsigned long area, char *path)
{
	sprintf(path, "%s/%lx_%lx", WCFS_MOUNT_PATH, id, area);
}


static inline void wdump_conn_addr2str(unsigned long addr, char *str)
{
	sprintf(str, "0x%lx", addr);
}


int wdump_conn_set(unsigned long id, unsigned long area, char *name, void *value, size_t size)
{
	int ret;
	struct file *file;
	char path[WCFS_PATH_MAX];
	
	wdump_print_conn_set("setting %s", name);
	wdump_conn_get_path(id, area, path);
	file = filp_open(path, WDUMP_CONN_FLAGS, WDUMP_CONN_MODE);
	if (IS_ERR(file))
		return PTR_ERR(file);
	
	ret = vfs_setxattr(file->f_dentry, name, value, size, 0);
	filp_close(file, NULL);
	return 0;
}


int wdump_conn_update(unsigned long id, unsigned long area, unsigned long address, void *buf, size_t len)
{
	char name[WCFS_PATH_MAX];
	unsigned long off = address - area;
	
	wdump_conn_addr2str(off, name);
	return wdump_conn_set(id, area, name, buf, len);
}


int wdump_conn_attach(unsigned long id, unsigned long area, size_t size, int prot, int flags)
{
	unsigned long ret;
	struct file *file;
	char path[WCFS_PATH_MAX];
	
	wdump_conn_get_path(id, area, path);
	file = filp_open(path, WDUMP_CONN_FLAGS, WDUMP_CONN_MODE);
	if (IS_ERR(file))
		return PTR_ERR(file);
	
	ret = do_mmap_pgoff(file, area, size, prot, flags, 0);
	if (ret != area) {
		wdump_log("failed to attach (%d)", (int)ret);
		return -EINVAL;
	}
	wdump_print_conn_attach("path=%s", path);
	return 0;
}

