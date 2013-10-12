//      save.c
//      
//      Copyright (C) 2013 Yi-Wei Ci <ciyiwei@hotmail.com>
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
//
//		This work originally started from Berkeley Lab Checkpoint/Restart (BLCR):
//		https://ftg.lbl.gov/projects/CheckpointRestart/

#include "save.h"

int wdump_save_pages(wdump_desc_t desc, struct task_struct *tsk, struct vm_area_struct *vma)
{
	int ret = 0;
	int count = 0;
	unsigned long addr;
	struct mm_struct *mm = tsk->mm;
	int conn = !vma->vm_file && wdump_can_conn(vma->vm_flags);
	
	for (addr = vma->vm_start; addr < vma->vm_end; addr += PAGE_SIZE) {
		char *buf = wdump_get_page(mm, vma, addr);
		
		if (!buf)
			continue;
			
		if (conn) {
			ret = wdump_conn_update(tsk->gpid, vma->vm_start, addr, buf, PAGE_SIZE);
			if (ret) {
				wdump_log("failed to connect");
				goto out;
			}
		} else {
			if (wdump_write(desc, &addr, sizeof(addr)) != sizeof(addr)) {
				wdump_log("failed to save page");
				ret = -EIO;
				goto out;
			}
			if (wdump_write(desc, buf, PAGE_SIZE) != PAGE_SIZE) {
				wdump_log("failed to save page");
				ret = -EIO;
				goto out;
			}
		}
		count++;
	}
	
	if (conn) {
		size_t size = vma->vm_end - vma->vm_start;
		ret = wdump_conn_set(tsk->gpid, vma->vm_start, WDUMP_CONN_SIZE, &size, sizeof(size));
	} else {
		addr = 0;
		if (wdump_write(desc, &addr, sizeof(addr)) != sizeof(addr)) {
			wdump_log("failed to save page");
			ret = -EIO;
		}
	}
	if (!ret)
		ret = count;
out:
	return ret;
}


int wdump_save_vma(wdump_desc_t desc, struct task_struct *tsk, struct vm_area_struct *vma) 
{
	int ret = 0;
	int nr_pages = 0;
	wdump_vma_t area;
	char *buf = NULL;
	char *name = NULL;
	struct file *file = vma->vm_file;
	
	if (!wdump_can_dump(vma))
		return 0;
	
	area.start	= vma->vm_start;
	area.end	= vma->vm_end;
	area.flags	= vma->vm_flags;
	area.pgoff	= vma->vm_pgoff;
	area.arch	= wdump_is_arch_vma(vma);
	area.namelen= 0;
	
	if (!file) {
		if (wdump_write(desc, &area, sizeof(wdump_vma_t)) != sizeof(wdump_vma_t)) {
			wdump_log("failed to save vma");
			return -EIO;
		}
		if (!area.arch) {
			nr_pages = wdump_save_pages(desc, tsk, vma);
			if (nr_pages < 0) {
				wdump_log("failed to save pages");
				return nr_pages;
			}
		}
	} else {
		buf = kmalloc(WDUMP_PATH_MAX, GFP_KERNEL);
		if (!buf) {
			wdump_log("no memory");
			return -ENOMEM;
		}
		name = d_path(&file->f_path, buf, WDUMP_PATH_MAX);
		if (IS_ERR(name)) {
			wdump_log("invalid path");
			ret = PTR_ERR(name);
			goto out;
		}
		area.namelen = strlen(name) + 1;	 
		if (wdump_write(desc, &area, sizeof(wdump_vma_t)) != sizeof(wdump_vma_t)) {
			wdump_log("failed to save vma");
			ret = -EIO;
			goto out;
		}
		if (wdump_write(desc, name, area.namelen) != area.namelen) {
			wdump_log("failed to save path");
			ret = -EIO;
			goto out;
		}
		nr_pages = wdump_save_pages(desc, tsk, vma);
		if (nr_pages < 0) {
			wdump_log("failed to save pages");
			ret = nr_pages;
			goto out;
		}
	}
	wdump_print_save_vma(vma->vm_start, vma->vm_end, name, nr_pages,
							wdump_get_vma_checksum(tsk->mm, vma, vma->vm_start, 0));
out:
	if (buf)
		kfree(buf);
	return ret;
}  


