#ifndef _WAIT_H
#define _WAIT_H

#include <pthread.h>
#include <sys/time.h>

static inline void set_timeout(long long timeout, struct timespec *time)
{
	long nsec = (timeout % 1000000) * 1000;
	
	clock_gettime(CLOCK_REALTIME, time);
	time->tv_sec += timeout / 1000000;
	if (time->tv_nsec + nsec >= 1000000000) {
		time->tv_nsec += nsec - 1000000000;
		time->tv_sec += 1;
	} else
		time->tv_nsec += nsec;
}

static inline long long get_time()
{
	struct timespec time;
	clock_gettime(CLOCK_REALTIME, &time);
	return (long long)time.tv_sec * 1000000 + time.tv_nsec / 1000;
}

static inline void wait(long long timeout)
{
	struct timespec time;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	
	if (timeout <= 0)
		return;

	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&cond, NULL);
	set_timeout(timeout, &time);

	pthread_mutex_lock(&mutex);
	pthread_cond_timedwait(&cond, &mutex, &time);
	pthread_mutex_unlock(&mutex);
}

#endif
