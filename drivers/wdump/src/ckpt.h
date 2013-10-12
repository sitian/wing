#ifndef _CKPT_H
#define _CKPT_H

#include "dump.h"
#include "util.h"
#include "save.h"
#include "restore.h"

#define WDUMP_CKPT_PATH			"/var/wdump/"
#define WDUMP_CKPT_SUFFIX		".ck"
#define WDUMP_CKPT_PATH_MAX		32

typedef struct wdump_ckpt_header {
	char signature[4];
	int major;
	int minor;
} wdump_ckpt_header_t;

int wdump_ckpt_save(struct task_struct *tsk);
int wdump_ckpt_restore(pid_t pid);

#endif
