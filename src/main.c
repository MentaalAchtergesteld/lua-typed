#include <stdio.h>

#include "arena.h"
#include "lexer.h"
#include "string_pool.h"
#include "vec.h"

char *read_file(MemArena *arena, const char *path) {
	FILE *f = fopen(path, "rb");

	if (!f) {
		fprintf(stderr, "Error: Could not open file: '%s'\n", path);
		return NULL;
	}

	fseek(f, 0, SEEK_END);
	long length = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (length < 0) {
		fclose(f);
		return NULL;
	}

	char *buffer = (char*)arena_push(arena, length+1, false);

	size_t read_count = fread(buffer, 1, length, f);
	if (read_count != (size_t)length) {
		fprintf(stderr, "Error: could not read entire file: '%s'\n", path);
		fclose(f);
		return NULL;
	}

	buffer[length] = '\0';
	fclose(f);
	return buffer;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("Usage: %s <file.luat>\n", argv[0]);
		return 1;
	}

	MemArena *perm_arena = arena_create(MiB(1));
	MemArena *scratch_arena = arena_create(MiB(1));
	StringPool pool = pool_create(perm_arena, KiB(4));

	char *source = read_file(perm_arena, argv[1]);
	if (!source) return 1;

	Token *tokens = tokenize(perm_arena, scratch_arena, &pool, source);

	for (size_t i = 0; i < vec_length(&tokens); i++) {
		printf("Token: %s; Kind: %d\n", tokens[i].text, tokens[i].kind);
	}

	arena_destroy(scratch_arena);
	arena_destroy(perm_arena);
	return 0;
}
