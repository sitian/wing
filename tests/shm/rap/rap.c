#include "lock.h"
#include "barrier.h"
#include "rap.h"

void free_lock(rap_lock_t *lock)
{
	shmlock_release(lock->desc, lock->lock);
}


void free_area(rap_areas_t *areas, int n)
{
	rap_area_t *area = &areas->areas[n];
	
	if (!(areas->flags & RAP_CONT)) {
		shmdt(area->buf);
		shmctl(area->desc, IPC_RMID, NULL);
	}
	free_lock(&area->lock);
}


void free_areas(rap_areas_t *areas)
{
	int i;
	
	for (i = 0; i < areas->count; i++)
		free_area(areas, i);
		
	if (areas->flags & RAP_CONT) {
		shmdt(areas->buf);
		shmctl(areas->desc, IPC_RMID, NULL);
	}
	free(areas);
}


void detach_lock(rap_lock_t *lock)
{
	shmdt(lock->lock);
}


void detach_area(rap_areas_t *areas, int n)
{
	rap_area_t *area = &areas->areas[n];
	
	if (!(areas->flags & RAP_CONT))
		shmdt(area->buf);
	detach_lock(&area->lock);
}


void detach_areas(rap_areas_t *areas)
{
	int i;
	
	for (i = 0; i < areas->count; i++)
		detach_area(areas, i);
		
	if (areas->flags & RAP_CONT)
		shmdt(areas->buf);
	free(areas);
}


int alloc_lock(rap_lock_t *lock, int users)
{
	static int key = KEY_LOCK;
	
	lock->desc = shmlock_create(key, users);
	if (lock->desc < 0) {
		printf("Failed to allocate lock\n");
		return -1;
	}
	lock->lock = shmlock_init(lock->desc, users);
	if (lock->lock == (shmlock_t *)-1) {
		printf("Failed to allocate lock\n");
		shmlock_release(lock->desc, lock->lock);
		return -1;
	}
	key++;
	return 0;
}


int alloc_area(rap_areas_t *areas, int n, int users)
{
	int size = 1 << n;
	static int key = KEY_AREA;
	rap_area_t *area = &areas->areas[n];
	
	if (areas->flags & RAP_CONT) {
		area->desc = 0;
		area->buf = &areas->buf[size];
	} else {
		if ((area->desc = shmget(key, size, IPC_DIPC | IPC_CREAT)) < 0) {
			printf("Failed to allocate area\n");
			return -1;
		}
		area->buf = shmat(area->desc, NULL, 0);
		if (area->buf == (char *)-1) {
			printf("Failed to allocate area\n");
			shmctl(area->desc, IPC_RMID, NULL);
			return -1;
		}
		key++;
	}
	if (alloc_lock(&area->lock, users) < 0) {
		shmdt(area->buf);
		shmctl(area->desc, IPC_RMID, NULL);
	}
	area->size = size;
	return 0;
}


rap_areas_t *alloc_areas(int users, int flags)
{
	int i;
	rap_areas_t *areas = (rap_areas_t *)malloc(sizeof(rap_areas_t));
	
	if (!areas) {
		printf("Failed to allocate areas (no mem)");
		return NULL;
	}
	memset(areas, 0, sizeof(rap_areas_t));
	if (flags & RAP_CONT) {
		if ((areas->desc = shmget(KEY_AREA, (1 << RAP_AREAS), IPC_DIPC | IPC_CREAT)) < 0) {
			free(areas);
			return NULL;
		}
		areas->buf = shmat(areas->desc, NULL, 0);
		if (areas->buf == (char *)-1) {
			shmctl(areas->desc, IPC_RMID, NULL);
			free(areas);
			return NULL;
		}
	}
	areas->flags = flags;
	for (i = 0; i < RAP_AREAS; i++) {
		if (alloc_area(areas, i, users) < 0)
			goto err;
		areas->count += 1;
	}
	return areas;
err:
	free_areas(areas);
	return NULL;
}


int attach_lock(rap_lock_t *lock, int users)
{
	static int key = KEY_LOCK;
	
	lock->desc = shmlock_find(key, users);
	if (lock->desc < 0) {
		printf("Failed to attach lock\n");
		return -1;
	}
	lock->lock = shmlock_check(lock->desc);
	if (lock->lock == (shmlock_t *)-1) {
		printf("Failed to  attach lock\n");
		return -1;
	}
	key++;
	return 0;
}


int attach_area(rap_areas_t *areas, int n, int users)
{
	int size = 1 << n;
	static int key = KEY_AREA;
	rap_area_t *area = &areas->areas[n];
	
	if (areas->flags & RAP_CONT) {
		area->desc = 0;
		area->buf = &areas->buf[size];
	} else {
		if ((area->desc = shmget(key, size, IPC_DIPC)) < 0) {
			printf("Failed to attach area\n");
			return -1;
		}
		area->buf = shmat(area->desc, NULL, 0);
		if (area->buf == (char *)-1) {
			printf("Failed to attach area\n");
			return -1;
		}
		key++;
	}
	if (attach_lock(&area->lock, users) < 0) {
		printf("Failed to attach area\n");
		shmdt(area->buf);
		return -1;
	}
	area->size = size;
	return 0;
}


