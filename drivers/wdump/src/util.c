//      util.c
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

#include "util.h"

pte_t *wdump_get_pte(struct mm_struct *mm, struct vm_area_struct *vma, unsigned long address)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	
	pgd = pgd_offset(mm, address);
	if (!pgd_present(*pgd))
		return NULL;

	pud = pud_offset(pgd, address);
	if (!pud_present(*pud))
		return NULL;

	pmd = pmd_offset(pud, address);
	if (!pmd_present(*pmd))
		return NULL;

	return pte_offset_map(pmd, address);
}


void *wdump_check_addr(struct mm_struct *mm, unsigned long address)
{
	pte_t *ptep;
	struct vm_area_struct *vma = find_vma(mm, address);
	
	if (!vma) {
		wdump_log("failed to check address");
		return NULL;
	}
		
	ptep = wdump_get_pte(mm, vma, address);
	if (!ptep) {
		int ret;
		
		ret = handle_mm_fault(mm, vma, address, 0);
		if (ret & VM_FAULT_ERROR) {
			wdump_log("failed to check address");
			return NULL;
		}
		ptep = wdump_get_pte(mm, vma, address);
		if (!ptep) {
			wdump_log("failed to check address");
			return NULL;
		}
	}
	pte_unmap(ptep);
	return page_address(pte_page(*ptep)) + (address & ~PAGE_MASK);
}


void *wdump_get_page(struct mm_struct *mm, struct vm_area_struct *vma, unsigned long address)
{
	pte_t *ptep;
	void *ptr = NULL;
	struct page *page = NULL;

retry:
	spin_lock(&mm->page_table_lock);
	ptep = wdump_get_pte(mm, vma, address);
	if (ptep) {
		pte_t pte = *ptep;
		
		pte_unmap(ptep);
		if (pte_present(pte)) {
			page = pte_page(pte);
			if (page == ZERO_PAGE(address) 
				|| (vma->vm_file && !PageAnon(page)) 
				|| pte_dump(pte)) {
				spin_unlock(&mm->page_table_lock);
				return NULL;
			}
			ptr = page_address(page);
		} else if (!pte_none(pte)) {
			int ret;
			
			spin_unlock(&mm->page_table_lock);
			ret = handle_mm_fault(mm, vma, address, 0);
			if (ret & VM_FAULT_ERROR) {
				wdump_log("failed to get page");
				return NULL;
			}
			goto retry;
		}
	}
	spin_unlock(&mm->page_table_lock);
	return ptr;
}


int wdump_check_fpu_state(void)
{
	int ret = 0;
    wdump_i387_t *i387 = wdump_get_i387(current);
	
    /* Invalid FPU states can blow us out of the water so we will do
     * the restore here in such a way that we trap the fault if the
     * restore fails.  This modeled after get_user and put_user. */
	if (cpu_has_fxsr) {
 	 	asm volatile
	  	("1: fxrstor %1               \n"
	  	"2:                          \n"
	 	".section .fixup,\"ax\"      \n"
	 	"3:  movl %2, %0             \n"
	 	"    jmp 2b                  \n"
         	".previous                   \n"
	 	".section __ex_table,\"a\"   \n"
	 	"    .align 4                \n"
	 	"    .long 1b, 3b            \n"
	 	".previous                   \n"
	 	: "+r"(ret)
	 	: "m" (i387->fxsave), "i"(-EFAULT));
	} else {
		asm volatile
		("1: frstor %1                \n"
	 	"2:                          \n"
 	 	".section .fixup,\"ax\"      \n"
	 	"3:  movl %2, %0             \n"
 	 	"    jmp 2b                  \n"
	 	".previous                   \n"
	 	".section __ex_table,\"a\"   \n"
	 	"    .align 4                \n"
	 	"    .long 1b, 3b            \n"
	 	".previous                   \n"
	 	: "+r"(ret)
	 	: "m" (i387->fsave), "i"(-EFAULT));
	}

	return ret;
}


static inline int wdump_is_unlinked_file(struct file *file)
{
	if (!file)
		return 0;
	else {
		struct dentry *dentry = file->f_dentry;
		
		return ((!IS_ROOT(dentry) && d_unhashed(dentry)) 
				|| !dentry->d_inode->i_nlink
				|| (dentry->d_flags & DCACHE_NFSFS_RENAMED));
	}
}


int wdump_can_dump(struct vm_area_struct *vma)
{
	return !vma->vm_file
			|| (!is_vm_hugetlb_page(vma)
				&& (!wdump_is_unlinked_file(vma->vm_file)
					|| !(vma->vm_flags & VM_SHARED)));
}


int wdump_get_file_type(struct file *file)
{
	if (file && file->f_dentry) {
		struct dentry *dentry = file->f_dentry;
		return dentry->d_inode->i_mode & S_IFMT;
	}
	return 0;
}


struct file *wdump_reopen_file(wdump_file_t *file, const char *path)
{
	struct file *filp = ERR_PTR(-ENOENT);
	int fd = get_unused_fd_flags(file->flags);
	
	if (fd >= 0) {
		int ret;
		
		filp = filp_open(path, file->flags, file->mode);
		if (IS_ERR(filp)) {
			put_unused_fd(fd);
			return filp;
		}
		fd_install(fd, filp);
		ret = sys_lseek(fd, file->pos, SEEK_SET);
		if (ret < 0) {
			filp_close(filp, NULL);
			put_unused_fd(fd);
			return ERR_PTR(ret);
		}
	}
	return filp;
}


static inline int wdump_get_file_flags(wdump_vma_t *area)
{
	unsigned long flags = area->flags;
	
	if ((flags & VM_MAYSHARE) && (flags & VM_MAYWRITE)) 
		return (flags & (VM_MAYREAD | VM_MAYEXEC)) ? O_RDWR : O_WRONLY;
	else
		return O_RDONLY;
}


