/* SPDX-License-Identifier: GPL-2.0.-or-later */
/*
 * Functions that help to benchmark stackcache
 *
 * Copyright (C) 2020 Google, Inc.
 *
 */

#ifndef _LINUX_STACKCACHE_HITRATE_H
#define _LINUX_STACKCACHE_HITRATE_H

#include <linux/types.h>

void stack_cache_memory_access(const volatile void *ptr, size_t size);

#endif /* _LINUX_STACKCACHE_HITRATE_H */
