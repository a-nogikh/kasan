// SPDX-License-Identifier: GPL-2.0
/*
 * LRU-cache based implementation for stackcache.
 *
 * Copyright (C) 2020, Google Inc.
 */

#include <linux/stackcache.h>
#include <linux/mm.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/prandom.h>
#include <linux/spinlock.h>

#define SAMPLING_PERIOD (1 << 19)
#define MAX_ABS_OFFSET 32

static u32 lookup_count, lookup_success;
static atomic64_t skip_access = ATOMIC_INIT(0);
static struct stack_cache_response cache_resp[8];
static bool enable_data_collection;
static DEFINE_SPINLOCK(lookup_lock);

void stack_cache_memory_access(const volatile void *ptr, size_t size)
{
	const volatile void *query_ptr;
	size_t resp_size, i;
	unsigned long flags;
	s32 rand_offset;

	/* It's too early. */
	if (unlikely(!READ_ONCE(enable_data_collection)))
		return;

	/* Don't introduce big delays to interrupt handlers. */
	if (!in_task())
		return;

	/* We're only interested in accesses to dynamic memory. */
	if (!virt_addr_valid(ptr) || !PageSlab(virt_to_head_page((const void*)ptr)))
		return;

	/* Do sampling. */
	if (likely(atomic64_inc_return(&skip_access) % SAMPLING_PERIOD))
		return;

	/* Skip entries that were allocated after stackcache is initialised. */
	if (!stack_cache_check_history((unsigned long)ptr))
		return;

	/* Do only one lookup at a time. This helps to reduce stack frame size by sharing cache_resp. */
	if (spin_trylock_irqsave(&lookup_lock, flags) == 0)
		return;

	rand_offset = (s32)prandom_u32_max(MAX_ABS_OFFSET * 2) - MAX_ABS_OFFSET;
	query_ptr = (void *)((unsigned long)ptr + rand_offset);

	resp_size = stack_cache_lookup(query_ptr, size, cache_resp, ARRAY_SIZE(cache_resp));
	lookup_count++;

	for (i = 0; i < resp_size; i++) {
		unsigned long dist;

		/* Skip entries without full stack trace. */
		if (cache_resp[i].n_entries == 0)
			continue;

		/* Skip entries that do not include [ptr; ptr+size]. */
		if (ptr < cache_resp[i].object)
			continue;

		dist = ((unsigned long)ptr + size) - (unsigned long)cache_resp[i].object;
		if (dist <= cache_resp[i].size) {
			lookup_success++;
			break;
		}
	}

	spin_unlock_irqrestore(&lookup_lock, flags);
}

EXPORT_SYMBOL(stack_cache_memory_access);

static int __init init_stackcache_hitrate(void)
{
	struct dentry *parent;

	parent = debugfs_create_dir("stackcache", NULL);
	if (!parent) {
		printk(KERN_WARNING "init_stackcache_hitrate: failed to execute debugfs_create_dir\n");
		return -1;
	}

	debugfs_create_u32("lookup_count", 0777, parent, &lookup_count);
	debugfs_create_u32("lookup_success", 0777, parent, &lookup_success);
	return 0;
}

static int __init stackcache_hitrate_enable(void)
{
	WRITE_ONCE(enable_data_collection, 1);
	return 0;
}

module_init(init_stackcache_hitrate);
core_initcall(stackcache_hitrate_enable);
