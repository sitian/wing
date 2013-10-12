/*      wcfs.c
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

#include "wcfs.h"

int wcfs_getattr(const char *path, struct stat *stbuf)
{
	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else {
		stbuf->st_mode = S_IFREG | 0777;
		stbuf->st_nlink = 1;
		stbuf->st_size = 0xffffffff;
	}
	return 0;
}


int wcfs_read(const char *path, char *buf, size_t size, loff_t off, struct fuse_file_info *fi)
{
	int ret;
	wcfs_file_t file;
	
	if ((off != wcfs_page_align(off)) || (size > WCFS_PAGE_SIZE)) {
		wcfs_log("invalid parameters");
		return -EINVAL;
	}
	ret = wcfs_file_open(path, off, WCFS_READ, &file);
	if (ret < 0) {
		if (-ENOENT == ret) {
			memset(buf, 0, size);
			return size;
		} else
			return ret;
	}
	ret = wcfs_file_read(&file, buf, size);
	wcfs_file_close(&file);
	return ret;
}


int wcfs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
	if (wcfs_is_addr(name))
		return wcfs_file_update(path, wcfs_strtoul(name), (void *)value, size);
	else if (!strncmp(name, WCFS_ATTR_AREA_SIZE, WCFS_ATTR_LEN))
		return wcfs_file_setlen(path, *(size_t *)value);
	return -EINVAL;
}


static struct fuse_operations wcfs_oper = {
	.read		= wcfs_read,
	.getattr	= wcfs_getattr,
	.setxattr	= wcfs_setxattr,
};


int main(int argc, char *argv[])
{
	umask(0);
	return fuse_main(argc, argv, &wcfs_oper, NULL);
}
