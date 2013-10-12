#ifndef _MEM_H
#define _MEM_H

#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

static inline int memget(unsigned long addr, size_t size, char **buf)
{
	int desc = open("/dev/mem", O_RDWR);
	
	if(desc < 0)
		return -1;

	*buf = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, desc, addr);
	if (!buf || (MAP_FAILED == buf)) {
		close(desc);
		return -1;
	}
	return desc;
}

#endif

