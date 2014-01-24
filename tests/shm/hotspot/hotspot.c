#include "lock.h"
#include "barrier.h"
#include "hotspot.h"

void hotspot(char *shmbuf, int shmsz, int flags)
{
	int i;
	int j;
	int size;
	int start;
	
	srand((int)time(0));
	for (i = 0; i < HOTSPOT_ROUNDS; i++) {
		start = 0;
		size = shmsz;
		
		while (size > PAGE_SIZE) {
			size /= 2;
			if (rand() % 2)
				start += size;
				
			if (flags & HOTSPOT_RANDOM) {
				int pos;
				int total;
				
				pos = rand() % size;
				total = rand() % size;
				for (j = 0; j < total; j++) {
					shmbuf[start + pos] = rand() % 256;
					pos++;
					if (pos == size)
						pos = 0;
				}
			} else {
				for (j = 0; j < size; j++)
					shmbuf[start + j] = rand() % 256;
			}
		}
	}
}


void save_result(float result)
{
	FILE *filp;
	
	filp = fopen("/var/._tmp", "w");
	fprintf(filp, "%f", result);
	fclose(filp);
}


int start_leader(int id, int total, int shmsz, int flags)
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
	hotspot(shmbuf, shmsz, flags);
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


int start_member(int id, int total, int shmsz, int flags)
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
	hotspot(shmbuf, shmsz, flags);
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


int start(int total, int shmsz, int flags)
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
		if (start_leader(id, total, shmsz, flags))
			return -1;
	} else {
		if (start_member(id, total, shmsz, flags))
			return -1;
	}
	return 0;
}


int main(int argc, char **argv)
{
	int total;
	int shmsz;
	int flags = 0;
	
	if ((argc < 3) || (argc > 4) || ((argc == 4) && strcmp(argv[1], "-r"))) {
		printf("Usage: hotspot [-r] [nodes] [shmsz]\n");
		return -1;
	}
	if (argc == 3) {
		total = atoi(argv[1]);
		shmsz = atoi(argv[2]);
	} else {
		total = atoi(argv[2]);
		shmsz = atoi(argv[3]);
		flags = HOTSPOT_RANDOM;
	}
	return start(total, shmsz, flags);
}
