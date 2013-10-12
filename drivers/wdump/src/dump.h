#ifndef _WDUMP_H
#define _WDUMP_H

#include <linux/cdev.h> 
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/fs_struct.h>
#include <linux/hugetlb_inline.h>
#include <linux/highmem.h>
#include <linux/mman.h>
#include <linux/mount.h>
#include <linux/module.h>
#include <linux/namei.h>
#include <linux/prctl.h>
#include <linux/syscalls.h>
#include <asm/i387.h>

#define WDUMP_DEBUG

#ifdef WDUMP_DEBUG
#define DEBUG_SAVE
#define DEBUG_RESTORE
//#define DEBUG_CONN
#endif

#define WDUMP_DEVICE_NAME		"wdump"
#define WDUMP_VERSION			"0.0.1"
#define WDUMP_MAJOR				1
#define WDUMP_MINOR				6

#define WDUMP_DEV_FILE    		"/dev/wdump"
#define WDUMP_MAGIC				0xcc
#define WDUMP_IOCTL_CHECKPOINT	_IOW(WDUMP_MAGIC, 1, wdump_ioctl_args_t)
#define WDUMP_IOCTL_RESTART		_IOW(WDUMP_MAGIC, 2, wdump_ioctl_args_t)
#define WDUMP_MAX_IOC_NR		2
#define WDUMP_PATH_MAX			4096
#define WDUMP_DEBUGREG_NUM		HBP_NUM + 2

#define WDUMP_CONT				0x00000001
#define WDUMP_KILL				0x00000002
#define WDUMP_STOP				0x00000004

#define wdump_log(fmt, ...) printk("%s: " fmt "\n", __func__, ##__VA_ARGS__)

typedef struct wdump_ioctl_args {
	int pid;
	int flags;
} wdump_ioctl_args_t;

typedef struct wdump_cred {
	pid_t gpid;
	uid_t uid,euid,suid,fsuid;
	gid_t gid,egid,sgid,fsgid;
	int ngroups;
} wdump_cred_t;

typedef struct wdump_vma {
	unsigned long start;
	unsigned long end;
	unsigned long flags;						  
	unsigned long pgoff;
	unsigned long namelen;
	int arch;
} wdump_vma_t;

typedef struct wdump_mm {
	unsigned long context;
	unsigned long start_code, end_code, start_data, end_data;
	unsigned long start_brk, brk, start_stack, start_mmap;
	unsigned long arg_start, arg_end, env_start, env_end;
	unsigned long start_seg, mmap_base;
	int map_count;
} wdump_mm_t;

typedef struct wdump_file {
	int fd;
	int namelen;
	loff_t pos;
	mode_t mode;
	unsigned int flags;
} wdump_file_t;

typedef struct wdump_files_struct {
	int max_fds;
	int next_fd;
} wdump_files_struct_t;

typedef struct wdump_cpu {
	struct pt_regs regs;
	unsigned long debugregs[WDUMP_DEBUGREG_NUM];
	struct desc_struct tls_array[GDT_ENTRY_TLS_ENTRIES];
	void *sysenter_return;
} wdump_cpu_t;

typedef struct wdump_sigpending {
	sigset_t signal;
	int count;
} wdump_sigpending_t;

typedef struct wdump_sigqueue {
	int flags;
	struct siginfo info;
} wdump_sigqueue_t;

typedef struct wdump_auxc {
	unsigned long personality;
	int __user *clear_child_tid;
	char comm[TASK_COMM_LEN];
} wdump_auxc_t;

#endif

