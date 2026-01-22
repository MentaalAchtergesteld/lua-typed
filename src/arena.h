#pragma once
#include <stdint.h>
#include <stdbool.h>

#include "typedefs.h"

typedef struct {
	u64 capacity;
	u64 pos;
} MemArena;

MemArena *arena_create(u64 capacity);
void arena_destroy(MemArena *arena);

void *arena_push(MemArena *arena, u64 size, bool non_zero);
void arena_pop(MemArena *arena, u64 size);
void arena_pop_to(MemArena *arena, u64 pos);
void arena_clear(MemArena *arena);

#define PUSH_STRUCT(arena, T)      (T*)arena_push((arena), sizeof(T), false)
#define PUSH_STRUCT_NZ(arena, T)   (T*)arena_push((arena), sizeof(T), true)
#define PUSH_ARRAY(arena, T, n)    (T*)arena_push((arena), sizeof(T) * (n), true)
#define PUSH_ARRAY_NZ(arena, T, n) (T*)arena_push((arena), sizeof(T) * (n), true)
