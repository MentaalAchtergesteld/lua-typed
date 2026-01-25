#include "parser.h"
#include "arena.h"
#include "token.h"
#include <stdio.h>

typedef struct {
	Token *tokens;
	int current;
	int count;

	MemArena *arena;
	bool panic_mode;
	bool had_error;
} Parser;

static void error_at(Parser *p, Token t, const char *msg) {
	if (p->panic_mode) return;
	p->panic_mode = true;
	p->had_error = true;
	fprintf(stderr, "[line %d] Error at '%s': %s\n", (int)t.line, t.text, msg);
}

#define peek(p) ((p)->tokens[(p)->current])
#define previous(p) ((p)->tokens[(p)->current-1])

static Token advance(Parser *p) {
	if (p->current < p->count) p->current++;
	return previous(p);
}

static void consume(Parser *p, TokenKind expected, const char *msg) {
	if (peek(p).kind == expected) { advance(p); return; }
	error_at(p, peek(p), msg);
}

typedef enum {
	PREC_NONE,
	PREC_OR,
	PREC_AND,
	PREC_COMPARISON,
	PREC_CONCAT,
	PREC_TERM,
	PREC_FACTOR,
	PREC_UNARY,
	PREC_POW,
	PREC_CALL,
	PREC_PRIMARY
} Precedence;

typedef Expr *(*ParsePrefixFn)(Parser *p);
typedef Expr *(*ParseInfixFn)(Parser *p, Expr *left);

typedef struct {
	ParsePrefixFn prefix;
	ParseInfixFn infix;
	Precedence precedence;
} ParseRule;

ParseRule rules[];
#define prec_rule(kind) (&rules[(kind)])

static Expr *parse_precedence(Parser *p, Precedence precedence);
static Expr *parse_expression(Parser *p);

static Expr *new_expr(Parser *p, ExprKind kind) {
	Expr *e = PUSH_STRUCT(p->arena, Expr);
	e->kind = kind;
	return e;
}

static BinaryOp get_binary_op(TokenKind kind) {
	switch (kind) {
		case TOKEN_PLUS:    return OP_ADD;
		case TOKEN_MINUS:   return OP_SUB;
		case TOKEN_STAR:    return OP_MUL;
		case TOKEN_SLASH:   return OP_DIV;
		case TOKEN_PERCENT: return OP_MOD;
		case TOKEN_CARET:   return OP_POW;
		
		case TOKEN_DOT_DOT: return OP_CONCAT;

		case TOKEN_EQ:      return OP_EQ;
		case TOKEN_NOT_EQ:  return OP_NEQ;
		case TOKEN_LT:      return OP_LT;
		case TOKEN_LTEQ:    return OP_LTE;
		case TOKEN_GT:      return OP_GT;
		case TOKEN_GTEQ:    return OP_GTE;

		case TOKEN_AND:     return OP_AND;
		case TOKEN_OR:      return OP_OR;
		default: return -1;
	}
}

static UnaryOp get_unary_op(TokenKind kind) {
	switch (kind) {
		case TOKEN_MINUS: return OP_NEGATE;
		case TOKEN_NOT:   return OP_NOT;
		case TOKEN_HASH:  return OP_LEN;
		default: return -1;
	}
}

static Expr *number(Parser *p) {
	Expr *e = new_expr(p, EXPR_NUMBER);
}

static Expr *string(Parser *p) {
	Expr *e = new_expr(p, EXPR_STRING);
	e->as.string = previous(p).text;
	return e;
}

static Expr *literal(Parser *p) {
	switch (previous(p).kind) {
		case TOKEN_NIL:         return new_expr(p, EXPR_NIL);
		case TOKEN_TRUE:  { Expr *e = new_expr(p, EXPR_BOOL); e->as.boolean = true; return e; };
		case TOKEN_FALSE: { Expr *e = new_expr(p, EXPR_BOOL); e->as.boolean = false; return e; };
		case TOKEN_DOT_DOT_DOT: return new_expr(p, EXPR_VARARG);
		default: return NULL;
	}
}

