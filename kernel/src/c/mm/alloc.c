#include <mm/alloc.h>
#include <global.h>
#include <temp/interface.h>

void *alloc_pages(struct pool_descriptor *pool, size_t pages) {
	if (pages == 0) {
		return NULL;
	}

	void *address = (void *)pool->pool_head;

	size_t count = 1;

	while (pool->pool_head != NULL && count <= pages) {
		size_t difference = (size_t)((uintptr_t)pool->pool_head->next - (uintptr_t)pool->pool_head);
		
		pool->pool_head = pool->pool_head->next;
		
		if (difference == pool->object_size) {
			count++;
			continue;
		}

		count = 1;
		address = (void *)pool->pool_head;
	}

	if (count < pages) {
		printf("Failed to allocate %d kernel pages\n", pages);
		return NULL;
	}
	
	return address;
}

void *free_pages(struct pool_descriptor *pool, void *address, size_t pages) {
	if (pages == 0) {
		return NULL;
	}

	struct free_node *current = (struct free_node *)address;

	for (size_t i = 0; i < pages; i++) {
		current = (struct free_node *)((uintptr_t)address + i * pool->object_size);
		current->next = (struct free_node *)((uintptr_t)current + pool->object_size);
		current = (struct free_node *)((uintptr_t)current + pool->object_size);
	}

	current->next = pool->pool_head;
	pool->pool_head = (struct free_node *)address;

	return address;
}

struct pool_descriptor init_pool(void *base, size_t object_size, size_t objects) {
	struct free_node *n_base = (struct free_node *)base;
	
	for (size_t i = 0; i < objects; i++) {
		uintptr_t address = (uintptr_t)base + i * object_size;

		struct free_node *current = (struct free_node *)(address);
		struct free_node *next = (struct free_node *)(address + object_size);

		current->next = next;
		next->next = 0;
	}

	return (struct pool_descriptor){ .pool_base = n_base, .pool_head = n_base, .object_size = object_size };
}