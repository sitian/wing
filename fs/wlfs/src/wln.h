#ifndef _WLN_H
#define _WLN_H

#include <sys/stat.h>
#include <asm/ioctl.h>
#include <errno.h>
#include <wres.h>
#include <eval.h>
#include "wcom.h"
#include "wait.h"

#define WLN_DEBUG
#define WLN_RETRY_MAX			10
#define WLN_RETRY_INTERVAL		5000000 // usec
#define WLN_IO_MAX				((1 << 14) - 1)
#define WLN_PORT				12000

#ifdef WLN_DEBUG
#define wln_log(fmt, ...) printf("%s: " fmt "\n", __func__, ##__VA_ARGS__)
#else
#define wln_log(fmt, ...) do {} while (0)
#endif

int wln_send_request(wres_args_t *args);
int wln_broadcast_request(wres_args_t *args);
int wln_wait(wres_args_t *args);

#endif