static Expr *variable(Parser *p) {
	Expr *e = new_expr(p, EXPR_VARIABLE);
	e->as.variable = previous(p).text;
	return e;
}

static Expr *grouping(Parser *p) {
	Expr *e = parse_expression(p);
	consume(p, TOKEN_RPAREN, "Expected ')' after expression.");
	return e;
}

static Expr *unary(Parser *p) {
	TokenKind op_token = previous(p).kind;
	Expr *operand = parse_precedence(p, PREC_UNARY);

	Expr *e = new_expr(p, EXPR_UNARY);
	e->as.unary.op = get_unary_op(op_token);
	e->as.unary.operand = operand;
	return e;
}

static Expr *binary(Parser *p, Expr *left) {
	TokenKind op_token = previous(p).kind;
	ParseRule *rule = prec_rule(op_token);

	int next_prec = rule->precedence;
	if (op_token != TOKEN_CARET && op_token != TOKEN_DOT_DOT) {
		next_prec++;
	}

	Expr *right = parse_precedence(p, next_prec);

	Expr *e = new_expr(p, EXPR_BINARY);
	e->as.binary.op = get_binary_op(op_token);
	e->as.binary.left = left;
	e->as.binary.right = right;

	return e;
}

ParseRule rules[] = {
	[TOKEN_LPAREN]    = {grouping, NULL,   PREC_NONE},
	[TOKEN_MINUS]     = {unary,    binary, PREC_TERM},
	[TOKEN_PLUS]      = {NULL,     binary, PREC_TERM},
	[TOKEN_SLASH]     = {NULL,     binary, PREC_FACTOR},
	[TOKEN_STAR]      = {NULL,     binary, PREC_FACTOR},
	[TOKEN_PERCENT]   = {NULL,     binary, PREC_FACTOR},
	[TOKEN_CARET]     = {NULL,     binary, PREC_POW},
	[TOKEN_DOT_DOT]   = {NULL,     binary, PREC_CONCAT},
	
	[TOKEN_NUMBER]    = {number,   NULL,   PREC_NONE},
	[TOKEN_STRING]    = {string,   NULL,   PREC_NONE},
	[TOKEN_IDENTIFIER]= {variable, NULL,   PREC_NONE},
	
	[TOKEN_NIL]       = {literal,  NULL,   PREC_NONE},
	[TOKEN_TRUE]      = {literal,  NULL,   PREC_NONE},
	[TOKEN_FALSE]     = {literal,  NULL,   PREC_NONE},
	[TOKEN_DOT_DOT_DOT]={literal,  NULL,   PREC_NONE},
	
	[TOKEN_NOT]       = {unary,    NULL,   PREC_NONE},
	[TOKEN_HASH]      = {unary,    NULL,   PREC_NONE}, 
	
	[TOKEN_EQ_EQ]     = {NULL,     binary, PREC_COMPARISON},
	[TOKEN_NOT_EQ]    = {NULL,     binary, PREC_COMPARISON},
	[TOKEN_LT]        = {NULL,     binary, PREC_COMPARISON},
	[TOKEN_GT]        = {NULL,     binary, PREC_COMPARISON},
	[TOKEN_LTEQ]      = {NULL,     binary, PREC_COMPARISON},
	[TOKEN_GTEQ]      = {NULL,     binary, PREC_COMPARISON},
	
	[TOKEN_AND]       = {NULL,     binary, PREC_AND},
	[TOKEN_OR]        = {NULL,     binary, PREC_OR},
};

static Expr *parse_precedence(Parser *p, Precedence precedence) {
	advance(p);

	ParsePrefixFn prefix = prec_rule(previous(p).kind)->prefix;
	if (prefix == NULL) {
		error_at(p, previous(p), "Expected expression.");
		return NULL;
	}

	Expr *left = prefix(p);

	while (precedence <= prec_rule(peek(p).kind)->precedence) {
		advance(p);
		ParseInfixFn infix = prec_rule(previous(p).kind)->infix;
		left = infix(p, left);
	}

	return left;
}

static Expr *parse_expression(Parser *p) {
	return parse_precedence(p, PREC_OR);
}
