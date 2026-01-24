#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>

#include "lexer.h"
#include "arena.h"
#include "string_pool.h"
#include "token.h"
#include "vec.h"

typedef struct {
	const char *word;
	TokenKind kind;
} Keyword;

static Keyword keywords[] = {
	{"local",    TOKEN_LOCAL},
	{"function", TOKEN_FUNCTION},
	{"return",   TOKEN_RETURN},
	{"if",       TOKEN_IF},
	{"then",     TOKEN_THEN},
	{"else",     TOKEN_ELSE},
	{"elseif",   TOKEN_ELSEIF},
	{"end",      TOKEN_END},
	{"while",    TOKEN_WHILE},
	{"do",       TOKEN_DO},
	{"repeat",   TOKEN_REPEAT},
	{"until",    TOKEN_UNTIL},
	{"for",      TOKEN_FOR},
	{"in",       TOKEN_IN},
	{"break",    TOKEN_BREAK},
	{"nil",      TOKEN_NIL},
	{"true",     TOKEN_TRUE},
	{"false",    TOKEN_FALSE},
	{"and",      TOKEN_AND},
	{"or",       TOKEN_OR},
	{"not",      TOKEN_NOT},
	{"type",     TOKEN_TYPE},
};

#define KEYWORD_COUNT (sizeof(keywords)/sizeof(keywords[0]))

typedef struct {
	const char *start;
	const char *current;
	u64 line;
} Scanner;

static Token make_empty_token(Scanner *s, TokenKind kind) {
	Token token;
	token.kind = kind;
	token.start = s->start;
	token.length = (u64)(s->current-s->start);
	token.line = s->line;
	return token;
}

static Token make_token(Scanner *s, StringPool *pool, TokenKind kind) {
	Token token = make_empty_token(s, kind);
	token.text = pool_intern(pool, token.start, token.length);
	return token;
}

static Token error_token(Scanner *s, StringPool *pool, char *msg) {
	Token token = make_empty_token(s, TOKEN_ERROR);
	token.text = pool_intern(pool, msg, strlen(msg));
	return token;
}

static bool is_at_end(Scanner *s) {
	return *s->current == '\0';
}

static char advance(Scanner *s) {
	s->current++;
	return s->current[-1];
}

static char peek(Scanner *s) {
	return *s->current;
}

static char peek_next(Scanner *s) {
	if (is_at_end(s)) return '\0';
	return s->current[1];
}

static bool match(Scanner *s, char expected) {
	if (is_at_end(s)) return false;
	if (peek(s) != expected) return false;
	advance(s);
	return true;
}

static void skip_whitespace(Scanner *s) {
	while (true) {
		char c = peek(s);

		switch (c) {
			case ' ':
			case '\r':
			case '\t':
				advance(s);
				break;
			case '\n':
				advance(s);
				s->line++;
				break;
			case '-': if (peek_next(s) == '-') {
				while (peek(s) != '\n' && !is_at_end(s)) advance(s);
				break;
			} else return;
			default: return;
		}
	}
}

static TokenKind get_identifier_kind(const char *start, u64 length) {
	for (ulong i = 0; i < KEYWORD_COUNT; i++) {
		Keyword *k = &keywords[i];
		if (
			strlen(k->word) == length &&
			memcmp(start, k->word, length) == 0
		) {
			return k->kind;
		}
	}

	return TOKEN_IDENTIFIER;
}

static Token identifier(Scanner *s, StringPool *pool) {
	const char *start = s->start;
	while (isalnum(*s->current) || *s->current == '_') s->current++;
	
	int length = s->current - s->start;
	
	TokenKind kind = get_identifier_kind(start, length);

	return make_token(s, pool, kind);
};

static Token number(Scanner *s, StringPool *pool) {
	while (isdigit(*s->current)) s->current++;
	if (match(s, '.')) while (isdigit(*s->current)) s->current++;

	return make_token(s, pool, TOKEN_NUMBER);
};