rap_areas_t *attach_areas(int users, int flags)
{
	int i;
	rap_areas_t *areas = (rap_areas_t *)malloc(sizeof(rap_areas_t));
	
	if (!areas) {
		printf("Failed to allocate areas\n");
		return NULL;
	}
	if (flags & RAP_CONT) {
		if ((areas->desc = shmget(KEY_AREA, (1 << RAP_AREAS), IPC_DIPC)) < 0) {
			free(areas);
			return NULL;
		}
		areas->buf = shmat(areas->desc, NULL, 0);
		if (areas->buf == (char *)-1) {
			free(areas);
			return NULL;
		}
	}
	areas->flags = flags;
	for (i = 0; i < RAP_AREAS; i++) {
		if (attach_area(areas, i, users) < 0)
			goto err;
		areas->count += 1;
	}
	return areas;
err:
	printf("Failed to attach areas\n");
	detach_areas(areas);
	return NULL;
}


void rap(int id, rap_areas_t *areas)
{
	int i;
	int j;
	rap_area_t *area;
	
	srand((int)time(0));
	for (i = 0; i < RAP_ROUNDS; i++) {
		area = &areas->areas[rand() % RAP_AREAS];
		shmlock_lock(id, area->lock.lock);
		if (rand() % 4) {
			char tmp;
			
			for (j = 0; j < area->size; j++)
				tmp = area->buf[j];
		} else { // 1/4 write operations
			for (j = 0; j < area->size; j++)
				area->buf[j] = rand() % 256;
		}
		shmlock_unlock(id, area->lock.lock);
	}
}


void save_result(float result)
{
	FILE *filp;
	
	filp = fopen("/var/._tmp", "w");
	fprintf(filp, "%f", result);
	fclose(filp);
}


int start_leader(int id, int total, int flags)
{
	int ret = -1;
	float result;
	int desc_start, desc_end;
	rap_areas_t *areas = NULL;
	shmlock_t *lock_end = NULL;
	shmlock_t *lock_start = NULL;
	struct timeval time_start, time_end;
	
	areas = alloc_areas(total, flags);
	if (!areas)
		return -1;
	
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
	
	shmlock_wait(lock_start, total - 1);
	gettimeofday(&time_start, NULL);
	rap(id, areas);
	shmlock_wait(lock_end, total - 1);
	gettimeofday(&time_end, NULL);
	result = time_end.tv_sec - time_start.tv_sec + (time_end.tv_usec - time_start.tv_usec) / 1000000.0;
	save_result(result);
	barrier_wait();
	ret = 0;
release_end:
	shmlock_release(desc_end, lock_end);
release_start:
	shmlock_release(desc_start, lock_start);
out:
	if (ret < 0)
		printf("Failed to start leader\n");
	free_areas(areas);
	return ret;
}


int start_member(int id, int total, int flags)
{
	float result;
	rap_areas_t *areas;
	shmlock_t *lock_end;
	shmlock_t *lock_start;
	int desc_start, desc_end;
	struct timeval time_start, time_end;
	
	while ((desc_end = shmlock_find(KEY_END, total)) < 0);
		
	lock_end = shmlock_check(desc_end);
	if (lock_end == (shmlock_t *)-1)
		goto err;
	
	if ((desc_start = shmlock_find(KEY_START, total)) < 0)
		goto err;
		
	lock_start = shmlock_check(desc_start);
	if (lock_start == (shmlock_t *)-1)
		goto err;
	
	areas = attach_areas(total, flags);
	if (!areas)
		goto err;
		
	shmlock_join(id, lock_start);
	shmlock_wait(lock_start, total - 1);
	gettimeofday(&time_start, NULL);
	rap(id, areas);
	shmlock_join(id, lock_end);
	shmlock_wait(lock_end, total - 1);
	gettimeofday(&time_end, NULL);
	result = time_end.tv_sec - time_start.tv_sec + (time_end.tv_usec - time_start.tv_usec) / 1000000.0;
	save_result(result);
	barrier_wait();
	detach_areas(areas);
	return 0;
err:
	printf("Failed to start member\n");
	return -1;
}


int start(int total, int flags)
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
		if (start_leader(id, total, flags))
			return -1;
	} else {
		if (start_member(id, total, flags))
			return -1;
	}
	return 0;
}


int main(int argc, char **argv)
{
	int total;
	int flags = 0;
	
	if (((argc < 2) || (argc > 3)) || ((3 == argc) && strcmp(argv[1], "-c"))) {
		printf("Usage: rap [-c] [users]\n");
		return -1;
	}
	
	if (3 == argc) {
		flags = RAP_CONT;
		total = atoi(argv[2]);
	} else
		total = atoi(argv[1]);
	
	return start(total, flags);
}
