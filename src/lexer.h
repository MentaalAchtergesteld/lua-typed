#pragma once
#include "arena.h"
#include "token.h"
#include "string_pool.h"

Token *tokenize(MemArena *perm, MemArena *scratch, StringPool *pool, char *source);
