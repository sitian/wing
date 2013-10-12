#ifndef _DEBUG_H
#define _DEBUG_H

#include "dump.h"

#ifdef WDUMP_DEBUG
#define wdump_print_pos(desc) wdump_log("@%d", (int)wdump_desc_pos(desc))
#else
#define wdump_print_pos(desc) do {} while (0)
#endif

void wdump_print_regs(struct pt_regs *regs);
void wdump_print_task_regs(struct task_struct *tsk);
void wdump_print_stack(struct mm_struct *mm, struct pt_regs *regs, loff_t len);

unsigned long wdump_get_checksum(char *buf, size_t len);
unsigned long wdump_get_vma_checksum(struct mm_struct *mm, struct vm_area_struct *vma, 
										unsigned long addr, int force);
#endif