static Token string(Scanner *s, StringPool *pool, MemArena *scratch) {
	char quote = advance(s);

	u64 start_scratch = scratch->pos;

	while (peek(s) != quote && !is_at_end(s)) {
		char c = advance(s);

		if (c == '\\') {
			char esc = peek(s);

			if (isdigit(esc)) {
				int val = 0;
				for (int i = 0; i < 3 && isdigit(peek(s)); i++) {
					val = val * 10 + (advance(s) - '0');
				}

				arena_push_byte(scratch, (u8)val);
				continue;
			}

			switch (esc) {
				case 'a':  advance(s); arena_push_byte(scratch, '\a'); break;
				case 'b':  advance(s); arena_push_byte(scratch, '\b'); break;
				case 'f':  advance(s); arena_push_byte(scratch, '\f'); break;
				case 'n':  advance(s); arena_push_byte(scratch, '\n'); break;
				case 'r':  advance(s); arena_push_byte(scratch, '\r'); break;
				case 't':  advance(s); arena_push_byte(scratch, '\t'); break;
				case 'v':  advance(s); arena_push_byte(scratch, '\v'); break;
				case '\\': advance(s); arena_push_byte(scratch, '\\'); break;
				case '"':  advance(s); arena_push_byte(scratch, '"'); break;
				case '\'': advance(s); arena_push_byte(scratch, '\''); break;
				case '\n':
					advance(s);
					s->line++;
					break;
				default: arena_push_byte(scratch, advance(s)); break;
			}
		} else {
			if (c == '\n') s->line++;
			arena_push_byte(scratch, c);
		}
	}

	if (is_at_end(s)) return error_token(s, pool, "Unterminated string.");
	advance(s);

	arena_push_byte(scratch, '\0');

	char *text = (char*)((u8*)scratch + start_scratch);
	u64 len = scratch->pos - start_scratch - 1;

	Token token = make_empty_token(s, TOKEN_STRING);
	token.text = pool_intern(pool, text, len);
	arena_pop_to(scratch, start_scratch);
	return token;
}

Token scan_token(Scanner *s, StringPool *pool, MemArena *scratch) {
	skip_whitespace(s);

	s->start = s->current;

	if (is_at_end(s)) return make_token(s, pool, TOKEN_EOF);

	char c = advance(s);

	if (isalpha(c) || c == '_') return identifier(s, pool);
	if (isdigit(c)) return number(s, pool);

	#define TOKEN(KIND) make_token(s, pool, KIND)

	switch (c) {
		case '(': return TOKEN(TOKEN_LPAREN);
		case ')': return TOKEN(TOKEN_LPAREN);
		case '{': return TOKEN(TOKEN_LBRACE);
		case '}': return TOKEN(TOKEN_RBRACE);
		case '[': return TOKEN(TOKEN_LBRACK);
		case ']': return TOKEN(TOKEN_RBRACK);

		case ',': return TOKEN(TOKEN_COMMA);
		case ':': return TOKEN(TOKEN_COLON);
		case ';': return TOKEN(TOKEN_SEMICOLON);

		case '+': return TOKEN(TOKEN_PLUS);
		case '-': return TOKEN(TOKEN_MINUS);
		case '*': return TOKEN(TOKEN_STAR);
		case '/': return TOKEN(TOKEN_SLASH);
		case '%': return TOKEN(TOKEN_PERCENT);
		case '^': return TOKEN(TOKEN_CARET);
		case '#': return TOKEN(TOKEN_HASH);
		case '|': return TOKEN(TOKEN_PIPE);

		case '=': if (match(s, '=')) return TOKEN(TOKEN_EQ_EQ);
		          else return TOKEN(TOKEN_EQ);
		case '~': if (match(s, '=')) return TOKEN(TOKEN_NOT_EQ);
		          else return error_token(s, pool, "Unknown character.");
		case '<': if (match(s, '=')) return TOKEN(TOKEN_LTEQ);
		          else return TOKEN(TOKEN_LT);
		case '>': if (match(s, '=')) return TOKEN(TOKEN_GTEQ);
		          else return TOKEN(TOKEN_GT);

		case '.': if (match(s, '.')) {
								if (match(s, '.')) return TOKEN(TOKEN_DOT_DOT_DOT);
								else return TOKEN(TOKEN_DOT_DOT);
							} else return TOKEN(TOKEN_DOT);
		case '"':
		case '\'':
			s->current--;
			return string(s, pool, scratch);
	}

	return error_token(s, pool, "Unknown character.");
}

Token *tokenize(MemArena *perm, MemArena *scratch, StringPool *pool, char *source) {
	Token *temp_tokens = vec_new(Token, scratch);

	Scanner s = { source, source, 1 };

	while (!is_at_end(&s)) {
		Token token = scan_token(&s, pool, scratch);
		vec_append(&temp_tokens, token);
	}

	Token *final_tokens = vec_copy(temp_tokens, perm);
	arena_clear(scratch);

	return final_tokens;
}
