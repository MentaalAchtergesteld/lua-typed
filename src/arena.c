#include <stdlib.h>
#include <memory.h>

#include "arena.h"

#define ARENA_BASE_POS (sizeof(MemArena))
#define ARENA_ALIGN (sizeof(void*))

MemArena *arena_create(u64 capacity) {
	MemArena *arena = (MemArena*)malloc(capacity);
	arena->capacity = capacity;
	arena->pos = ARENA_BASE_POS;

	return arena;
};

void arena_destroy(MemArena *arena) {
	free(arena);
};

void *arena_push(MemArena *arena, u64 size, bool non_zero) {
	u64 pos_aligned = ALIGN_UP_POW2(arena->pos, ARENA_ALIGN);
	u64 new_pos = pos_aligned + size;

	if (new_pos > arena->capacity) return NULL;

	arena->pos = new_pos;

	u8 *out = (u8*)arena + pos_aligned;

	if (!non_zero) {
		memset(out, 0, size);
	}

	return out;
};

void *arena_push_byte(MemArena *arena, u8 byte) {
	if (arena->pos >= arena->capacity) return NULL;
	u8 *ptr = (u8*)arena + arena->pos;
	*ptr = byte;
	arena->pos++;
	return ptr;
}

void *arena_resize(MemArena *arena, void *base, u64 old_size, u64 new_size) {
	if (base == NULL) return arena_push(arena, new_size, false);

	u8 *old_mem = (u8 *)base;
	u8 *arena_base = (u8 *)arena;

	u8 *old_end = old_mem + old_size;

	u8 *arena_head = arena_base + arena->pos;

	if (old_end == arena_head) {
		u64 diff = new_size - old_size;
		if (arena->pos + diff > arena->capacity) {
			return NULL;
		}

		arena->pos += diff;
		return base;
	}

	void *new_base = arena_push(arena, new_size, false);
	if (new_base) {
		memcpy(new_base, base, old_size);
	}
	return new_base;
}

void arena_pop(MemArena *arena, u64 size) {
	size = MIN(size, arena->pos - ARENA_BASE_POS);
	arena->pos -= size;
};

void arena_pop_to(MemArena *arena, u64 pos) {
	u64 size = pos < arena->pos ? arena->pos - pos : 0;
	arena_pop(arena, size);
};

void arena_clear(MemArena *arena) {
	arena_pop_to(arena, ARENA_BASE_POS);
};