int wdump_save_mm(wdump_desc_t desc, struct task_struct *tsk)
{
	int ret = 0;
	struct mm_struct *mm = tsk->mm;
	struct vm_area_struct *vma;
	wdump_mm_t memory;
	
	wdump_print_save_mm("saving mm ...");
	down_read(&mm->mmap_sem);
	memory.start_code	= mm->start_code;
	memory.end_code		= mm->end_code;
	memory.start_data	= mm->start_data;
	memory.end_data		= mm->end_data;
	memory.start_brk	= mm->start_brk;
	memory.brk			= mm->brk;
	memory.start_stack	= mm->start_stack;
	memory.arg_start	= mm->arg_start;
	memory.arg_end		= mm->arg_end;
	memory.env_start	= mm->env_start;
	memory.env_end		= mm->env_end;
	memory.mmap_base 	= mm->mmap_base;
	memory.map_count	= mm->map_count;
	
	if (wdump_write(desc, &memory, sizeof(wdump_mm_t)) != sizeof(wdump_mm_t)) {
		wdump_log("failed to save memory");
		ret = -EIO;
		goto out;
	}
	
	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		ret = wdump_save_vma(desc, tsk, vma);
		if (ret) {
			wdump_log("failed to save vma");
			goto out;
		}
	}
	wdump_print_pos(desc);
out:
	up_read(&mm->mmap_sem);
	return ret;
}


int wdump_save_fpu(wdump_desc_t desc, struct task_struct *tsk)
{
	int ret = 0;
	int flag = !!(tsk->flags & PF_USED_MATH);
	
	wdump_print_save_fpu("saving fpu ...");
	if (wdump_write(desc, &flag, sizeof(int)) != sizeof(int)) {
		wdump_log("failed to save fpu");
		return -EIO;
	}
	if (flag) {
		kernel_fpu_begin();
		if (wdump_write(desc, wdump_get_i387(tsk), xstate_size) != xstate_size) {
			wdump_log("failed to save fpu");
			ret = -EIO;
			goto out;
		}
		kernel_fpu_end();
	}
	wdump_print_pos(desc);
out:
	return ret;
}


int wdump_save_cpu(wdump_desc_t desc, struct task_struct *tsk)
{		
	int i;
	int ret = 0;
	wdump_cpu_t *cpu;
	struct pt_regs *pt;
	struct pt_regs *regs;
	struct thread_struct *thread = &tsk->thread;
	
	cpu = kmalloc(sizeof(wdump_cpu_t), GFP_KERNEL);
	if (!cpu) {
		wdump_log("no memory");
		return -ENOMEM;
	}
	
	wdump_print_save_cpu("saving regs ...");
	pt = task_pt_regs(tsk);
	regs = &cpu->regs;
	memcpy(regs, pt, sizeof(struct pt_regs));
	regs->cs &= 0xffff;
	regs->ds &= 0xffff;
	regs->es &= 0xffff;
	regs->ss &= 0xffff;
	regs->fs &= 0xffff;
	regs->gs &= 0xffff;
	wdump_print_regs(regs);
	wdump_print_save_cpu("saving debugregs ...");
	for (i = 0; i < HBP_NUM; i++) {
	    struct perf_event *bp = thread->ptrace_bps[i];
	    cpu->debugregs[i] = bp ? bp->hw.info.address : 0;
	}
	cpu->debugregs[HBP_NUM] = thread->debugreg6;
	cpu->debugregs[HBP_NUM + 1] = thread->ptrace_dr7;
	
	wdump_print_save_cpu("saving tls ...");
	for (i = 0; i < GDT_ENTRY_TLS_ENTRIES; i++)
		cpu->tls_array[i] = thread->tls_array[i];

	cpu->sysenter_return = task_thread_info(tsk)->sysenter_return;
	if (wdump_write(desc, cpu, sizeof(wdump_cpu_t)) != sizeof(wdump_cpu_t)) {
		wdump_log("failed to save cpu");
		ret = -EIO;
	}
	kfree(cpu);
	wdump_print_pos(desc);
	return ret;
}


