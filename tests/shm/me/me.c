#include "lock.h"
#include "barrier.h"
#include "me.h"

void me(char *shmbuf, int shmsz, int id, shmlock_t *lock)
{
	int i;
	int j;
	int k;
	int nr_pages = shmsz / PAGE_SIZE;
	
	srand((int)time(0));
	for (i = 0; i < ME_ROUNDS; i++) {
		int count = rand() % nr_pages;
		
		shmlock_lock(id, lock);
		for (j = 0; j < count; j++) {
			int n = (rand() % nr_pages) * PAGE_SIZE;
			
			for (k = 0; k < PAGE_SIZE; k++) {
				shmbuf[n] = rand() % 256;
				n++;
			}
		}	
		shmlock_unlock(id, lock);
	}
}


void save_result(float result)
{
	FILE *filp;
	
	filp = fopen("/var/._tmp", "w");
	fprintf(filp, "%f", result);
	fclose(filp);
}


int start_leader(int id, int total, int shmsz)
{
	int ret = -1;
	char *shmbuf;
	float result;
	shmlock_t *lock_end = NULL;
	shmlock_t *lock_start = NULL;
	int desc, desc_start, desc_end;
	struct timeval time_start, time_end;
	
	desc_start = shmlock_create(KEY_START, total);
	if (desc_start < 0)
		goto out;
		
	lock_start = shmlock_init(desc_start, total);
	if (lock_start == (shmlock_t *)-1)
		goto release_start;
	
	desc_end = shmlock_create(KEY_END, total);
	if (desc_end < 0)
		goto release_start;
	
	lock_end = shmlock_init(desc_end, total);
	if (lock_end == (shmlock_t *)-1)
		goto release_end;
	
	if ((desc = shmget(KEY_MAIN, shmsz, IPC_DIPC | IPC_CREAT)) < 0)
		goto release_end;
	
	shmbuf = shmat(desc, NULL, 0);
	if (shmbuf == (char *)-1)
		goto release;

	memset(shmbuf, 0, shmsz);
	shmlock_wait(lock_start, total - 1);
	gettimeofday(&time_start, NULL);
	me(shmbuf, shmsz, id, lock_start);
	shmlock_wait(lock_end, total - 1);
	gettimeofday(&time_end, NULL);
	result = time_end.tv_sec - time_start.tv_sec + (time_end.tv_usec - time_start.tv_usec) / 1000000.0;
	save_result(result);
	barrier_wait();
	shmdt(shmbuf);
	ret = 0;
	
release:
	shmctl(desc, IPC_RMID, NULL);
release_end:
	shmlock_release(desc_end, lock_end);
release_start:
	shmlock_release(desc_start, lock_start);
out:
	if (ret < 0)
		printf("Failed to start leader\n");
	return ret;
}


int start_member(int id, int total, int shmsz)
{
	float result;
	char *shmbuf;
	shmlock_t *lock_end;
	shmlock_t *lock_start;
	int desc, desc_start, desc_end;
	struct timeval time_start, time_end;
	
	while ((desc = shmget(KEY_MAIN, shmsz, IPC_DIPC)) < 0)
		sleep(1);
	
	shmbuf = shmat(desc, NULL, 0);
	if (shmbuf == (char *)-1)
		goto err;
	
	if ((desc_start = shmlock_find(KEY_START, total)) < 0)
		goto err;
		
	lock_start = shmlock_check(desc_start);
	if (lock_start == (shmlock_t *)-1)
		goto err;
	
	if ((desc_end = shmlock_find(KEY_END, total)) < 0)
		goto err;
		
	lock_end = shmlock_check(desc_end);
	if (lock_end == (shmlock_t *)-1)
		goto err;
	
	shmlock_join(id, lock_start);
	shmlock_wait(lock_start, total - 1);
	gettimeofday(&time_start, NULL);
	me(shmbuf, shmsz, id, lock_start);
	shmlock_wait(lock_end, total - 1);
	gettimeofday(&time_end, NULL);
	result = time_end.tv_sec - time_start.tv_sec + (time_end.tv_usec - time_start.tv_usec) / 1000000.0;
	save_result(result);
	barrier_wait();
	return 0;
err:
	printf("Failed to start member\n");
	return -1;
}


int start(int total, int shmsz)
{
	int id;
	
	if (total <= 0) {
		printf("Invalid patameters\n");
		return -1;
	}
	
	id = barrier_init(total);
	if ((id < 0) || (id >= total)) {
		printf("Invalid ID\n");
		return -1;
	}
	
	if (0 == id) {
		if (start_leader(id, total, shmsz))
			return -1;
	} else {
		if (start_member(id, total, shmsz))
			return -1;
	}
	return 0;
}


int main(int argc, char **argv)
{
	int total;
	int shmsz;
	
	if (argc != 3) {
		printf("Usage: me [nodes] [shmsz]\n");
		return -1;
	}
	total = atoi(argv[1]);
	shmsz = atoi(argv[2]);
	return start(total, shmsz);
}
