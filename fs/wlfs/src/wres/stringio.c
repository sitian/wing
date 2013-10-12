/*      stringio.c
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

#include "stringio.h"

rbtree stringio_file_tree;
rbtree stringio_dir_tree;
pthread_rwlock_t stringio_file_lock;
pthread_rwlock_t stringio_dir_lock;

int stringio_stat = 0;
int stringio_mkdir(char *path);
int stringio_seek(stringio_file_t *filp, long int offset, int origin);
int stringio_write(const void *ptr, size_t size, size_t count, stringio_file_t *filp);

void stringio_init()
{
	if (stringio_stat != STRINGIO_STAT_INIT) {
		stringio_file_tree = rbtree_create();
		stringio_dir_tree = rbtree_create();
		pthread_rwlock_init(&stringio_file_lock, NULL);
		pthread_rwlock_init(&stringio_dir_lock, NULL);
		stringio_stat = STRINGIO_STAT_INIT;
		stringio_mkdir(STRINGIO_ROOT_PATH);
	}
}


static inline int stringio_compare(char *left, char *right)
{
	size_t l_len = strlen(left);
	size_t r_len = strlen(right);
	
	if (l_len < r_len)
		return -1;
	else if(l_len > r_len)
		return 1;
	else {
		int i;
		
		for (i = l_len - 1; i >= 0; i--) {
			if (left[i] == right[i])
				continue;
			else if (left[i] < right[i])
				return -1;
			else
				return 1;
		}
		return 0;
	}
}


int stringio_dirname(char *path, char *dir_name)
{
	int lst = strlen(path) - 1;
	
	if (lst < 0) {
		stringio_log("failed");
		return -1;
	}
	
	if (STRINGIO_SEPARATOR == path[lst])
		lst--;
	while ((lst >= 0) && (path[lst] != STRINGIO_SEPARATOR)) 
		lst--;
	if (lst < 0) {
		stringio_log("failed");
		return -1;
	}
	lst++;
	memcpy(dir_name, path, lst);
	dir_name[lst] = '\0';
	return 0;
}


static inline stringio_name_t * stringio_name_alloc(const char *path)
{
	stringio_name_t *name = (stringio_name_t *)malloc(sizeof(stringio_name_t));
	
	if (name)
		strncpy(name->name, path, STRINGIO_PATH_MAX);
	return name;
}


static inline stringio_file_t *stringio_file_alloc(const char *path)
{
	stringio_file_t *file = (stringio_file_t *)malloc(sizeof(stringio_file_t));
	
	if (file) {
		file->f_pos = 0;
		file->f_size = 0;
		file->f_buf = NULL;
		strncpy(file->f_path, path, STRINGIO_PATH_MAX);
	}
	return file;
}


static inline int stringio_calloc(stringio_file_t *filp, size_t size)
{
	if (filp && (size <= STRINGIO_BUF_SIZE)) {
		char *buf = NULL;
		
		if (size > 0) {
			buf = calloc(1, size);
			if (!buf)
				return -ENOMEM;
		}
		if (filp->f_buf)
			free(filp->f_buf);
		filp->f_pos = 0;
		filp->f_size = size;
		filp->f_buf = buf;
		return 0;
	}
	return -EINVAL;
}


static inline stringio_file_t *stringio_file_lookup(const char *path)
{
	stringio_file_t *file;
	void *key = (void *)path;

	pthread_rwlock_rdlock(&stringio_file_lock);
	file = rbtree_lookup(stringio_file_tree, (void *)key, stringio_compare);
	pthread_rwlock_unlock(&stringio_file_lock);
	return file;
}


static inline stringio_file_t *stringio_file_new(const char *path)
{
	stringio_file_t *file = stringio_file_alloc(path);
	
	if (!file)
		return NULL;
	
	pthread_rwlock_wrlock(&stringio_file_lock);
	rbtree_insert(stringio_file_tree, (void *)file->f_path, (void *)file, stringio_compare);
	pthread_rwlock_unlock(&stringio_file_lock);
	return file;
}


static inline void stringio_file_del(const char *path)
{
	void *key = (void *)path;

	pthread_rwlock_wrlock(&stringio_file_lock);
	rbtree_delete(stringio_file_tree, (void *)key, stringio_compare);
	pthread_rwlock_unlock(&stringio_file_lock);
}


static inline stringio_dir_t *stringio_dir_alloc(const char *path)
{
	stringio_dir_t *dir = (stringio_dir_t *)malloc(sizeof(stringio_dir_t));
	
	if (dir) {
		dir->d_count = 0;
		dir->d_tree = rbtree_create();
		strncpy(dir->d_path, path, STRINGIO_PATH_MAX);
		pthread_rwlock_init(&dir->d_lock, NULL);
	}
	return dir;
}


static inline stringio_dir_t *stringio_dir_lookup(const char *path)
{
	stringio_dir_t *dir;
	void *key = (void *)path;
	
	pthread_rwlock_rdlock(&stringio_dir_lock);
	dir = rbtree_lookup(stringio_dir_tree, (void *)key, stringio_compare);
	pthread_rwlock_unlock(&stringio_dir_lock);
	return dir;
}


static inline stringio_dir_t *stringio_dir_new(const char *path)
{
	stringio_dir_t *dir = stringio_dir_alloc(path);
	
	if (!dir)
		return NULL;
	
	pthread_rwlock_wrlock(&stringio_dir_lock);
	rbtree_insert(stringio_dir_tree, (void *)dir->d_path, (void *)dir, stringio_compare);
	pthread_rwlock_unlock(&stringio_dir_lock);
	return dir;
}


static inline void stringio_dir_del(const char *path)
{
	void *key = (void *)path;

	pthread_rwlock_wrlock(&stringio_dir_lock);
	rbtree_delete(stringio_dir_tree, (void *)key, stringio_compare);
	pthread_rwlock_unlock(&stringio_dir_lock);
}


static inline int stringio_add_to_dir(char *path)
{
	size_t len;
	char *file_name;
	char dir_name[STRINGIO_PATH_MAX];
	
	if (stringio_dirname(path, dir_name)) {
		stringio_log("failed, path=%s", path);
		return -1;
	}
	
	file_name = path + strlen(dir_name);
	len = strlen(file_name);
	if ((len > 0) && (len + file_name - path < STRINGIO_PATH_MAX)) {
		stringio_dir_t *dir;
		stringio_name_t *name = stringio_name_alloc(file_name);
		
		if (!name) {
			stringio_log("failed, path=%s", path);
			return -1;
		}
		dir = stringio_dir_lookup(dir_name);
		if (!dir) {
			stringio_log("failed, path=%s", path);
			free(name);
			return -1;
		}
		pthread_rwlock_wrlock(&dir->d_lock);
		rbtree_insert(dir->d_tree, (void *)name->name, (void *)name, stringio_compare);
		dir->d_count++;
		pthread_rwlock_unlock(&dir->d_lock);
	} else {
		stringio_log("failed, path=%s", path);
		return -1;
	}
	return 0;
}


static inline int stringio_remove_from_dir(char *path)
{
	size_t len;
	char *file_name;
	char dir_name[STRINGIO_PATH_MAX];
	
	if (stringio_dirname(path, dir_name)) {
		stringio_log("failed");
		return -1;
	}
	
	file_name = path + strlen(dir_name);
	len = strlen(file_name);
	if ((len > 0) && (len + file_name - path < STRINGIO_PATH_MAX)) {
		stringio_dir_t *dir = stringio_dir_lookup(dir_name);
		
		if (!dir) {
			stringio_log("failed");
			return -1;
		}
		pthread_rwlock_wrlock(&dir->d_lock);
		if (dir->d_count > 0) {
			rbtree_node node = rbtree_lookup_node(dir->d_tree, (void *)file_name, stringio_compare);
			if (node) {
				stringio_name_t *name = node->value;
				rbtree_delete_node(dir->d_tree, node);
				dir->d_count--;
				free(name);
			}
		}
		pthread_rwlock_unlock(&dir->d_lock);
	} else {
		stringio_log("failed");
		return -1;
	}
	return 0;
}


int stringio_size(stringio_file_t *filp)
{
	if (!filp)
		return -EINVAL;
	else
		return filp->f_size;
}


int stringio_truncate(stringio_file_t *filp, off_t length)
{
	if ((length < 0) || !filp)
		return -EINVAL;
	
	if (!length) {
		free(filp->f_buf);
		filp->f_buf = NULL;
	} else
		filp->f_buf = realloc(filp->f_buf, length);
	filp->f_size = length;
	return 0;
}


int stringio_write(const void *ptr, size_t size, size_t count, stringio_file_t *filp)
{
	size_t total;
	size_t buflen;
	
	if (!filp)
		return -EINVAL;
	
	total = size * count;
	if (!total)
		return 0;
	
	buflen = filp->f_pos + total;
	if (buflen > filp->f_size) {
		filp->f_buf = realloc(filp->f_buf, buflen);
		if (!filp->f_buf) {
			filp->f_size = 0;
			filp->f_pos = 0;
			return 0;
		}
		filp->f_size = buflen;
	}
	memcpy(filp->f_buf + filp->f_pos, (char *)ptr, total);
	filp->f_pos += total;
	return count;
}


int stringio_read(void *ptr, size_t size, size_t count, stringio_file_t *filp)
{
	size_t total;
	
	if (!filp)
		return -EINVAL;
	
	total = size * count;
	if (!total)
		return 0;
	
	if (filp->f_pos + total > filp->f_size)
		total = filp->f_size - filp->f_pos;
	if (!total)
		return 0;
	total -= total % size;
	memcpy((char *)ptr, filp->f_buf + filp->f_pos, total);
	filp->f_pos += total;
	return total / size;
}


int stringio_seek(stringio_file_t *filp, long int offset, int origin)
{
	long int pos;
	
	if ((offset < 0) || !filp)
		return -EINVAL;
	switch (origin) {
	case SEEK_SET:
		pos = offset;
		break;
	case SEEK_END:
		pos = filp->f_size + offset;
		break;
	case SEEK_CUR:
		pos = filp->f_pos + offset;
		break;
	default:
		return -EINVAL;
	}	
	if ((pos > filp->f_size) || (pos < 0))
		return -EINVAL;
	filp->f_pos = pos;
	return 0;
}


stringio_file_t *stringio_open(char *path, const char *mode)
{
	stringio_file_t *file;
	size_t len = strlen(path);
	
	if (stringio_stat != STRINGIO_STAT_INIT) {
		stringio_log("stringio is uninitialized");
		return NULL;
	}
	
	if ((len >= STRINGIO_PATH_MAX) || (STRINGIO_SEPARATOR == path[len - 1])) {
		stringio_log("invalid path");
		return NULL;
	}
	
	file = stringio_file_lookup(path);
	if (!strcmp(mode, "w")) {
		if (!file) {
			if (!stringio_add_to_dir(path)) {
				file = stringio_file_new(path);
				if (!file) {
					stringio_log("failed, path=%s", path);
					stringio_remove_from_dir(path);
					return NULL;
				}
			}
		}
		if (file && file->f_buf) {
			free(file->f_buf);
			file->f_buf = NULL;
			file->f_size = 0;
		}
	}
	if (file)
		file->f_pos = 0;
	return file;
}


int stringio_access(char *path, int mode)
{
	size_t len = strlen(path);
	
	if (stringio_stat != STRINGIO_STAT_INIT) {
		stringio_log("stringio is uninitialized");
		return -1;
	}
	
	if (!len || (len >= STRINGIO_PATH_MAX)) {
		stringio_log("invalid path");
		return -1;
	}
	
	if (F_OK == mode) {
		if (stringio_file_lookup(path))
			return 0;
	}
	return -1;
}


int stringio_isdir(char *path)
{
	size_t len = strlen(path);
	
	if (stringio_stat != STRINGIO_STAT_INIT) {
		stringio_log("stringio is uninitialized");
		return -1;
	}
	
	if (!len || (len >= STRINGIO_PATH_MAX)) {
		stringio_log("invalid path");
		return 0;
	}
	
	if (path[len - 1] == STRINGIO_SEPARATOR)
		return stringio_dir_lookup(path) != NULL;
	else {
		char dir_name[STRINGIO_PATH_MAX] = {0};
		
		if (STRINGIO_PATH_MAX - 1 == len) {
			stringio_log("invalid path");
			return 0;
		}
		memcpy(dir_name, path, len);
		dir_name[len] = STRINGIO_SEPARATOR;
		return stringio_dir_lookup(dir_name) != NULL;
	}
}


int stringio_mkdir(char *path)
{
	char *d_path;
	char dir_name[STRINGIO_PATH_MAX] = {0};
	size_t len = strlen(path);
	
	if (stringio_stat != STRINGIO_STAT_INIT) {
		stringio_log("stringio is uninitialized");
		return -1;
	}
	
	if (!len || (len >= STRINGIO_PATH_MAX)) {
		stringio_log("invalid path");
		return -1;
	}
	
	if (path[len - 1] == STRINGIO_SEPARATOR)
		d_path = path;
	else {
		if (STRINGIO_PATH_MAX - 1 == len) {
			stringio_log("invalid path");
			return -1;
		}
		memcpy(dir_name, path, len);
		dir_name[len] = STRINGIO_SEPARATOR;
		d_path = dir_name;
	}

	if (strncmp(path, STRINGIO_ROOT_PATH, STRINGIO_PATH_MAX)) {
		if (stringio_add_to_dir(d_path)) {
			stringio_log("failed, path=%s", d_path);
			return -1;
		}
	}
	
	if (NULL == stringio_dir_new(d_path)) {
		stringio_log("failed, path=%s", d_path);
		if (strncmp(path, STRINGIO_ROOT_PATH, STRINGIO_PATH_MAX))
			stringio_remove_from_dir(d_path);
		return -1;
	}
	return 0;
}


int stringio_tree_iter(char *path, rbtree_node node, stringio_callback func) 
{
	int ret;
	char name[STRINGIO_PATH_MAX];
	
    if (!node)
        return 0;
	
    if (node->right) {
        ret = stringio_tree_iter(path, node->right, func);
		if (ret)
			return ret;
	}
	strcpy(name, path);
	strcat(name, (char *)node->key);
	ret = func(name);
    if (ret)
		return ret;
    if (node->left) {
        ret = stringio_tree_iter(path, node->left, func);
		if (ret)
			return ret;
	}
	return 0;
}


int stringio_dir_iter(char *path, stringio_callback func)
{
	size_t len;
	char *d_path;
	char dir_name[STRINGIO_PATH_MAX] = {0};
	stringio_dir_t *dir;
	
	if (stringio_stat != STRINGIO_STAT_INIT) {
		stringio_log("stringio is uninitialized");
		return 0;
	}
	
	len = strlen(path);
	if (!len || (len >= STRINGIO_PATH_MAX)) {
		stringio_log("invalid path");
		return 0;
	}
	
	if (path[len - 1] == STRINGIO_SEPARATOR)
		d_path = path;
	else {
		if (STRINGIO_PATH_MAX - 1 == len) {
			stringio_log("invalid path");
			return 0;
		}
		memcpy(dir_name, path, len);
		dir_name[len] = STRINGIO_SEPARATOR;
		d_path = dir_name;
	}
	dir = stringio_dir_lookup(d_path);
	if (dir)
		return stringio_tree_iter(d_path, dir->d_tree->root, func);
	return 0;
}


int stringio_print_name(char *name)
{
	printf("%s ", name);
	return 0;
}


void stringio_lsdir(char *path)
{
	printf("<%s>:", path);
	stringio_dir_iter(path, stringio_print_name);
	printf("\n");
}


int stringio_dir_clear(stringio_dir_t *dir)
{
	char *name;
	size_t len;
	int ret = 0;
	rbtree_node node;
	
	pthread_rwlock_wrlock(&dir->d_lock);
	while (dir->d_count > 0) {
		node = dir->d_tree->root;
		if (!node) {
			stringio_log("no root");
			ret = -1;
			goto out;
		}
		name = (char *)node->key;
		len = strlen(name);
		if (!len || (len >= STRINGIO_PATH_MAX)) {
			stringio_log("invalid name");
			ret = -1;
			goto out;
		}
		if (name[len - 1] == STRINGIO_SEPARATOR) {
			char child[STRINGIO_PATH_MAX];
			strcpy(child, dir->d_path);
			strcat(child, name);
			pthread_rwlock_unlock(&dir->d_lock);
			if (stringio_rmdir(child)) {
				stringio_log("failed to remove child dir");
				return -1;
			}
			pthread_rwlock_wrlock(&dir->d_lock);
		} else {
			stringio_name_t *ptr = node->value;
			rbtree_delete_node(dir->d_tree, node);
			free(ptr);
			dir->d_count--;
		}
	}
out:
	pthread_rwlock_unlock(&dir->d_lock);
	return ret;
}


int stringio_is_empty_dir(char *path)
{
	size_t len;
	char *d_path;
	char dir_name[STRINGIO_PATH_MAX] = {0};
	stringio_dir_t *dir;
	
	if (stringio_stat != STRINGIO_STAT_INIT) {
		stringio_log("stringio is uninitialized");
		goto out;
	}
	
	len = strlen(path);
	if (!len || (len >= STRINGIO_PATH_MAX)) {
		stringio_log("invalid path");
		goto out;
	}
	
	if (path[len - 1] == STRINGIO_SEPARATOR)
		d_path = path;
	else {
		if (STRINGIO_PATH_MAX - 1 == len) {
			stringio_log("invalid path");
			goto out;
		}
		memcpy(dir_name, path, len);
		dir_name[len] = STRINGIO_SEPARATOR;
		d_path = dir_name;
	}

	dir = stringio_dir_lookup(d_path);
	if (dir && dir->d_count > 0)
		return 0;
out:
	return 1;
}


int stringio_rmdir(char *path)
{
	size_t len;
	char *d_path;
	char dir_name[STRINGIO_PATH_MAX] = {0};
	stringio_dir_t *dir;
	
	if (stringio_stat != STRINGIO_STAT_INIT) {
		stringio_log("stringio is uninitialized");
		return -1;
	}
	
	len = strlen(path);
	if (!len || (len >= STRINGIO_PATH_MAX)) {
		stringio_log("invalid path");
		return -1;
	}
	if (path[len - 1] == STRINGIO_SEPARATOR)
		d_path = path;
	else {
		if (STRINGIO_PATH_MAX - 1 == len) {
			stringio_log("invalid path");
			return -1;
		}
		memcpy(dir_name, path, len);
		dir_name[len] = STRINGIO_SEPARATOR;
		d_path = dir_name;
	}
	dir = stringio_dir_lookup(d_path);
	if (!dir) {
		stringio_log("no entry");
		return -1;
	}
	if (stringio_dir_clear(dir)) {
		stringio_log("failed to clear");
		return -1;
	}
	if (stringio_remove_from_dir(d_path)) {
		stringio_log("failed to locate");
		return -1;
	}
	stringio_dir_del(d_path);
	return 0;
}


int stringio_remove(char *path)
{
	size_t len;
	
	if (stringio_stat != STRINGIO_STAT_INIT) {
		stringio_log("stringio is uninitialized");
		return -1;
	}
	len = strlen(path);
	if (!len || len >= STRINGIO_PATH_MAX) {
		stringio_log("invalid path");
		return -1;
	}
	if (STRINGIO_SEPARATOR == path[len - 1]) {
		stringio_log("cannot remove a dir");
		return -1;
	} else {
		if (stringio_remove_from_dir(path)) {
			stringio_log("cannot locate file %s", path);
			return -1;
		}
		stringio_file_del(path);
	}
	return 0;
}


void *stringio_mmap(void *start, size_t length, int prot, int flags, stringio_file_t *filp, off_t offset)
{
	if (!filp || offset + length > filp->f_size)
		return NULL;
	return filp->f_buf + offset;
}


int stringio_munmap(void *start, size_t length)
{
	return 0;
}


int stringio_check_path(char *path)
{
	int ret = 0;
	char dir[STRINGIO_PATH_MAX];
	
	stringio_dirname(path, dir);
	if (!stringio_isdir(dir)) {
		if (!stringio_check_path(dir))
			ret = stringio_mkdir(dir);
	}
	return ret;
}


stringio_entry_t *stringio_get_entry(char *path, size_t size, int flags)
{
	int prot;
	stringio_file_t *filp = NULL;
	stringio_entry_t *entry;
	
	entry = (stringio_entry_t *)malloc(sizeof(stringio_entry_t));
	if (!entry)
		return NULL;
	
	if (flags & STRINGIO_RDWR) {
		filp = stringio_open(path, "r+");
		prot = PROT_READ | PROT_WRITE;
	} else if (flags & STRINGIO_RDONLY) {
		filp = stringio_open(path, "r");
		prot = PROT_READ;
	} else
		goto bad;
	
	if (!filp) {
		if (flags & STRINGIO_CREAT) {
			filp = stringio_open(path, "w");
#ifdef STRINGIO_CHECK_PATH
			if (!filp) {
				if (!stringio_check_path(path))
					filp = stringio_open(path, "w");
			}
#endif
			if (stringio_calloc(filp, size))
				goto bad;
		}
		if (!filp)
			goto bad;
	}
	
	if (size > filp->f_size) {
		if (prot & PROT_WRITE) {
			filp->f_buf = realloc(filp->f_buf, size);
			if (filp->f_buf) {
				bzero(&filp->f_buf[filp->f_size], size - filp->f_size);
				filp->f_size = size;
			} else {
				filp->f_size = 0;
				filp->f_pos = 0;
			}
		} else
			goto bad;
	}
	
	if (flags & STRINGIO_AUTO_INCREMENT)
		size = filp->f_size;
	
	if (!filp->f_buf && (size > 0))
		goto bad;
	
	entry->file = filp;
	entry->size = size;
	entry->desc = &filp->f_buf;
	return entry;
bad:
	free(entry);
	return NULL;
}


void stringio_put_entry(stringio_entry_t *entry)
{
	if (entry) {
		stringio_munmap(entry->desc, entry->size);
		stringio_close(entry->file);
		free(entry);
	}
}


int stringio_append(stringio_entry_t *entry, void *buf, size_t size)
{
	stringio_file_t *filp = entry->file;
	
	stringio_seek(filp, 0, SEEK_END);
	if (stringio_write(buf, 1, size, filp) != size)
		return -EIO;
	return 0;
}


void stringio_close(stringio_file_t *filp)
{
}
