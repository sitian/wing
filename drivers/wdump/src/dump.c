//      wdump.c
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

#include "dump.h"
#include "ckpt.h"
#include "util.h"
#include "config.h"

static int wdump_checkpoint(pid_t pid, int flags) 
{
	int ret = 0;
	int stop = 0;
	struct task_struct *tsk = find_task_by_vpid(pid);
	
	if (!tsk) {
		wdump_log("cannot find task %d", pid);
		return -ESRCH;
	}
	
	if (tsk->cred->uid != current->cred->uid) {
		wdump_log("no permission");
		return -EACCES;    
	} 

	if (tsk != current) {
		send_sig(SIGSTOP, tsk, 0);
		stop = 1;
	}

	ret = wdump_ckpt_save(tsk);
	if (ret) {
		wdump_log("failed to take checkpoint");
		return ret;
	}

	if (flags & WDUMP_KILL)
		send_sig(SIGKILL, tsk, 0);
	else if (stop & (flags & WDUMP_CONT))
		send_sig(SIGCONT, tsk, 0);
			
	return 0;
}


static int wdump_restart(pid_t pid, int flags) 
{	
	send_sig(SIGSTOP, current, 0);
	if (wdump_ckpt_restore(pid)) {
		wdump_log("failed to restore");
		return -EFAULT;
	}
	if (!(flags & WDUMP_STOP))
		send_sig(SIGCONT, current, 0);
	return 0;		
}


/* Device Declarations */
/* The name for our device, as it will appear in /proc/devices */
static long wdump_ioctl(struct file *filp, unsigned int cmd, unsigned long val) 
{
	wdump_ioctl_args_t args;
	
	if ((_IOC_TYPE(cmd) != WDUMP_MAGIC) || (_IOC_NR(cmd) > WDUMP_MAX_IOC_NR))
		return -ENOTTY;
	
	if (copy_from_user(&args, (wdump_ioctl_args_t *)val, sizeof(wdump_ioctl_args_t)))
		return -EFAULT;
	
	switch(cmd){
	case WDUMP_IOCTL_CHECKPOINT:
		wdump_checkpoint(args.pid, args.flags);
		break;
	case WDUMP_IOCTL_RESTART:
		wdump_restart(args.pid, args.flags);
		break;
	default:
		break;    
	}
	return 0;
}


/* Module Declarations */
static int major;
struct cdev wdump_dev;
struct class *wdump_class;
struct file_operations wdump_ops = {
	owner: THIS_MODULE,
	unlocked_ioctl: wdump_ioctl,
};


static int wdump_init(void) 
{
	int ret;
	int devno;
	dev_t dev;
	
	ret = alloc_chrdev_region(&dev, 0, 1, WDUMP_DEVICE_NAME);  
	major = MAJOR(dev);
	if(ret < 0) {
		wdump_log("alloc_chrdev_region failed");
		return ret;
	}
	devno = MKDEV(major, 0);
	cdev_init(&wdump_dev, &wdump_ops);
	wdump_dev.owner = THIS_MODULE;
	wdump_dev.ops = &wdump_ops;
	ret = cdev_add(&wdump_dev, dev, 1);
	if (ret) {
		wdump_log("cdev_add failed");
		goto out;
	}
	wdump_class = class_create(THIS_MODULE, "cls_ckpt");
	if (IS_ERR(wdump_class)) {
		wdump_log("class_create failed");
		cdev_del(&wdump_dev);
		ret = -EINVAL;
		goto out;
	}
	device_create(wdump_class, NULL, devno, NULL, WDUMP_DEVICE_NAME);
	return 0;
out:
	unregister_chrdev_region(dev, 1);
	return ret;
}

static void wdump_cleanup(void) 
{
	dev_t dev = MKDEV(major, 0);         
	cdev_del(&wdump_dev);
	device_destroy(wdump_class, dev);
	class_destroy(wdump_class);
	unregister_chrdev_region(dev, 1);
}

module_init(wdump_init);
module_exit(wdump_cleanup);
MODULE_LICENSE("GPL");
