#include <stdlib.h>
#include <string.h>

#include "string_pool.h"
#include "arena.h"

static u64 hash_string(const char *str, u64 length) {
	u64 hash = 0xcbf29ce484222325;
	for (u64 i = 0; i < length; i++) {
			hash ^= (u8)str[i];
			hash *= 0x100000001b3;
	}
	return hash;
}

StringPool pool_init(MemArena *arena, u64 capacity) {
	StringPool pool;
	pool.arena = arena;
	pool.capacity = capacity;

	pool.buckets = arena_push(arena, sizeof(StringEntry*) * capacity, false);

	return pool;
};

const char *pool_intern(StringPool *pool, const char *start, u64 length) {
	u64 hash = hash_string(start, length);
	u64 index = hash % pool->capacity;

	for (StringEntry *entry = pool->buckets[index]; entry != NULL; entry = entry->next) {
		if (
			entry->length == length &&
			memcmp(entry->str, start, length) == 0
		) return entry->str;
	}

	char *new_str = arena_push(pool->arena, length+1, false);
	memcpy(new_str, start, length);
	new_str[length] = '\0';

	StringEntry *new_entry = PUSH_STRUCT(pool->arena, StringEntry);
	new_entry->length = length;
	new_entry->str = new_str;

	new_entry->next = pool->buckets[index];
	pool->buckets[index] = new_entry;

	return new_str;
};