int wdump_save_file(wdump_desc_t desc, int fd, struct file *filp)
{
	char *buf;
	char *name;
	int ret = 0;
	int namelen;
	wdump_file_t file;
	struct dentry *dentry = filp->f_dentry;
	struct inode *inode = dentry->d_inode;
	
	buf = (char *)kmalloc(WDUMP_PATH_MAX, GFP_KERNEL);
	if (!buf) {
		wdump_log("no memory");
		return -ENOMEM;
	}
	name = d_path(&filp->f_path, buf, WDUMP_PATH_MAX);
	if (IS_ERR(name)) {
		wdump_log("invalid path");
		ret = PTR_ERR(name);
		goto out;
	}
	namelen = strlen(name) + 1;
	file.namelen = namelen;
	file.pos = filp->f_pos;
	file.flags = filp->f_flags;
	file.mode = inode->i_mode;
	file.fd = fd;
	
	if (wdump_write(desc, &file, sizeof(wdump_file_t)) != sizeof(wdump_file_t)) {
		wdump_log("failed to save file");
		ret = -EIO;
		goto out;
	}
	if (wdump_write(desc, name, namelen) != namelen) {
		wdump_log("failed to save path");
		ret = -EIO;
		goto out;
	}
	wdump_print_save_file("%s", name);
out:
	kfree(buf);
	return ret;
}


int wdump_save_files(wdump_desc_t desc, struct task_struct *tsk)
{
	int fd;
	int type;
	int ret = 0;
	struct fdtable *fdt;
	struct files_struct *files = tsk->files;
	wdump_files_struct_t fs;
	struct file *filp;
	
	wdump_print_save_files("saving files ...");
	rcu_read_lock();
	fdt = files_fdtable(files);
	fs.max_fds = fdt->max_fds;
	fs.next_fd = files->next_fd;
	
	if (wdump_write(desc, &fs, sizeof(wdump_files_struct_t)) != sizeof(wdump_files_struct_t)) {
		wdump_log("failed to save files_struct");
		return -EIO;
	}
	
	for (fd = 0; fd < fdt->max_fds; fd++) {
		filp = fcheck_files(files, fd);
		if (!filp)
			continue;
			
		get_file(filp);
		type = wdump_get_file_type(filp);
		if (wdump_write(desc, &type, sizeof(int)) != sizeof(int)) {
			wdump_log("failed to save type");
			ret = -EIO;
			goto out;
		}
		switch(type) {
		case S_IFREG:
		case S_IFDIR:
			ret = wdump_save_file(desc, fd, filp);
			break;
		default:
			break;
		}
		fput(filp);
		if (ret) {
			wdump_log("failed to save file");
			goto out;
		}
	}
	type = 0;
	if (wdump_write(desc, &type, sizeof(int)) != sizeof(int)) {
		wdump_log("failed to finalize");
		ret = -EIO;
		goto out;
	}
	wdump_print_pos(desc);
out:
	rcu_read_unlock();
	return ret;
}


