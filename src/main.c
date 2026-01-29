#include <stdio.h>

#include "arena.h"
#include "debug.h"
#include "lexer.h"
#include "parser.h"
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
	StringPool pool = pool_create(perm_arena, KiB(50));

	char *source = read_file(perm_arena, argv[1]);
	if (!source) return 1;

	Token *tokens = tokenize(source, &pool);
	ParseResult parse_result = parse(tokens, perm_arena);

	if (parse_result.success) {
		Stmt *root = parse_result.root;

		FILE *token_dump = fopen("token_dump.txt", "w");
		if (token_dump) {
			fprint_tokens(token_dump, tokens);
			fclose(token_dump);
		}
		FILE *ast_dump = fopen("ast_dump.txt", "w");
		if (ast_dump) {
			fprint_ast(ast_dump, root);
			fclose(ast_dump);
		}
	} else {
		printf("Parser Error.\n");
	}


	vec_free(tokens);

	arena_destroy(perm_arena);
	return 0;
}
