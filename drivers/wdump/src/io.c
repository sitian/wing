//      io.c
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

#include "io.h"

wdump_desc_t wdump_open(const char *path)
{
	return filp_open(path, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
}


int wdump_close(wdump_desc_t desc)
{
	return filp_close(desc, NULL);
}


int wdump_read(wdump_desc_t desc, void *buf, size_t len)
{
	int ret;
	size_t bytes = len;
	char *ptr = buf;
	mm_segment_t fs = get_fs();
	
	set_fs(KERNEL_DS);
	while (bytes > 0) {
		ret = vfs_read(desc, ptr, bytes, &wdump_desc_pos(desc));
		if (ret < 0)
			goto out;
		bytes -= ret;
		ptr += ret;
	}
	ret = len;
out:
	set_fs(fs);
	return ret; 
}


int wdump_uread(wdump_desc_t desc, void *buf, size_t len)
{
	int ret;
	size_t bytes = len;
	char *ptr = buf;
	
	while (bytes > 0) {
		ret = vfs_read(desc, ptr, bytes, &wdump_desc_pos(desc));
		if (ret < 0)
			return ret;
		bytes -= ret;
		ptr += ret;
	}
	return len; 
}


int wdump_write(wdump_desc_t desc, void *buf, size_t len)
{
	int ret;
	size_t bytes = len;
	const char *ptr = buf;
	mm_segment_t fs = get_fs();
	
	set_fs(KERNEL_DS);
	while (bytes > 0) {
		ret = vfs_write(desc, ptr, bytes, &wdump_desc_pos(desc)); 
		if (ret < 0)
			goto out;
		bytes -= ret;
		ptr += ret;
	}
	ret = len;
out:
	set_fs(fs);
	return ret; 
}