int wdump_save_sigpending(wdump_desc_t desc, struct task_struct *tsk, struct sigpending *sigpending)
{
	int ret = 0;
	int count = 0;
	struct list_head *entry;
	wdump_sigpending_t pending;
	
	spin_lock_irq(&tsk->sighand->siglock);
	list_for_each(entry, &sigpending->list)
		count++;
	memcpy(&pending.signal, &sigpending->signal, sizeof(sigset_t));
	pending.count = count;
	spin_unlock_irq(&tsk->sighand->siglock);
	if (wdump_write(desc, &pending, sizeof(wdump_sigpending_t)) != sizeof(wdump_sigpending_t)) {
		wdump_log("failed to save pending");
		ret = -EIO;
	}
	spin_lock_irq(&tsk->sighand->siglock);
	list_for_each(entry, &sigpending->list) {
		struct siginfo info;
		wdump_sigqueue_t queue;
		mm_segment_t fs = get_fs();
		struct sigqueue *sq = list_entry(entry, struct sigqueue, list);
		
		if (!count) {
			spin_lock_irq(&tsk->sighand->siglock);
			wdump_log("out of range");
			return -EINVAL;
		}
		memset(&queue, 0, sizeof(wdump_sigqueue_t));
		queue.flags = sq->flags;
		info = sq->info;
		spin_unlock_irq(&tsk->sighand->siglock);
		
		set_fs(KERNEL_DS);
		ret = copy_siginfo_to_user(&queue.info, &info);
		set_fs(fs);
		if (ret) {
			wdump_log("failed to copy siginfo");
			return ret;
		}
		if (wdump_write(desc, &queue, sizeof(wdump_sigqueue_t)) != sizeof(wdump_sigqueue_t)) {
			wdump_log("failed to save sigqueue");
			return -EIO;
		}
		count--;
		spin_lock_irq(&tsk->sighand->siglock);
	}
	spin_unlock_irq(&tsk->sighand->siglock);
	return ret;
}


int wdump_save_signals(wdump_desc_t desc, struct task_struct *tsk)
{
	int i;
	int ret;
	stack_t sigstack;
	sigset_t sigblocked;
	struct k_sigaction action;
	
	wdump_print_save_signals("saving sigstack ...");
	ret = compat_sigaltstack(tsk, NULL, &sigstack, 0);
	if (ret) {
		wdump_log("failed to get sigstack (ret=%d)", ret);
		return ret;
	}
	if (wdump_write(desc, &sigstack, sizeof(stack_t)) != sizeof(stack_t)) {
		wdump_log("failed to save sigstack");
		return -EIO;
	}
	
	wdump_print_save_signals("saving sigblocked ...");
	spin_lock_irq(&tsk->sighand->siglock);
	memcpy(&sigblocked, &tsk->blocked, sizeof(sigset_t));
	spin_unlock_irq(&tsk->sighand->siglock);
	if (wdump_write(desc, &sigblocked, sizeof(sigset_t)) != sizeof(sigset_t)) {
		wdump_log("failed to save sigblocked");
		return -EIO;
	}
	
	wdump_print_save_signals("saving pending ...");
	ret = wdump_save_sigpending(desc, tsk, &tsk->pending);
	if (ret) {
		wdump_log("failed to save pending");
		return ret;
	}
	
	wdump_print_save_signals("saving shared_pending ...");
	ret = wdump_save_sigpending(desc, tsk, &tsk->signal->shared_pending);
	if (ret) {
		wdump_log("failed to save shared_pending");
		return ret;
	}
	
	wdump_print_save_signals("saving sigaction ...");
	for (i = 0; i < _NSIG; i++) {
		spin_lock_irq(&tsk->sighand->siglock);
		memcpy(&action, &tsk->sighand->action[i], sizeof(struct k_sigaction));
		spin_unlock_irq(&tsk->sighand->siglock);
		if (wdump_write(desc, &action, sizeof(struct k_sigaction)) != sizeof(struct k_sigaction)) {
			wdump_log("failed to save sigaction");
			return -EIO;
		}
	}
	wdump_print_pos(desc);
	return 0;
}


