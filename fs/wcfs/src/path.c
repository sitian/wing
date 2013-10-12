/*      path.c
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

#include "path.h"

void wcfs_path_mark(char *path)
{
	strcat(path, WCFS_PATH_MARK);
}


void wcfs_path_append(char *path, unsigned long val)
{
	sprintf(path + strlen(path), "%lx", val);
}


void wcfs_path_area(wcfs_file_t *file, char *path)
{
	sprintf(path, "%s/%lx/%lx/", WCFS_PATH_HOME, file->id, file->area);
}


void wcfs_path_off(wcfs_file_t *file, char *path)
{
	wcfs_path_area(file, path);
	wcfs_path_append(path, file->off);
}


void wcfs_path_header(wcfs_file_t *file, char *path)
{
	wcfs_path_area(file, path);
	wcfs_path_mark(path);
}


int wcfs_path_get(wcfs_file_t *file, char *path)
{
	int fd;
	int ver = 0;
	int flags = file->flags;
	char name[WCFS_PATH_MAX];
	
	wcfs_path_header(file, name);
	fd = open(name, O_RDWR);
	if (fd >= 0) {
		wcfs_file_header_t hdr;
		if (read(fd, &hdr, sizeof(wcfs_file_header_t)) != sizeof(wcfs_file_header_t)) {
			close(fd);
			return -EIO;
		}
		if (flags & WCFS_WRITE)
			ver = hdr.ver;
		else if (flags & WCFS_READ)
			ver = !hdr.ver;
		close(fd);
	}
	wcfs_path_off(file, path);
	if (ver > 0)
		wcfs_path_mark(path);
	return 0;
}


void wcfs_path_check_dir(const char *path)
{
	struct stat st;
	const char *p = path;
	char name[WCFS_PATH_MAX];
	
	if (!path || ('\0' == *path))
		return;
		
	while (p && *p++ != '\0') {
		name[0] = '\0';
		p = strchr(p, '/');
		if (p) {
			strncat(name, path, p - path);
			if (stat(name, &st))
				mkdir(name, S_IRWXU | S_IRWXG | S_IRWXO);
		} 
	}
}


void wcfs_path_remove_mark(char *path)
{
	size_t len = strlen(path);
	
	if (len > WCFS_PATH_MARK_LEN)
		path[len - WCFS_PATH_MARK_LEN] = '\0';
}


int wcfs_path_is_marked(char *path)
{
	size_t len = strlen(path);
	
	return 0 == strncmp(path + len - WCFS_PATH_MARK_LEN, WCFS_PATH_MARK, WCFS_PATH_MARK_LEN);
}



