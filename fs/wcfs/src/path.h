#ifndef _PATH_H
#define _PATH_H

#include <sys/stat.h>
#include "file.h"

#define WCFS_PATH_MAX			32
#define WCFS_PATH_MARK			"@"
#define WCFS_PATH_HOME			"/var/cache/wing/wcfs"
#define WCFS_PATH_MARK_LEN		sizeof(WCFS_PATH_MARK)

void wcfs_path_mark(char *path);
void wcfs_path_remove_mark(char *path);
void wcfs_path_check_dir(const char *path);
void wcfs_path_off(wcfs_file_t *file, char *path);
void wcfs_path_area(wcfs_file_t *file, char *path);
void wcfs_path_header(wcfs_file_t *file, char *path);
void wcfs_path_append(char *path, unsigned long val);

int wcfs_path_is_marked(char *path);
int wcfs_path_get(wcfs_file_t *file, char *path);

#endif
