/*      file.c
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

#include "file.h"
#include "path.h"

static int wcfs_file_get_desc(const char *path, unsigned long off, int flags, wcfs_file_t *file)
{
	char *end;
	const char *p = path;
	
	if (*p++ != '/') {
		wcfs_log("invalid path");
		return -EINVAL;
	}
	bzero(file, sizeof(wcfs_file_t));
	file->id = (pid_t)strtoul(p, &end, 16);
	if (p == end) {
		wcfs_log("invalid path");
		return -EINVAL;
	}
	p = end + 1;
	file->area = wcfs_strip(strtoul(p, &end, 16));
	file->off = wcfs_strip(off);
	file->flags = flags;
	return 0;
}


int wcfs_file_open(const char *path, unsigned long off, int flags, wcfs_file_t *file)
{
	int ret;
	int desc = -1;
	char name[WCFS_PATH_MAX];
	
	ret = wcfs_file_get_desc(path, off, flags, file);
	if (ret) {
		wcfs_log("failed to get desc");
		return ret;
	}
	ret = wcfs_path_get(file, name);
	if (ret) {
		wcfs_log("failed to get path");
		return ret;
	}
	if (flags & WCFS_READ) {
		desc = open(name, O_RDONLY);
		if (desc < 0) {
			if (wcfs_path_is_marked(name))
				wcfs_path_remove_mark(name);
			else
				wcfs_path_mark(name);
			desc = open(name, O_RDONLY);
		}
	} else if (flags & WCFS_WRITE) {
		if (!access(name, F_OK)) {
			char tmp[WCFS_PATH_MAX];
			
			strncpy(tmp, name, WCFS_PATH_MAX);
			if (wcfs_path_is_marked(tmp))
				wcfs_path_remove_mark(tmp);
			else
				wcfs_path_mark(tmp);
			ret = rename(name, tmp);
			if (ret) {
				wcfs_log("failed to rename");
				return ret;
			}
		}
		desc = open(name, O_CREAT | O_EXCL | O_WRONLY);
		if (desc < 0) {
			wcfs_path_check_dir(name);
			desc = open(name, O_CREAT | O_EXCL | O_WRONLY);
		}
	}
	if (desc < 0)
		return -errno;
	file->desc = desc;
	return 0;
}


void wcfs_file_close(wcfs_file_t *file)
{
	close(file->desc);
}


int wcfs_file_get_header(const char *path, wcfs_file_header_t *hdr)
{
	int desc;
	wcfs_file_t file;
	char name[WCFS_PATH_MAX];
	
	wcfs_file_get_desc(path, 0, 0, &file);
	wcfs_path_header(&file, name);
	desc = open(name, O_RDWR);
	if (desc < 0) {
		wcfs_path_check_dir(name);
		desc = open(name, O_RDWR | O_CREAT);
		if (desc < 0) {
			wcfs_log("failed to get header");
			return -EINVAL;
		}
		memset(hdr, 0, sizeof(wcfs_file_header_t));
		if (write(desc, hdr, sizeof(wcfs_file_header_t)) != sizeof(wcfs_file_header_t)) {
			wcfs_log("failed to get header");
			close(desc);
			return -EIO;
		}
	}
	hdr->desc = desc;
	return 0;
}


void wcfs_file_put_header(wcfs_file_header_t *hdr)
{
	lseek(hdr->desc, 0, SEEK_SET);
	write(hdr->desc, hdr, sizeof(wcfs_file_header_t));
	close(hdr->desc);
}


void wcfs_file_truncate(const char *path, unsigned long start, unsigned long end)
{
	char *p;
	char name[WCFS_PATH_MAX];
	wcfs_file_t file;
	
	wcfs_file_get_desc(path, 0, 0, &file);
	wcfs_path_area(&file, name);
	p = name + strlen(name);
	while (start < end) {
		wcfs_path_append(name, start);
		remove(name);
		wcfs_path_mark(name);
		remove(name);
		p[0] = '\0';
		start++;
	}
}


int wcfs_file_setlen(const char *path, size_t length)
{
	wcfs_file_header_t hdr;
	
	if (wcfs_file_get_header(path, &hdr) < 0) {
		wcfs_log("failed to get header");
		return -ENOENT;
	}
	if (length < hdr.length)
		wcfs_file_truncate(path, length, hdr.length);
	hdr.length = length;
	hdr.ver = !hdr.ver;
	wcfs_file_put_header(&hdr);
	return 0;
}


int wcfs_file_read(wcfs_file_t *file, void *buf, size_t len)
{
	return read(file->desc, buf, len);
}


int wcfs_file_write(wcfs_file_t *file, void *buf, size_t len)
{
	return write(file->desc, buf, len);
}


int wcfs_file_update(const char *path, unsigned long off, void *buf, size_t len)
{
	int ret;
	wcfs_file_t file;
	
	ret = wcfs_file_open(path, off, WCFS_WRITE, &file);
	if (ret < 0) {
		wcfs_log("failed to open file");
		return ret;
	}
	if (wcfs_file_write(&file, buf, len) != len) {
		wcfs_log("failed to write");
		ret = -EIO;
	}
	wcfs_file_close(&file);
	return ret;
}

