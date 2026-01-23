#include "lexer.h"
#include "arena.h"
#include "string_pool.h"
#include <string.h>

typedef struct {
	const char *start;
	const char *current;
	u64 line;
	StringPool *pool;
} LexerState;
