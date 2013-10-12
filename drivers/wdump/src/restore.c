//      restore.c
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

#include "restore.h"

int wdump_restore_pages(wdump_desc_t desc, wdump_vma_t *area)
{
	int count = 0;
	unsigned long addr = 0;
		
	for (;;) {
		if (wdump_read(desc, &addr, sizeof(addr)) != sizeof(addr)) {
			wdump_log("failed to get address");
			return -EIO;
		}
		if (!addr)
			break;
		if ((addr < area->start) || (addr >= area->end)) {
			wdump_log("invalid address");
			return -EINVAL;
		}
		if (wdump_uread(desc, (void *)addr, PAGE_SIZE) != PAGE_SIZE) {
			wdump_log("failed to get page");
			return -EIO;
		}
		count++;
	}
	if (wdump_get_map_prot(area) & PROT_EXEC)
		flush_icache_range(area->start, area->end);
	return count;
}


int wdump_restore_vma(wdump_desc_t desc)
{
	int ret;
	int nr_pages = -1;
	char *path = NULL;
	wdump_vma_t area;
	
	if (wdump_read(desc, &area, sizeof(wdump_vma_t)) != sizeof(wdump_vma_t)) {
		wdump_log("failed to get area");
		return -EIO;
	}
	if (area.arch) {
		void *vdso;
		void *sysenter_return = current_thread_info()->sysenter_return;
		
		current->mm->context.vdso = (void *)(~0UL);
		ret = arch_setup_additional_pages(NULL, 0);
		if (ret < 0) {
			wdump_log("arch_setup_additional_pages failed");
			return ret;
		}
		vdso = current->mm->context.vdso;
		if ((vdso != (void *)(0UL)) && (vdso != (void *)area.start)) {
			ret = wdump_remap_area((unsigned long)vdso, &area);
			if (ret < 0) {
				wdump_log("failed to remap area");
				return ret;
			}
		}
		current->mm->context.vdso = (void *)area.start;
		current_thread_info()->sysenter_return = sysenter_return;
	} else {
		int namelen = area.namelen;
		
		if (namelen) {
			path = (char *)kmalloc(WDUMP_PATH_MAX, GFP_KERNEL);
			if (!path) {
				wdump_log("no memory");
				return -ENOMEM;
			}
			if (wdump_read(desc, path, namelen) != namelen) {
				wdump_log("failed to get path");
				kfree(path);
				return -EIO;
			}
		}
		if (!namelen && wdump_can_conn(area.flags)) {
			unsigned long prot = wdump_get_map_prot(&area);
			unsigned long flags = wdump_get_map_flags(&area);
			
			ret = wdump_conn_attach(current->gpid, area.start, area.end - area.start, prot, flags);
			if (ret) {
				wdump_log("failed to attach");
				return ret;
			}
		} else {
			ret = wdump_map_area(path, &area);
			if (ret) {
				wdump_log("failed to map");
				return ret;
			}
			nr_pages = wdump_restore_pages(desc, &area);
			if (nr_pages < 0) {
				wdump_log("failed to restore pages");
				return nr_pages;
			}
		}
		sys_mprotect(area.start, area.end - area.start, wdump_get_map_prot(&area));
	}
	wdump_print_restore_vma(area.start, area.end, path, nr_pages,
							wdump_get_vma_checksum(current->mm, NULL, area.start, 1));
	return 0;
}


int wdump_restore_mm(wdump_desc_t desc)
{
	int i;
	int ret;
	wdump_mm_t memory;
	struct mm_struct *mm = current->mm;
	
	wdump_print_restore_mm("restoring mm ...");
	if (wdump_read(desc, &memory, sizeof(wdump_mm_t)) != sizeof(wdump_mm_t)) {
		wdump_log("failed to get mem");
		return -EIO;
	}
	
	while (mm->mmap) {
		struct vm_area_struct *vma = mm->mmap;
		
		if (vma->vm_start >= TASK_SIZE)
			break;
		
		ret = do_munmap(mm, vma->vm_start, vma->vm_end - vma->vm_start);
		if (ret) {
			wdump_log("do_munmap failed");
			return ret;
		}
	}
	
	down_write(&mm->mmap_sem);
	mm->task_size	= TASK_SIZE;
	arch_pick_mmap_layout(mm);
	mm->mmap_base = memory.mmap_base;
	mm->free_area_cache = memory.mmap_base;
	mm->cached_hole_size = ~0UL;
	mm->start_code	= memory.start_code;
	mm->end_code	= memory.end_code;
	mm->start_data	= memory.start_data;
	mm->end_data	= memory.end_data;
	mm->start_brk	= memory.start_brk;
	mm->brk			= memory.brk;
	mm->start_stack = memory.start_stack;
	mm->arg_start	= memory.arg_start;
	mm->arg_end		= memory.arg_end;
	mm->env_start	= memory.env_start;
	mm->env_end		= memory.env_end;
	up_write(&mm->mmap_sem);
	
	for (i = 0; i < memory.map_count; i++) {
		ret = wdump_restore_vma(desc);
		if (ret) {
			wdump_log("failed to restore vma");
			return ret;
		}
	}
	wdump_print_pos(desc);
	return 0;
}


