//      debug.c
//      
//      Copyright 2013 Yi-Wei Ci <ciyiwei@hotmail.com>
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

#include "debug.h"
#include "util.h"
#include "io.h"

void wdump_print_regs(struct pt_regs *regs)
{
	if (!regs)
		return;
		
	printk("pt_regs: ax=%08lx bx=%08lx cx=%08lx dx=%08lx\n", 
				regs->ax, regs->bx, regs->cx, regs->dx);
	printk("pt_regs: si=%08lx di=%08lx sp=%08lx bp=%08lx\n",
				regs->si, regs->di, regs->sp, regs->bp);
	printk("pt_regs: ip=%08lx fl=%08lx cs=%08lx ss=%08lx\n",
				regs->ip, regs->flags, regs->cs, regs->ss);
	printk("pt_regs: ds=%08lx es=%08lx fs=%08lx gs=%08lx\n",
				regs->ds, regs->es, regs->fs, regs->gs);
	printk("pt_regs: orig_ax=%08lx\n", regs->orig_ax);
}


void wdump_print_task_regs(struct task_struct *tsk)
{
	struct pt_regs *regs = task_pt_regs(tsk);
	
	wdump_print_regs(regs);
}


void wdump_print_stack(struct mm_struct *mm, struct pt_regs *regs, loff_t len)
{
	int i, j;
	
	len = len >> 2 << 2;
	for (i = 0; i < len; i += 4) {
		u32 *ptr = (u32 *)wdump_check_addr(mm, regs->sp + i);
		printk("sp [%08lx-%08lx]:", regs->sp + i, regs->sp + i + 3);
		for (j = 0; j < 4; j++)
			printk(" %08lx", (unsigned long)ptr[j]);
		printk("\n");
	}
}


static inline unsigned long wdump_update_checksum(unsigned long old, unsigned long new)
{
	return ((old + new) & 0xff);
}


unsigned long wdump_get_checksum(char *buf, size_t len)
{
	int i;
	unsigned long ret = 0; 
	
	for (i = 0; i < len; i++)
		ret = wdump_update_checksum(ret, buf[i]);
	return ret;
}


static int wdump_read_file(struct path *path, loff_t off, char *buf, size_t len)
{
	int ret = 0;
	char *name;
	char *p = buf;
	char *path_buf;
	struct file *file;
	mm_segment_t fs = get_fs();
	
	memset(buf, 0, len);
	path_buf = (char *)kmalloc(WDUMP_PATH_MAX, GFP_KERNEL);
	if (!path_buf) {
		wdump_log("no memory");
		return -ENOMEM;
	}
	name = d_path(path, path_buf, WDUMP_PATH_MAX);
	if (IS_ERR(name)) {
		wdump_log("invalid path");
		ret = PTR_ERR(name);
		goto out;
	}
	file = filp_open(name, O_RDONLY | O_LARGEFILE, 0);
	if (IS_ERR(file)) {
		wdump_log("failed to open file");
		ret = PTR_ERR(file);
		goto out;
	}
	set_fs(KERNEL_DS);
	while (len > 0) {
		int n = vfs_read(file, p, len, &off);
		if (n <= 0) {
			if (n < 0) {
				wdump_log("failed to read");
				ret = -EIO;
			}
			break;
		}
		len -= n;
		p += n;
	}
	set_fs(fs);
	filp_close(file, NULL);
out:
	kfree(path_buf);
	return ret;
}


unsigned long wdump_get_vma_checksum(struct mm_struct *mm, struct vm_area_struct *vma, 
										unsigned long addr, int force)
{
	struct file *file;
	unsigned long ret = 0;
	
	if (!vma) {
		vma = find_vma(mm, addr);
		if (!vma) {
			wdump_log("cannot find vma");
			return 0;
		}
	}
	if (addr < vma->vm_start) {
		wdump_log("invalid address");
		return 0;
	}
	file = vma->vm_file;
	while (addr < vma->vm_end) {
		char *buf = wdump_get_page(mm, vma, addr);
		
		if (!buf) {
			if (force) {
				buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
				if (!buf) {
					wdump_log("no memory");
					return 0;
				}
				if (copy_from_user(buf, (void *)addr, PAGE_SIZE)) {
					wdump_log("failed to copy");
					kfree(buf);
					return 0;
				}
				ret = wdump_update_checksum(ret, wdump_get_checksum(buf, PAGE_SIZE));
				kfree(buf);
			} else if (file) {
				loff_t off = addr - vma->vm_start + (vma->vm_pgoff << PAGE_SHIFT);
				
				buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
				if (!buf) {
					wdump_log("no memory");
					return 0;
				}
				if (wdump_read_file(&file->f_path, off, buf, PAGE_SIZE)) {
					wdump_log("failed to read file");
					kfree(buf);
					return 0;
				}
				ret = wdump_update_checksum(ret, wdump_get_checksum(buf, PAGE_SIZE));
				kfree(buf);
			}
		} else if (buf)
			ret = wdump_update_checksum(ret, wdump_get_checksum(buf, PAGE_SIZE));
		addr += PAGE_SIZE;
	}
	return ret;
}
