#ifndef _UTIL_H
#define _UTIL_H

#include <mem.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include "event.h"
#include "page.h"
#include "proc.h"
#include "record.h"
#include "resource.h"
#include "shm.h"
#include "stringio.h"
#include "trace.h"
#include "member.h"
#include "rwlock.h"

#define __NR_shm_rdprotect		350
#define __NR_shm_wrprotect		351
#define __NR_shm_present		352

#define S_IRWXUGO	(S_IRWXU|S_IRWXG|S_IRWXO)
#define S_IALLUGO	(S_ISUID|S_ISGID|S_ISVTX|S_IRWXUGO)
#define S_IRUGO		(S_IRUSR|S_IRGRP|S_IROTH)
#define S_IWUGO		(S_IWUSR|S_IWGRP|S_IWOTH)
#define S_IXUGO		(S_IXUSR|S_IXGRP|S_IXOTH)

#define WRES_RETRY_INTERVAL 5000000 //usec

typedef struct wres_synctime_result {
	long retval;
	wres_time_t time;
} wres_synctime_result_t;

typedef struct wres_signin_result {
	long retval;
	int total;
	wres_id_t list[0];
} wres_signin_result_t;

typedef struct wres_signout_result {
	long retval;
} wres_signout_result_t;

typedef int(*func_ipc_init)(wresource_t *);
typedef int(*func_ipc_create)(wresource_t *);

static inline int sys_shm_rdprotect(key_t key, unsigned long addr, char *buf)
{
	return syscall(__NR_shm_rdprotect, key, addr, buf);
}

static inline int sys_shm_wrprotect(key_t key, unsigned long addr, char *buf)
{
	return syscall(__NR_shm_wrprotect, key, addr, buf);
}

static inline int sys_shm_present(key_t key, pid_t gpid, unsigned long addr, int flags)
{
	return syscall(__NR_shm_present, key, gpid, addr, flags);
}

extern int wln_send_request(wres_args_t *args);
extern int wln_broadcast_request(wres_args_t *args);
extern int wln_ioctl(wresource_t *resource, char *in, size_t inlen, char *out, size_t outlen, wres_id_t *to);
extern int wln_ioctl_async(wresource_t *resource, char *in, size_t inlen, char *out, size_t outlen, wres_id_t *to, pthread_t *tid);

int wres_ipcget(wresource_t *resource, func_ipc_create create, func_ipc_init init);
int wres_ioctl(wresource_t *resource, char *in, size_t inlen, char *out, size_t outlen, wres_id_t *to);
int wres_ioctl_async(wresource_t *resource, char *in, size_t inlen, char *out, size_t outlen, wres_id_t *to, pthread_t *tid);
int wres_synctime_request(wresource_t *resource, wres_time_t *off);
int wres_broadcast_request(wres_args_t *args);
int wres_send_request(wres_args_t *args);
int wres_ipcput(wresource_t *resource);
int wres_wait(wres_args_t *args);

wres_reply_t *wres_reply(wres_req_t *req, int flags);
wres_reply_t *wres_cancel(wres_req_t *req, int flags);
wres_reply_t *wres_signin(wres_req_t *req, int flags);
wres_reply_t *wres_signout(wres_req_t *req, int flags);
wres_reply_t *wres_synctime(wres_req_t *req, int flags);

#endif
