#ifndef _IO_H
#define _IO_H

#include "dump.h"

typedef struct file *wdump_desc_t;
#define wdump_desc_pos(desc) (desc)->f_pos

wdump_desc_t wdump_open(const char *path);
int wdump_read(wdump_desc_t desc, void *buf, size_t len);
int wdump_uread(wdump_desc_t desc, void *buf, size_t len);
int wdump_write(wdump_desc_t desc, void *buf, size_t len);
int wdump_close(wdump_desc_t desc);

#endif
