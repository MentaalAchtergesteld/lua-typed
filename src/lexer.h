#pragma once
#include "arena.h"
#include "token.h"
#include "string_pool.h"

typedef struct {
	Token *data;
	u64 count;
} TokenList;

TokenList lexer_tokenize(MemArena *arena, const char *source, StringPool *pool);
