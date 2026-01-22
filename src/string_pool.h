#pragma once

#include "arena.h"
#include "typedefs.h"

typedef struct StringEntry {
	struct StringEntry *next;
	u64 length;
	char *str;
} StringEntry;

typedef struct {
	MemArena *arena;
	StringEntry **buckets;
	u64 capacity;
} StringPool;

StringPool pool_init(MemArena *arena, u64 capacity);
const char *pool_intern(StringPool *pool, const char *start, u64 length);
