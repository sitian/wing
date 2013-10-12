#ifndef _LINE_H
#define _LINE_H

#include <string.h>
#include <stdlib.h>

#define PAGE_SIZE					4096
#define PAGE_SHIFT					12
	
#define WRES_LINE_SHIFT				4
#define WRES_LINE_SIZE				(PAGE_SIZE >> WRES_LINE_SHIFT)
#define WRES_LINE_MAX				(1 << WRES_LINE_SHIFT)	// Ensure that WRES_LINE_MAX >= 3
#define WRES_LINE_NR_SAMPLES 		16

#ifdef DEBUG_LINE
#define WRES_PRINT_NR_LINES			1
#define WRES_PRINT_NR_BYTES			(sizeof(unsigned long) * 20)
#define WRES_PRINT_LINE_DIFF
#define WRES_PRINT_LINES

#ifdef DEBUG_SHOW_MORE
#define WRES_PRINT_LINE_NUM
#define WRES_PRINT_LINE_CONTENT
#endif
#endif

typedef int32_t wres_digest_t;

typedef struct wres_line {
	wres_digest_t digest;
	int num;
	char buf[WRES_LINE_SIZE];
} wres_line_t;

#define wres_show_line_diff(line_num, old, nu) do { \
	int n; \
	const int cnt = WRES_PRINT_NR_BYTES / sizeof(unsigned long); \
	unsigned long *ptr; \
	ptr = (unsigned long *)(old); \
	printf("%s: ln=%d, old <", __func__, line_num); \
	for (n = 0; n < cnt - 1; n++) \
		printf("%lx, ", *ptr++); \
	printf("%lx>", *ptr); \
	ptr = (unsigned long *)(nu); \
	printf(", new <"); \
	for (n = 0; n < cnt - 1; n++) \
		printf("%lx, ", *ptr++); \
	printf("%lx>\n", *ptr); \
} while (0)

#define wres_show_line_info(resource, nr_lines, total) do { \
	printf("ln <new=%d, total=%d>", nr_lines, total); \
} while (0)

#ifdef WRES_PRINT_LINE_NUM
#define wres_show_line_num(resource, lines, nr_lines) do { \
	wres_show_owner(resource); \
	if (nr_lines > 0) { \
		int i; \
		printf(", ln <"); \
		for (i = 0; i < nr_lines - 1; i++) \
			printf("%d, ", (lines)[i].num); \
		printf("%d>", (lines)[i].num); \
	} else \
		printf(", ln <NULL>"); \
} while (0)
#else
#define wres_show_line_num(...) do {} while (0)
#endif

#ifdef WRES_PRINT_LINE_CONTENT
#define wres_show_line_content(resource, lines, nr_lines) do { \
	int n = WRES_PRINT_NR_LINES < nr_lines ? WRES_PRINT_NR_LINES : nr_lines; \
	if (n > 0) { \
		int i; \
		int left = -1; \
		const int cnt = WRES_PRINT_NR_BYTES / sizeof(unsigned long); \
		wres_show_owner(resource); \
		for (i = 0; i < n; i++) { \
			int j, pos = -1; \
			int min = WRES_LINE_MAX; \
			unsigned long *ptr; \
			for (j = 0; j < nr_lines; j++) { \
				int num = (lines)[j].num; \
				if ((num > left) && (num < min)) { \
					min = num; \
					pos = j; \
				} \
			} \
			if (pos < 0) \
				break; \
			ptr = (unsigned long *)(lines)[pos].buf; \
			printf(", ln%d <", (lines)[pos].num); \
			for (j = 0; j < cnt - 1; j++) \
				printf("%lx, ", *ptr++); \
			printf("%lx>", *ptr); \
			left = min; \
		} \
	} \
} while (0)
#else
#define wres_show_line_content(...) do {} while (0)
#endif

#ifdef WRES_PRINT_LINE_DIFF
#define wres_print_line_diff wres_show_line_diff
#else
#define wres_print_line_diff(...) do {} while (0)
#endif

#ifdef WRES_PRINT_LINES
#define wres_print_lines(resource, lines, nr_lines, total) do { \
	wres_show_owner(resource); \
	wres_show_line_info(resource, nr_lines, total); \
	wres_show_line_num(resource, lines, nr_lines); \
	wres_show_line_content(resource, lines, nr_lines); \
	printf("\n"); \
} while (0)
#else
#define wres_print_lines(...) do {} while (0)
#endif

wres_digest_t wres_line_get_digest(char *buf);

#endif
