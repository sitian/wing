#include "barrier.h"

void barrier_wait()
{
	FILE *fp;
	
	fp = fopen("/var/._wait", "w");
	fclose(fp);
	while (!access("/var/._wait", F_OK))
		sleep(1);
}


int barrier_init(int total)
{
	int i;
	int desc;
	int ret = -1;
	union semun sem;
	int user_key = BARRIER_USER_KEY;
	
	desc = semget(BARRIER_KEY, 1, IPC_DIPC | IPC_CREAT);
	if (desc < 0) {
	    printf("barrier_init failed\n");
	    exit(-1);
	}
	
	sem.val = 1;
	if (semctl(desc, 0, SETVAL, sem) < 0) {
	    printf("barrier_init failed\n");
	    exit(-1);
	}
	
	for (i = 0; i < total; i++) {
		if (semget(user_key, 1, IPC_CREAT | IPC_DIPC | IPC_EXCL) >= 0) {
			ret = i;
			if (i == total - 1) {
				struct sembuf down;
				
				memset(&down, 0, sizeof(struct sembuf));
				down.sem_op = -1;
				if (semop(desc, &down, 1) < 0) {
					printf("barrier_init failed\n");
					exit(-1);
				}
			} else {
				struct sembuf wait;
				
				memset(&wait, 0, sizeof(struct sembuf));
				if (semop(desc, &wait, 1) < 0) {
					printf("barrier_init failed\n");
					exit(-1);
				}
			}
			break;
		}
		user_key++;
	}
	
	return ret;
}
