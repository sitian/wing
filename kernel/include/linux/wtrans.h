#ifndef _WTRANS_H
#define _WTRANS_H

#include <asm/uaccess.h>
#include <linux/mm.h>
#include <linux/utsname.h>
#include <linux/hugetlb.h>
#include <linux/unistd.h>
#include <linux/fs_struct.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/wres.h>


#define WCFS_MOUNT_PATH		"/wing/mnt/wcfs"
#define WLFS_MOUNT_PATH		"/wing/mnt/wlfs"

#define WTRANS_ROOT			"/wing/root/"
#define WTRANS_IO_MAX		((1 << 14) - 1)

#define WTRANS_MSG_DEBUG
#define WTRANS_SEM_DEBUG
#define WTRANS_SHM_DEBUG

#define wtrans_log(fmt, ...) printk("%s@%d: " fmt "\n", __func__, current->gpid, ##__VA_ARGS__)

static inline void *wtrans_malloc(size_t size)
{
	int order = get_order(size);
	unsigned long ret = __get_free_pages(GFP_KERNEL, order);
	
	if (ret) {
		int i;
		int nr_pages = 1 << order;
		unsigned long addr = ret;
		for (i = 0; i < nr_pages; i++) {
			SetPageReserved(virt_to_page(addr));
			addr += PAGE_SIZE;
		}
	}
	return (void *)ret;
}

static inline void wtrans_free(void *ptr, size_t size)
{
	unsigned long addr = (unsigned long)ptr;
	
	if (addr) {
		int i;
		int order = get_order(size);
		int nr_pages = 1 << order;
		for (i = 0; i < nr_pages; i++) {
			ClearPageReserved(virt_to_page(addr));
			addr += PAGE_SIZE;
		}
		free_pages((unsigned long)ptr, order);
	}
}

static inline void wtrans_get_path(wresource_t *resource, char *buf, size_t inlen, size_t outlen, char *path)
{
	unsigned long addr = buf ? __pa(buf) : 0;
	struct uts_namespace *ns = current->nsproxy->uts_ns;
	
	sprintf(path, "%s/%s/%lx_%lx_%lx_%lx_%lx_%lx_%lx_%lx_%lx", WLFS_MOUNT_PATH, ns->name.nodename,
		(unsigned long)resource->cls,
		(unsigned long)resource->key,
		(unsigned long)resource->entry[0],
		(unsigned long)resource->entry[1],
		(unsigned long)resource->entry[2],
		(unsigned long)resource->entry[3],
		(unsigned long)addr,
		(unsigned long)inlen,
		(unsigned long)outlen);
}

static inline int wtrans_ioctl(key_t key, wres_cls_t cls, wres_op_t op, pid_t gpid, wres_val_t val1, wres_val_t val2, 
                               char *buf, size_t inlen, size_t outlen)
{
	struct file *filp;
	wresource_t resource;
	char path[WRES_PATH_MAX];
	
	resource.key = key;
	resource.cls = cls;
	wres_set_entry(resource.entry, op, gpid, val1, val2);
	wtrans_get_path(&resource, buf, inlen, outlen, path);
	filp = filp_open(path, O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
    if (IS_ERR(filp)) {
		int ret = PTR_ERR(filp);
		return -EOK == ret ? 0 : ret;
	}
	filp_close(filp, NULL);
	return 0;
}

static inline int wtrans_enter(struct task_struct *tsk)
{
	char path[WRES_PATH_MAX];
	struct path *pwd = &tsk->fs->pwd;
	char *name = d_path(pwd, path, WRES_PATH_MAX);
	
	if (IS_ERR(name))
		return 0;
	
	return !strncmp(name, WTRANS_ROOT, strlen(WTRANS_ROOT));
}

static inline int wtrans_is_global_task(struct task_struct *tsk)
{
	return tsk->gpid > 0;
}

static inline int wtrans_is_global_area(void)
{
	return wtrans_is_global_task(current);
}

static inline pte_t *wtrans_get_pte(struct mm_struct *mm, unsigned long address, spinlock_t **ptlp)
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

	return pte_offset_map_lock(mm, pmd, address, ptlp);
}

static inline void wtrans_put_pte(pte_t *ptep, spinlock_t *ptl)
{
	pte_unmap_unlock(ptep, ptl);
}

static inline void wtrans_print(char *str)
{
	struct tty_struct *tty = current->signal->tty;
	
	if (tty) {
		 tty->ops->write(tty, str, strlen(str));
		 tty->ops->write(tty, "\015\012", 2);
	 }
}

int wtrans_load_vma(struct vm_area_struct *vma);

#ifdef WTRANS_MSG_DEBUG
#define wtrans_msg_print wtrans_log
#else
#define wtrans_msg_print(...) do {} while (0)
#endif

#ifdef WTRANS_SEM_DEBUG
#define wtrans_sem_print wtrans_log
#else
#define wtrans_sem_print(...) do {} while (0)
#endif

#ifdef WTRANS_SHM_DEBUG
#define wtrans_shm_print wtrans_log
#else
#define wtrans_shm_print(...) do {} while (0)
#endif

#endif
