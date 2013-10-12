#ifndef _SYS_H
#define _SYS_H

#include "config.h"
#include "dump.h"

extern int *compat_suid_dumpable;
int lseek(unsigned int fd, int offset, unsigned int origin);
int compat_sigaltstack(struct task_struct *tsk, const stack_t *ss, stack_t *oss, unsigned long sp);

#endif
