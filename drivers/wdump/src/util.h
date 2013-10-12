#ifndef _UTIL_H
#define _UTIL_H

#include "dump.h"
#include "io.h"
#include "sys.h"

#define inside(a, b, c) ((c <= b) && (c >= a))
#define wdump_get_i387(tsk) (tsk)->thread.fpu.state
#define wdump_can_conn(flags) (flags & VM_GROWSDOWN ? 0 : 1)
#define wdump_is_arch_vma(vma) ((vma)->vm_start == (long)(vma)->vm_mm->context.vdso)

typedef union thread_xstate wdump_i387_t;

int wdump_set_cwd(char *name);
int wdump_set_root(char *name);
int wdump_check_fpu_state(void);
int wdump_get_file_type(struct file *file);
int wdump_can_dump(struct vm_area_struct *vma);
int wdump_map_area(char *path, wdump_vma_t *area);
int wdump_remap_area(unsigned long from, wdump_vma_t *area);
int wdump_get_cwd(struct task_struct *tsk, char *buf, unsigned long size);
int wdump_get_root(struct task_struct *tsk, char *buf, unsigned long size);

struct file *wdump_reopen_file(wdump_file_t *file, const char *path);

void *wdump_check_addr(struct mm_struct *mm, unsigned long address);
void *wdump_get_page(struct mm_struct *mm, struct vm_area_struct *vma, unsigned long address);

unsigned long wdump_get_map_prot(wdump_vma_t *area);
unsigned long wdump_get_map_flags(wdump_vma_t *area);

#endif