int wdump_restore_cpu(wdump_desc_t desc)
{
	int i;
	wdump_cpu_t cpu;
	unsigned long fs;
	unsigned long gs;
	struct pt_regs *regs = task_pt_regs(current);
	
	if (wdump_read(desc, &cpu, sizeof(wdump_cpu_t)) != sizeof(wdump_cpu_t)) {
		wdump_log("failed to restore cpu");
		return -EIO;
	}
	
	wdump_print_restore_cpu("restoring regs ...");
	fs = cpu.regs.fs;
	gs = cpu.regs.gs;
	cpu.regs.fs = 0;
	cpu.regs.gs = 0;
	cpu.regs.cs = __USER_CS;
	cpu.regs.ds = __USER_DS;
	cpu.regs.es = __USER_DS;
	cpu.regs.ss = __USER_DS;
	cpu.regs.flags = 0x200 | (cpu.regs.flags & 0xff);
	*regs = cpu.regs;
	
	wdump_print_restore_cpu("restoring tls ...");
	for (i = 0; i < GDT_ENTRY_TLS_ENTRIES; i++) {
		if ((cpu.tls_array[i].b & 0x00207000) != 0x00007000) {
			cpu.tls_array[i].a = 0;
			cpu.tls_array[i].b = 0;
		}
		current->thread.tls_array[i] = cpu.tls_array[i];
	}
	load_TLS(&current->thread, get_cpu());
	put_cpu();
	i = fs >> 3;
	if ((i < GDT_ENTRY_TLS_MIN) || (i > GDT_ENTRY_TLS_MAX) || ((fs & 7) != 3))
		fs = 0;
	i = gs >> 3;
	if ((i < GDT_ENTRY_TLS_MIN) || (i > GDT_ENTRY_TLS_MAX) || ((gs & 7) != 3))
		gs = 0;
	regs->fs = fs;
	regs->gs = gs;
	wdump_print_regs(regs);
	
	current_thread_info()->sysenter_return = cpu.sysenter_return;
	wdump_print_pos(desc);
	return 0;
}


int wdump_restore_fpu(wdump_desc_t desc)
{
	int ret;
	int flag;
	
	wdump_print_restore_fpu("restoring fpu ...");
	if (wdump_read(desc, &flag, sizeof(int)) != sizeof(int)) {
		wdump_log("failed to get file");
		return -EIO;
	}
	kernel_fpu_begin();
	clear_used_math();
	if (flag) {
		if (!wdump_get_i387(current)) {
			init_fpu(current);
			if (!wdump_get_i387(current)) {
				wdump_log("failed to get i387");
				return -EFAULT;
			}
		}
		if (wdump_read(desc, wdump_get_i387(current), xstate_size) != xstate_size) {
			wdump_log("failed to get i387");
			return -EFAULT;
		}
		ret = wdump_check_fpu_state();
		if (ret) {
			wdump_log("failed to restore i387");
			return ret;
		}
		set_used_math();
	}
	kernel_fpu_end();
	wdump_print_pos(desc);
	return 0;
}


int wdump_restore_file(wdump_desc_t desc)
{
	int ret = 0;
	int namelen;
	wdump_file_t file;
	struct file *filp;
	char *path = kmalloc(WDUMP_PATH_MAX, GFP_KERNEL);
	
	if (!path) {
		wdump_log("no memory");
		return -ENOMEM;
	}
	if (wdump_read(desc, &file, sizeof(wdump_file_t)) != sizeof(wdump_file_t)) {
		wdump_log("failed to get file");
		ret = -EIO;
		goto out;
	}
	if (!file.namelen) {
		wdump_log("invalid file name");
		ret = -EINVAL;
		goto out;
	}
	namelen = file.namelen;
	if (wdump_read(desc, path, namelen) != namelen) {
		wdump_log("failed to get path");
		ret = -EIO;
		goto out;
	}
	filp = wdump_reopen_file(&file, path);
	if (IS_ERR(filp)) {
		wdump_log("failed to reopen file");
		ret = -EINVAL;
		goto out;
	}
	wdump_print_restore_file("%s", path);
out:
	kfree(path);
	return ret;
}


