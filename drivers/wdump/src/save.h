#ifndef _SAVE_H
#define _SAVE_H

#include "dump.h"
#include "debug.h"
#include "conn.h"
#include "util.h"
#include "sys.h"
#include "io.h"

#ifdef DEBUG_SAVE
#define WDUMP_PRINT_SAVE_PATHS
#define WDUMP_PRINT_SAVE_VMA
#define WDUMP_PRINT_SAVE_FILE
#define WDUMP_PRINT_SAVE_FILES
#define WDUMP_PRINT_SAVE_CPU
#define WDUMP_PRINT_SAVE_FPU
#define WDUMP_PRINT_SAVE_MM
#define WDUMP_PRINT_SAVE_CRED
#define WDUMP_PRINT_SAVE_SIGNALS
#define WDUMP_PRINT_SAVE_PROC
#define WDUMP_PRINT_SAVE_AUXC
#endif

#ifdef WDUMP_PRINT_SAVE_PATHS
#define wdump_print_save_paths wdump_log
#else
#define wdump_print_save_paths(...) do {} while (0)
#endif

#ifdef WDUMP_PRINT_SAVE_VMA
#define wdump_print_save_vma(start, end, path, nr_pages, checksum) do { \
	if (path) \
		wdump_log("[%08lx, %08lx] %s pages=%d checksum=%02lx", start, end, path, nr_pages, checksum); \
	else \
		wdump_log("[%08lx, %08lx] pages=%d checksum=%02lx", start, end, nr_pages, checksum); \
} while (0)
#else
#define wdump_print_save_vma(...) do {} while (0)
#endif

#ifdef WDUMP_PRINT_SAVE_FILE
#define wdump_print_save_file wdump_log
#else
#define wdump_print_save_file(...) do {} while (0)
#endif

#ifdef WDUMP_PRINT_SAVE_FILES
#define wdump_print_save_files wdump_log
#else
#define wdump_print_save_files(...) do {} while (0)
#endif

#ifdef WDUMP_PRINT_SAVE_CPU
#define wdump_print_save_cpu wdump_log
#else
#define wdump_print_save_cpu(...) do {} while (0)
#endif

#ifdef WDUMP_PRINT_SAVE_FPU
#define wdump_print_save_fpu wdump_log
#else
#define wdump_print_save_fpu(...) do {} while (0)
#endif

#ifdef WDUMP_PRINT_SAVE_MM
#define wdump_print_save_mm wdump_log
#else
#define wdump_print_save_mm(...) do {} while (0)
#endif

#ifdef WDUMP_PRINT_SAVE_CRED
#define wdump_print_save_cred wdump_log
#else
#define wdump_print_save_cred(...) do {} while (0)
#endif

#ifdef WDUMP_PRINT_SAVE_SIGNALS
#define wdump_print_save_signals wdump_log
#else
#define wdump_print_save_signals(...) do {} while (0)
#endif

#ifdef WDUMP_PRINT_SAVE_AUXC
#define wdump_print_save_auxc wdump_log
#else
#define wdump_print_save_auxc(...) do {} while (0)
#endif

int wdump_save(wdump_desc_t desc, struct task_struct *tsk);

#endif
