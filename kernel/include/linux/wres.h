#ifndef _WRES_H
#define _WRES_H

enum wres_class {
	WRES_CLS_MSG,
	WRES_CLS_SEM,
	WRES_CLS_SHM,
	WRES_CLS_TSK,
	WRES_NR_CLASSES,
};

enum wres_operation {
	WRES_OP_UNUSED0,
	WRES_OP_MSGSND,
	WRES_OP_MSGRCV,
	WRES_OP_MSGCTL,
	WRES_OP_SEMOP,
	WRES_OP_SEMCTL,
	WRES_OP_SEMEXIT,
	WRES_OP_SHMFAULT,
	WRES_OP_SHMCTL,
	WRES_OP_UNUSED1,
	WRES_OP_TSKGET,
	WRES_OP_MSGGET,
	WRES_OP_SEMGET,
	WRES_OP_SHMGET,
	WRES_OP_SHMPUT,
	WRES_OP_TSKPUT,
	WRES_OP_RELEASE,
	WRES_OP_PROBE,
	WRES_OP_UNUSED2,
	WRES_OP_SIGNIN,
	WRES_OP_SIGNOUT,
	WRES_OP_SYNCTIME,
	WRES_OP_REPLY,
	WRES_OP_CANCEL,
	WRES_OP_UNUSED3,
	WRES_NR_OPERATIONS,
};

#define EOK 	900
#define ENOWNER 901
#define ERMID	902

#define WRES_RDONLY			0x00000001
#define WRES_RDWR			0x00000002	

#define WRES_POS_OP			0
#define WRES_POS_ID			1
#define WRES_POS_KEY		1
#define WRES_POS_VAL1		2
#define WRES_POS_CLS		2
#define WRES_POS_VAL2		3
#define WRES_POS_INDEX		3

#define WRES_PATH_MAX		128
#define WRES_ERRNO_MAX		1000
#define WRES_INDEX_MAX		((1 << 30) - 1)

typedef key_t wres_key_t;
typedef int32_t wres_id_t;
typedef int32_t wres_val_t;
typedef int32_t wres_entry_t;
typedef int32_t wres_index_t;
typedef uint32_t wres_cls_t;
typedef uint32_t wres_flg_t;
typedef uint32_t wres_op_t;

static inline void wres_set_entry(wres_entry_t *entry, wres_op_t op, wres_id_t id, wres_val_t val1, wres_val_t val2) 
{
	entry[WRES_POS_OP] = (wres_entry_t)op;
	entry[WRES_POS_ID] = (wres_entry_t)id; 
	entry[WRES_POS_VAL1] = (wres_entry_t)val1;
	entry[WRES_POS_VAL2] = (wres_entry_t)val2; 
}

typedef struct wresource {
	wres_cls_t cls;
	wres_key_t key;
	wres_entry_t entry[4];
} wresource_t;

typedef struct wres_cancel_args {
	wres_op_t op;
	wres_index_t index;
} wres_cancel_args_t;

typedef struct wres_msgrcv_result {
	long retval;
} wres_msgrcv_result_t;

typedef struct wres_msgrcv_args {
	long msgtyp;
	size_t msgsz;
	int msgflg;
} wres_msgrcv_args_t;

typedef struct wres_msgctl_args {
	int cmd;
} wres_msgctl_args_t;

typedef struct wres_msgctl_result {
	long retval;
} wres_msgctl_result_t;

typedef struct wres_semop_args {
	struct timespec timeout;
	short nsops;
	struct sembuf sops[0];
} wres_semop_args_t;

typedef struct wres_semop_result {
	long retval;
} wres_semop_result_t;

typedef struct wres_semctl_args {
	int semnum;
	int cmd;
} wres_semctl_args_t;

typedef struct wres_semctl_result {
	long retval;
} wres_semctl_result_t;

typedef struct wres_msgsnd_args {
	long msgtyp;
	size_t msgsz;
	int msgflg;
} wres_msgsnd_args_t;

typedef struct wres_msgsnd_result {
	long retval;
} wres_msgsnd_result_t;	

typedef struct wres_shmfault_result {
	long retval;
	char buf[PAGE_SIZE];
} wres_shmfault_result_t;

typedef struct wres_shmctl_args {
	int cmd;
} wres_shmctl_args_t;

typedef struct wres_shmctl_result {
	long retval;
} wres_shmctl_result_t;

#endif
