#ifndef _RESTORE_H
#define _RESTORE_H

#include "dump.h"
#include "debug.h"
#include "util.h"
#include "conn.h"
#include "sys.h"
#include "io.h"

#ifdef DEBUG_RESTORE
#define WDUMP_PRINT_RESTORE_PATHS
#define WDUMP_PRINT_RESTORE_VMA
#define WDUMP_PRINT_RESTORE_FILE
#define WDUMP_PRINT_RESTORE_FILES
#define WDUMP_PRINT_RESTORE_CPU
#define WDUMP_PRINT_RESTORE_FPU
#define WDUMP_PRINT_RESTORE_MM
#define WDUMP_PRINT_RESTORE_CRED
#define WDUMP_PRINT_RESTORE_SIGNALS
#define WDUMP_PRINT_RESTORE_AUXC
#endif

#ifdef WDUMP_PRINT_RESTORE_PATHS
#define wdump_print_restore_paths wdump_log
#else
#define wdump_print_restore_paths(...) do {} while (0)
#endif

#ifdef WDUMP_PRINT_RESTORE_SIGNALS
#define wdump_print_restore_signals wdump_log
#else
#define wdump_print_restore_signals(...) do {} while (0)
#endif

#ifdef WDUMP_PRINT_RESTORE_VMA
#define wdump_print_restore_vma(start, end, path, nr_pages, checksum) do { \
	if (path) { \
		if (nr_pages >= 0) \
			wdump_log("[%08lx, %08lx] %s pages=%d checksum=%02lx", start, end, path, nr_pages, checksum); \
		else \
			wdump_log("[%08lx, %08lx] %s checksum=%02lx", start, end, path, checksum); \
	} else { \
		if (nr_pages >= 0) \
			wdump_log("[%08lx, %08lx] pages=%d checksum=%02lx", start, end, nr_pages, checksum); \
		else \
			wdump_log("[%08lx, %08lx] checksum=%02lx", start, end, checksum); \
	} \
} while (0)
#else
#define wdump_print_restore_vma(...) do {} while (0)
#endif

#ifdef WDUMP_PRINT_RESTORE_MM
#define wdump_print_restore_mm wdump_log
#else
#define wdump_print_restore_mm(...) do {} while (0)
#endif

#ifdef WDUMP_PRINT_RESTORE_CRED
#define wdump_print_restore_cred wdump_log
#else
#define wdump_print_restore_cred(...) do {} while (0)
#endif

#ifdef WDUMP_PRINT_RESTORE_CPU
#define wdump_print_restore_cpu wdump_log
#else
#define wdump_print_restore_cpu(...) do {} while (0)
#endif

#ifdef WDUMP_PRINT_RESTORE_FPU
#define wdump_print_restore_fpu wdump_log
#else
#define wdump_print_restore_fpu(...) do {} while (0)
#endif

#ifdef WDUMP_PRINT_RESTORE_FILE
#define wdump_print_restore_file wdump_log
#else
#define wdump_print_restore_file(...) do {} while (0)
#endif

#ifdef WDUMP_PRINT_RESTORE_FILES
#define wdump_print_restore_files wdump_log
#else
#define wdump_print_restore_files(...) do {} while (0)
#endif

#ifdef WDUMP_PRINT_RESTORE_AUXC
#define wdump_print_restore_auxc wdump_log
#else
#define wdump_print_restore_auxc(...) do {} while (0)
#endif

int wdump_restore(wdump_desc_t desc);

#endif
