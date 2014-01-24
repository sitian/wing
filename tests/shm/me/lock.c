#include "lock.h"

int shmlock_create(int key, int total)
{	
	return shmget(key, shmlock_size(total), IPC_DIPC | IPC_CREAT);
}


int shmlock_find(int key, int total)
{	
	return shmget(key, shmlock_size(total), IPC_DIPC);
}


shmlock_t *shmlock_check(int desc)
{
	return shmat(desc, NULL, 0);
}


shmlock_t *shmlock_init(int desc, int total)
{
	shmlock_t *lock = shmat(desc, NULL, 0);
	
	memset(lock, 0, shmlock_size(total));
	lock->total = total;
	return lock;
}


void shmlock_wait(shmlock_t *lock, int total)
{
	while (lock->count != total);
}


void shmlock_lock(int id, shmlock_t *lock)
{
	int pos = id - 1;
start:
	lock->B[pos] = 1;
	lock->X = id;
	if (lock->Y != 0) {
		lock->B[pos] = 0;
		while (lock->Y);
		goto start;
	}
	lock->Y = id;
	if (lock->X != id) {
		int i;
		int total = lock->total;
		
		lock->B[pos] = 0;
		for (i = 0; i < total; i++)
			while (lock->B[i]);
	
		if (lock->Y != id) {
			while (lock->Y);
			goto start;
		}
	}
}


void shmlock_unlock(int id, shmlock_t *lock)
{
	lock->Y = 0;
	lock->B[id - 1] = 0;
}


void shmlock_join(int id, shmlock_t *lock)
{
	shmlock_lock(id, lock);
	lock->count += 1;
	shmlock_unlock(id, lock);
}


void shmlock_release(int desc, shmlock_t *lock)
{
	if (lock && (lock != (shmlock_t *)-1))
		shmdt(lock);
		
	if (desc >= 0)
		shmctl(desc, IPC_RMID, NULL);
}