int wdump_restore_files(wdump_desc_t desc)
{
	int type;
	int ret = 0;
	
	wdump_print_restore_files("restoring files ...");
	do {		
		if (wdump_read(desc, &type, sizeof(int)) != sizeof(int)) {
			wdump_log("failed to get file type");
			return -EIO;
		}
		switch (type) {
		case S_IFREG:
		case S_IFDIR:
			ret = wdump_restore_file(desc);
			break;
		default:
			break;
		}
		if (ret) {
			wdump_log("failed to restore file");
			return ret;
		}
	} while (type);
	wdump_print_pos(desc);
	return ret;
}


int wdump_restore_sigpending(wdump_desc_t desc, struct task_struct *tsk, struct sigpending *sigpending, int shared)
{
	int i;
	wdump_sigqueue_t queue;
	wdump_sigpending_t pending;
	struct siginfo *info = &queue.info;
	
	if (wdump_read(desc, &pending, sizeof(wdump_sigpending_t)) != sizeof(wdump_sigpending_t)) {
		wdump_log("failed to get sigpending");
		return -EIO;
	}
		
	for (i = 0; i < pending.count; i++) {
		if (wdump_read(desc, &queue, sizeof(wdump_sigqueue_t)) != sizeof(wdump_sigqueue_t)) {
			wdump_log("failed to get sigqueue");
			return -EIO;
		}
		if (shared) {
			read_lock((rwlock_t *)TASKLIST_LOCK);
			group_send_sig_info(info->si_signo, info, tsk);
			read_unlock((rwlock_t *)TASKLIST_LOCK);
		} else
			send_sig_info(info->si_signo, info, tsk);		
	}
	
	spin_lock_irq(&tsk->sighand->siglock);
	sigorsets(&sigpending->signal, &sigpending->signal, &pending.signal);
	recalc_sigpending();
	spin_unlock_irq(&tsk->sighand->siglock);
	return 0;
}


int wdump_restore_signals(wdump_desc_t desc)
{
	int i;
	int ret;
	stack_t sigstack;
	sigset_t sigblocked;

	wdump_print_restore_signals("restoring sigstack ...");
	if (wdump_read(desc, &sigstack, sizeof(stack_t)) != sizeof(stack_t)) {
		wdump_log("failed to get sigstack");
		return -EIO;
	}
		
	ret = compat_sigaltstack(current, &sigstack, NULL, 0);
	if (ret) {
		wdump_log("failed to restore sigstack (ret=%d)", ret);
		return ret;
	}
	
	wdump_print_restore_signals("restoring sigblocked ...");
	if (wdump_read(desc, &sigblocked, sizeof(sigset_t)) != sizeof(sigset_t)) {
		wdump_log("failed to restore sigstack");
		return -EIO;
	}
		
	sigdelsetmask(&sigblocked, sigmask(SIGKILL) | sigmask(SIGSTOP));
	spin_lock_irq(&current->sighand->siglock);
	current->blocked = sigblocked;
	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);
	
	wdump_print_restore_signals("restoring pending ...");
	ret = wdump_restore_sigpending(desc, current, &current->pending, 0);
	if (ret) {
		wdump_log("failed to restore pending");
		return ret;
	}
		
	ret = wdump_restore_sigpending(desc, current, &current->signal->shared_pending, 1);
	if (ret) {
		wdump_log("failed to restore shared_pending");
		return ret; 
	}
	
	wdump_print_restore_signals("restoring sigaction ...");
	for (i = 0; i < _NSIG; i++) {
		struct k_sigaction sigaction;
		
		if (wdump_read(desc, &sigaction, sizeof(struct k_sigaction)) != sizeof(struct k_sigaction)) {
			wdump_log("failed to get sigaction");
			return -EIO;
		}
			
		if ((i != SIGKILL - 1) && (i != SIGSTOP - 1)) {
	        ret = do_sigaction(i + 1, &sigaction, 0);
	        if (ret) {
				wdump_log("failed to restore sigaction (ret=%d)", ret);
				return ret;
			}
		}
	}
	wdump_print_pos(desc);
	return 0;
}