int wdump_save_cred(wdump_desc_t desc, struct task_struct *tsk)
{
	int ret = 0;
	wdump_cred_t cred;
	const struct cred *tsk_cred = tsk->cred;
	struct group_info *gi = tsk_cred->group_info;
	
	wdump_print_save_cred("saving cred ...");
	cred.gpid	= tsk->gpid;
	cred.uid	= tsk_cred->uid;
	cred.euid	= tsk_cred->euid;
	cred.suid	= tsk_cred->suid;
	cred.fsuid	= tsk_cred->fsuid;
	cred.gid	= tsk_cred->gid;
	cred.egid	= tsk_cred->egid;
	cred.sgid	= tsk_cred->sgid;
	cred.fsgid	= tsk_cred->fsgid;
	cred.ngroups = gi->ngroups;
	
 	if (wdump_write(desc, &cred, sizeof(wdump_cred_t)) != sizeof(wdump_cred_t)) {
		wdump_log("failed to save cred");
		return -EIO;
	}

	if (cred.ngroups) {
		int i;
		size_t size = cred.ngroups * sizeof(gid_t);
		gid_t *groups = vmalloc(size);
		
		if (!groups) {
			wdump_log("no memory");
			return -ENOMEM;
		}
			
		for (i = 0; i < gi->ngroups; i++)
			groups[i] = GROUP_AT(gi, i);
			
		if (wdump_write(desc, groups, size) != size) {
			wdump_log("failed to save group_info");
			ret = -EIO;
		}
		vfree(groups);
	}
	wdump_print_pos(desc);
	return ret;
}


int wdump_save_auxc(wdump_desc_t desc, struct task_struct *tsk)
{
	wdump_auxc_t auxc;
	
	wdump_print_save_auxc("saving aux context...");
	auxc.personality = tsk->personality;
	auxc.clear_child_tid = tsk->clear_child_tid;
	strncpy(auxc.comm, tsk->comm, sizeof(auxc.comm));
	
	if (wdump_write(desc, &auxc, sizeof(wdump_auxc_t)) != sizeof(wdump_auxc_t)) {
		wdump_log("failed to save aux context");
		return -EIO;
	}
	wdump_print_pos(desc);
	return 0;
}


int wdump_save_paths(wdump_desc_t desc, struct task_struct *tsk)
{
	int len;
	int ret = 0;
	char *path = (char *)kmalloc(WDUMP_PATH_MAX, GFP_KERNEL);
	
	if (!path) {
		wdump_log("no memory");
		return -ENOMEM;
	}
	ret = wdump_get_cwd(tsk, path, WDUMP_PATH_MAX);
	if (ret < 0) {
		wdump_log("failed to get cwd");
		goto out;
	}
	len = strlen(path) + 1;
	if (wdump_write(desc, &len, sizeof(int)) != sizeof(int)) {
		wdump_log("failed to save cwd");
		ret = -EIO;
		goto out;
	}
	if (wdump_write(desc, path, len) != len) {
		wdump_log("failed to save cwd");
		ret = -EIO;
		goto out;
	}
	wdump_print_save_paths("cwd=%s", path);
	ret = wdump_get_root(tsk, path, WDUMP_PATH_MAX);
	if (ret < 0) {
		wdump_log("failed to get root");
		goto out;
	}
	len = strlen(path) + 1;
	if (wdump_write(desc, &len, sizeof(int)) != sizeof(int)) {
		wdump_log("failed to save root");
		ret = -EIO;
		goto out;
	}
	if (wdump_write(desc, path, len) != len) {
		wdump_log("failed to save root");
		ret = -EIO;
		goto out;
	}
	wdump_print_save_paths("root=%s", path);
	wdump_print_pos(desc);
out:
	kfree(path);
	return ret;
}


int wdump_save(wdump_desc_t desc, struct task_struct *tsk)
{
	int ret = wdump_save_paths(desc, tsk);
	if (ret)
		return ret;
		
	ret = wdump_save_cred(desc, tsk);
	if (ret)
		return ret;
	
	ret = wdump_save_auxc(desc, tsk);
	if (ret)
		return ret;
		
	ret = wdump_save_cpu(desc, tsk);
	if (ret)
		return ret;
		
	ret = wdump_save_fpu(desc, tsk);
	if (ret)
		return ret;
	
	ret = wdump_save_files(desc, tsk);
	if (ret)
		return ret;
	
	ret = wdump_save_signals(desc, tsk);
	if (ret)
		return ret;
	
	return wdump_save_mm(desc, tsk);
}
