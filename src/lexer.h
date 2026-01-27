#pragma once
#include "arena.h"
#include "token.h"
#include "string_pool.h"

Token *tokenize(char *source, StringPool *pool);