int wdump_restore_cred(wdump_desc_t desc)
{
	int ret = 0;
	wdump_cred_t cred;
	gid_t *groups = NULL;
	const struct cred *curr_cred = current->cred;
	
	wdump_print_restore_cred("restoring cred ...");
	if (wdump_read(desc, &cred, sizeof(wdump_cred_t)) != sizeof(wdump_cred_t)) {
		wdump_log("failed to get cred");
		return -EIO;
	}

	if (cred.ngroups > NGROUPS_MAX) {
		wdump_log("ngroups (%d) > NGROUPS_MAX", cred.ngroups);
		return -EINVAL;
	}
	
	if (cred.ngroups > 0) {
		int i; 
		size_t size = cred.ngroups * sizeof(gid_t);
		struct group_info *gi = curr_cred->group_info;
		
		groups = vmalloc(size);
		if (!groups) {
			wdump_log("no memory");
			return -ENOMEM;
		}
			
		if (wdump_read(desc, groups, size) != size) {
			wdump_log("failed to get groups");
			ret = -EIO;
			goto out;
		}
		
		for (i = 0; i < cred.ngroups; i++) {
			if (!groups_search(gi, groups[i])) {
				mm_segment_t fs = get_fs();
				set_fs(KERNEL_DS);
				ret = sys_setgroups(cred.ngroups, groups);
				set_fs(fs);
				if (ret < 0) {
					wdump_log("failed to set groups");
					goto out;
				}
				break;
			}
		}
	}

	current->gpid = cred.gpid;
	ret = sys_setresgid(cred.gid, cred.egid, cred.sgid);
	if (ret < 0) {
		wdump_log("failed to restore gid");
		goto out;
	}

	ret = sys_setresuid(cred.uid, cred.euid, cred.suid);
	if (ret < 0) {
		wdump_log("failed to restore uid");
		goto out;
	}
	
	if ((curr_cred->euid == curr_cred->uid) && (curr_cred->egid == curr_cred->gid))
		set_dumpable(current->mm, 1);
    else
		set_dumpable(current->mm, *compat_suid_dumpable);
	wdump_print_pos(desc);
out:
	if (groups)
		vfree(groups);
	return ret;
}


int wdump_restore_auxc(wdump_desc_t desc)
{
	int ret;
	wdump_auxc_t auxc;
	mm_segment_t fs;
	
	wdump_print_restore_auxc("restoring aux context...");
	if (wdump_read(desc, &auxc, sizeof(wdump_auxc_t)) != sizeof(wdump_auxc_t)) {
		wdump_log("failed to get aux context");
		return -EIO;
	}
	
	set_personality(auxc.personality);
	current->clear_child_tid = auxc.clear_child_tid;
	fs = get_fs();
	set_fs(KERNEL_DS);
	ret = sys_prctl(PR_SET_NAME, (unsigned long)auxc.comm, 0, 0, 0);
	set_fs(fs);
	
	wdump_print_pos(desc);
	return ret;
}


int wdump_restore_paths(wdump_desc_t desc)
{
	int length;
	int ret = 0;
	char *path = (char *)kmalloc(WDUMP_PATH_MAX, GFP_KERNEL);
	
	if (!path) {
		wdump_log("no memory");
		return -ENOMEM;
	}
	if (wdump_read(desc, &length, sizeof(int)) != sizeof(int)) {
		wdump_log("invalid cwd");
		ret = -EIO;
		goto out;
	}
	if (length <= 0) {
		wdump_log("invalid cwd");
		ret = -EINVAL;
		goto out;
	}
	if (wdump_read(desc, path, length) != length) {
		wdump_log("invalid cwd");
		ret = -EIO;
		goto out;
	}
	ret = wdump_set_cwd(path);
	if (ret) {
		wdump_log("failed to set cwd");
		goto out;
	}
	wdump_print_restore_paths("cwd=%s", path);
	if (wdump_read(desc, &length, sizeof(int)) != sizeof(int)) {
		wdump_log("invalid root");
		ret = -EIO;
		goto out;
	}
	if (length <= 0) {
		wdump_log("invalid root");
		ret = -EINVAL;
		goto out;
	}
	if (wdump_read(desc, path, length) != length) {
		wdump_log("invalid root");
		ret = -EIO;
		goto out;
	}
	ret = wdump_set_root(path);
	if (ret) {
		wdump_log("failed to set root");
		goto out;
	}
	wdump_print_restore_paths("root=%s", path);
	wdump_print_pos(desc);
out:
	kfree(path);
	return ret;
}


int wdump_restore(wdump_desc_t desc)
{
	int ret;
	
	ret = wdump_restore_paths(desc);
	if(ret)
		return ret;
	
	ret = wdump_restore_cred(desc);
	if (ret)
		return ret;
		
	ret = wdump_restore_auxc(desc);
	if (ret)
		return ret;
	
	ret = wdump_restore_cpu(desc);
	if (ret)
		return ret;
		
	ret = wdump_restore_fpu(desc);
	if (ret)
		return ret;
	
	ret = wdump_restore_files(desc);
	if (ret)
		return ret;
	
	ret = wdump_restore_signals(desc);
	if (ret)
		return ret;
	
	return wdump_restore_mm(desc);
}
