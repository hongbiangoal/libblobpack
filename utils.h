/*
 * utils - misc libubox utility functions
 *
 * Copyright (C) 2012 Felix Fietkau <nbd@openwrt.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __LIBUBOX_UTILS_H
#define __LIBUBOX_UTILS_H

#include <sys/types.h>
#include <sys/time.h>
#include <time.h>

/*
 * calloc_a(size_t len, [void **addr, size_t len,...], NULL)
 *
 * allocate a block of memory big enough to hold multiple aligned objects.
 * the pointer to the full object (starting with the first chunk) is returned,
 * all other pointers are stored in the locations behind extra addr arguments.
 * the last argument needs to be a NULL pointer
 */

#define calloc_a(len, ...) __calloc_a(len, ##__VA_ARGS__, NULL)

void *__calloc_a(size_t len, ...);

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#define __BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))

#ifdef __OPTIMIZE__
extern int __BUILD_BUG_ON_CONDITION_FAILED;
#define BUILD_BUG_ON(condition)					\
	do {							\
		__BUILD_BUG_ON(condition);			\
		if (condition)					\
			__BUILD_BUG_ON_CONDITION_FAILED = 1;	\
	} while(0)
#else
#define BUILD_BUG_ON __BUILD_BUG_ON
#endif

#ifdef __APPLE__

#define CLOCK_REALTIME	0
#define CLOCK_MONOTONIC	1

void clock_gettime(int type, struct timespec *tv);

#endif

#endif
