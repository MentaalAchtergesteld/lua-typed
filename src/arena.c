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

	if (new_pos > arena->capacity) {
		return NULL;
	}

	arena->pos = new_pos;

	u8 *out = (u8*)arena + pos_aligned;

	if (!non_zero) {
		memset(out, 0, size);
	}

	return out;
};

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
