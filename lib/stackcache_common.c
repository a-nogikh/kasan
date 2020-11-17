#include <linux/stackcache.h>
#include <linux/stacktrace.h>
#include <linux/memory.h>
#include <linux/stackdepot.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <../mm/slab.h>


void stack_cache_save(short trace_type, const volatile void *ptr, size_t size)
{
	unsigned long entries[STACK_CACHE_MAX_DEPTH];
	unsigned int nr_entries;

	nr_entries = stack_trace_save(entries, ARRAY_SIZE(entries), 0);
	/* Not sure if this is necessary. */
	nr_entries = filter_irq_stacks(entries, nr_entries);

	stack_cache_insert(ptr, size, trace_type, nr_entries, entries);
}

void stack_cache_save_kmem(short trace_type, const volatile void *ptr, struct kmem_cache *s)
{
	stack_cache_save(trace_type, ptr, s->object_size);
}
