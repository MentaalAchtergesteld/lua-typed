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
	{"impl",     TOKEN_IMPL},
	{"trait",    TOKEN_TRAIT},
	{"struct",   TOKEN_STRUCT},
};

#define KEYWORD_COUNT (sizeof(keywords)/sizeof(keywords[0]))

typedef struct {
	const char *start;
	const char *current;
	u64 line;
	StringPool *pool;
} Scanner;

static inline Token make_empty_token(Scanner *s, TokenKind kind) {
	Token token;
	token.kind = kind;
	token.start = s->start;
	token.length = (u64)(s->current-s->start);
	token.line = s->line;
	return token;
}

static inline Token make_token(Scanner *s, TokenKind kind) {
	Token token = make_empty_token(s, kind);
	token.text = pool_intern(s->pool, token.start, token.length);
	return token;
}

static inline Token error_token(Scanner *s, char *msg) {
	Token token = make_empty_token(s, TOKEN_ERROR);
	token.text = pool_intern(s->pool, msg, strlen(msg));
	return token;
}

#define is_at_end(s) (*(s)->current == '\0')
#define peek(s) (*(s)->current)
#define peek_next(s) (is_at_end(s) ? '\0' : (s)->current[1])

static char advance(Scanner *s) {
	s->current++;
	return s->current[-1];
}

static bool match(Scanner *s, char expected) {
	if (is_at_end(s)) return false;
	if (peek(s) != expected) return false;
	advance(s);
	return true;
}

static int scan_opening_level(Scanner *s) {
	int level = 0;
	const char *temp = s->current;

	while (*temp == '=') {
		level++;
		temp++;
	}

	if (*temp != '[') return -1;
	for (int i = 0; i < level + 1; i++) advance(s);
	if (peek(s) == '\r') advance(s);
	if (peek(s) == '\n') { advance(s); s->line++; }
	return level;
}

static bool scan_closing(Scanner *s, int level) {
	if (peek(s) != ']') return false;

	const char *temp = s->current+1;

	for (int i = 0; i < level; i++) {
		if (temp[i] != '=') return false;
	}

	if (temp[level] != ']') return false;

	advance(s);
	for (int i = 0; i < level; i++) advance(s);
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
				advance(s); advance(s);

				if (peek(s) == '[') {
					advance(s);
					int level = scan_opening_level(s);
					if (level >= 0) {
						while (!scan_closing(s, level) && !is_at_end(s)) {
							if (peek(s) == '\n') s->line++;
							advance(s);
						}
						break;
					}
				}
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

static Token identifier(Scanner *s) {
	const char *start = s->start;
	while (isalnum(*s->current) || *s->current == '_') s->current++;
	
	int length = s->current - s->start;
	
	TokenKind kind = get_identifier_kind(start, length);

	return make_token(s, kind);
};

static Token number(Scanner *s) {
	while (isdigit(*s->current)) s->current++;
	if (match(s, '.')) while (isdigit(*s->current)) s->current++;

	return make_token(s, TOKEN_NUMBER);
};

static Token string(Scanner *s) {
	char quote = advance(s);

	char *buf = NULL;
	while (peek(s) != quote && !is_at_end(s)) {
		char c = advance(s);

		if (c == '\\') {
			char esc = peek(s);

			if (isdigit(esc)) {
				int val = 0;
				for (int i = 0; i < 3 && isdigit(peek(s)); i++) {
					val = val * 10 + (advance(s) - '0');
				}

				char byte = (char)val;
				vec_push(buf, byte);
				continue;
			}

			switch (esc) {
				case 'a':  advance(s); { char b = '\a'; vec_push(buf, b); } break;
				case 'b':  advance(s); { char b = '\b'; vec_push(buf, b); } break;
				case 'f':  advance(s); { char b = '\f'; vec_push(buf, b); } break;
				case 'n':  advance(s); { char b = '\n'; vec_push(buf, b); } break;
				case 'r':  advance(s); { char b = '\r'; vec_push(buf, b); } break;
				case 't':  advance(s); { char b = '\t'; vec_push(buf, b); } break;
				case 'v':  advance(s); { char b = '\v'; vec_push(buf, b); } break;
				case '\\': advance(s); { char b = '\\'; vec_push(buf, b); } break;
				case '"':  advance(s); { char b = '"';  vec_push(buf, b); } break;
				case '\'': advance(s); { char b = '\''; vec_push(buf, b); } break;
				case '\n': advance(s); s->line++; { char b = '\n'; vec_push(buf, b); } break;
				default: vec_push(buf, advance(s)); break;
			}
		} else {
			if (c == '\n') s->line++;
			vec_push(buf, c);
		}
	}

	if (is_at_end(s)) {
		vec_free(buf);
		return error_token(s, "Unterminated string.");
	}

	advance(s);
	vec_push(buf, '\0');

	Token token = make_empty_token(s, TOKEN_STRING);
	token.text = pool_intern(s->pool, buf, vec_size(buf)-1);

	vec_free(buf);
	return token;
}

Token long_string(Scanner *s, int level) {
	char *buf = NULL;

	while (!scan_closing(s, level) && !is_at_end(s)) {
		char c = advance(s);
		if (c == '\n') s->line++;
		vec_push(buf, c);
	}

	if (is_at_end(s)) {
		vec_free(buf);
		return error_token(s, "Unterminated string.");
	}

	vec_push(buf, '\0');

	Token token = make_empty_token(s, TOKEN_STRING);
	token.text = pool_intern(s->pool, buf, vec_size(buf)-1);

	vec_free(buf);
	return token;
}

Token scan_token(Scanner *s) {
	skip_whitespace(s);

	s->start = s->current;

	if (is_at_end(s)) return make_token(s, TOKEN_EOF);

	char c = advance(s);

	if (isalpha(c) || c == '_') return identifier(s);
	if (isdigit(c)) return number(s);

	#define TOKEN(KIND) make_token(s, KIND)

	switch (c) {
		case '(': return TOKEN(TOKEN_LPAREN);
		case ')': return TOKEN(TOKEN_LPAREN);
		case '{': return TOKEN(TOKEN_RBRACE);
		case '}': return TOKEN(TOKEN_RBRACE);
		case '[': {
			int level = scan_opening_level(s);
			if (level == -1) return TOKEN(TOKEN_LBRACK);
			return long_string(s, level);
		}
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
		          else return error_token(s, "Unknown character.");
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
			return string(s);
	}

	return error_token(s, "Unknown character");
}

Token *tokenize(char *source, StringPool *pool) {
	Token *tokens = NULL; 

	Scanner s = { source, source, 1, pool };

	while (!is_at_end(&s)) {
		Token t = scan_token(&s);
		vec_push(tokens, t);
		if (t.kind == TOKEN_EOF) break;
	}

	if (tokens && tokens[vec_size(tokens)-1].kind != TOKEN_EOF) {
		Token eof = make_empty_token(&s, TOKEN_EOF);
		vec_push(tokens, eof);
	}

	return tokens;
}