unsigned long wdump_get_map_flags(wdump_vma_t *area)
{	
	unsigned long ret;
	
	if (area->arch) {
		ret = MAP_PRIVATE | MAP_ANONYMOUS;
	} else {
		unsigned long flags = area->flags;
		
		ret = MAP_FIXED | ((flags & VM_MAYSHARE) ? MAP_SHARED : MAP_PRIVATE);
		if (flags & VM_GROWSDOWN)
			ret |= MAP_GROWSDOWN;
		if (flags & VM_DENYWRITE)
			ret |= MAP_DENYWRITE;
	}	
	return ret;
}


unsigned long wdump_get_map_prot(wdump_vma_t *area)
{
	unsigned long ret = 0;
	
	if (area->arch) {
		ret = PROT_NONE;
	} else {
		unsigned long flags = area->flags;
		
		if (!area->namelen) {
			if (flags & VM_READ)
				ret |= PROT_READ;
			if (flags & VM_EXEC)
				ret |= PROT_EXEC;
			ret |= PROT_WRITE;
		} else {
			if (flags & VM_MAYSHARE) {
				if (flags & VM_MAYWRITE)
					ret |= PROT_WRITE;
				if (flags & VM_MAYREAD)
					ret |= PROT_READ;
				if (flags & VM_MAYEXEC)
					ret |= PROT_EXEC;
			} else
				ret = PROT_WRITE | PROT_READ | PROT_EXEC;
		}
	}
	return ret;
}


int wdump_map_area(char *path, wdump_vma_t *area)
{
	int ret;
	int prot = wdump_get_map_prot(area);
	int flags = wdump_get_map_flags(area);
	unsigned long pgoff = path ? area->pgoff : 0;
	struct file *filp = NULL;
	
	if (path) {
		filp = filp_open(path, wdump_get_file_flags(area), 0);
		if (IS_ERR(filp)) {
			wdump_log("failed to open %s", path);
			return PTR_ERR(filp);
		}
	}
	down_write(&current->mm->mmap_sem);
	ret = do_mmap_pgoff(filp, area->start, area->end - area->start, prot, flags, pgoff);
	up_write(&current->mm->mmap_sem);
	if (filp)
		fput(filp);
	if (ret != area->start) {
		wdump_log("[%lx, %lx], failed (%d)", area->start, area->end, ret);
		return -EINVAL;
	}
	return 0;
}


int wdump_remap_area(unsigned long from, wdump_vma_t *area)
{
	unsigned long addr;
	unsigned long to = area->start;
	unsigned long diff = from > to ? from - to : to - from;
	unsigned long len = area->end - area->start;
	
	if (diff < len) {
		unsigned long tmp = do_mmap_pgoff(NULL, 0, len, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, 0);
		
		if (IS_ERR_VALUE(tmp)) {
			wdump_log("failed to map (%d)", (int)tmp);
			return tmp;
		}
		addr = sys_mremap(from, len, len, MREMAP_FIXED | MREMAP_MAYMOVE, tmp); 
		if (addr != tmp) {
			wdump_log("failed to remap (%d)", (int)addr);
			return -EINVAL;
		}
		from = addr;
	}
		
	addr = sys_mremap(from, len, len, MREMAP_FIXED | MREMAP_MAYMOVE, to);
	if (addr != to) {
		wdump_log("failed to remap, %lx=>%lx (%d)", from, to, (int)addr);
		return -EINVAL;
	}
	return 0;
}


int wdump_get_cwd(struct task_struct *tsk, char *buf, unsigned long size)
{
	char *cwd;
	char *page = (char *)__get_free_page(GFP_USER);
	struct path path;
	
	if (!page) {
		wdump_log("no memory");
		return -ENOMEM;
	}

	get_fs_pwd(tsk->fs, &path);
	cwd = d_path(&path, page, PAGE_SIZE);
	if (IS_ERR(cwd)) {
		wdump_log("failed to get cwd");
		return PTR_ERR(cwd);
	}
	
	memcpy(buf, cwd, PAGE_SIZE + page - cwd);
	path_put(&path);
	free_page((unsigned long)page);
	return 0;
}


int wdump_get_root(struct task_struct *tsk, char *buf, unsigned long size)
{
	char *root;
	char *page = (char *)__get_free_page(GFP_USER);
	struct path path;
	
	if (!page) {
		wdump_log("no memory");
		return -ENOMEM;
	}

	get_fs_root(tsk->fs, &path);
	root = d_path(&path, page, PAGE_SIZE);
	if (IS_ERR(root)) {
		wdump_log("failed to get root");
		return PTR_ERR(root);
	}
	
	memcpy(buf, root, PAGE_SIZE + page - root);
	path_put(&path);
	free_page((unsigned long)page);
	return 0;
}


int wdump_set_cwd(char *name)
{
	int ret;
	struct path path;
	
	ret = kern_path(name, LOOKUP_DIRECTORY, &path);
	if (ret) {
		wdump_log("invalid path, %s", name);
		return ret;
	}
	ret = inode_permission(path.dentry->d_inode, MAY_EXEC | MAY_CHDIR);
	if (!ret)
		set_fs_pwd(current->fs, &path);
	path_put(&path);
	return ret;
}


int wdump_set_root(char *name)
{
	int ret;
	struct path path;
	
	ret = kern_path(name, LOOKUP_DIRECTORY, &path);
	if (ret) {
		wdump_log("invalid path, %s", name);
		return ret;
	}
	ret = inode_permission(path.dentry->d_inode, MAY_EXEC | MAY_CHDIR);
	if (!ret)
		set_fs_root(current->fs, &path);
	path_put(&path);
	return ret;
}
