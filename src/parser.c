#include "parser.h"
#include "arena.h"
#include "token.h"
#include "vec.h"
#include <stdio.h>

typedef struct {
	Token *tokens;
	int current;
	int count;

	MemArena *arena;
	MemArena *scratch;
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

static bool check(Parser *p, TokenKind expected) {
	return peek(p).kind == expected;
}

static bool match(Parser *p, TokenKind expected) {
	if (check(p, expected)) {
		advance(p);
		return true;
	}
	return false;
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

static Expr *parse_type(Parser *p) {
	// temporary
	return parse_expression(p);
}

static Param *parse_param(Parser *p) {
	consume(p, TOKEN_IDENTIFIER, "Expected param name.");
	const char *name = previous(p).text;

	consume(p, TOKEN_SEMICOLON, "Expected ':' in param.");
	Expr *type = parse_type(p);

	Param *param = PUSH_STRUCT(p->arena, Param);
	param->name = name;
	param->type = type;
	return param;
}

static Stmt *parse_statement(Parser *p);

static Stmt *typeAlias(Parser *p) {
	consume(p, TOKEN_TYPE, "Expected 'type' in type alias declaration.");
	consume(p, TOKEN_IDENTIFIER, "Expected type alias name.");
	const char *name = previous(p).text;

	consume(p, TOKEN_EQ, "Expected '=' after type alias name.");
	match(p, TOKEN_SEMICOLON);

	Expr *type = parse_type(p);

	Stmt *stmt = PUSH_STRUCT(p->arena, Stmt);
	stmt->kind = STMT_TYPE_ALIAS;
	stmt->as.type_alias.name = name;
	stmt->as.type_alias.type = type;

	return stmt;
}
static Stmt *implDecl(Parser *p) {}

static Stmt *traitDecl(Parser *p) {}

static Stmt *structDecl(Parser *p) {}

static Stmt *functionDecl(Parser *p) {}

static Stmt *forStmt(Parser *p) {}

static Stmt *repeatStmt(Parser *p) {}

static Stmt *whileStmt(Parser *p) {}

static Stmt *ifStmt(Parser *p) {}

static Stmt *localDecl(Parser *p) {
	consume(p, TOKEN_LOCAL, "Expected 'local' in local declaration.");

	Param *vars = vec_new(Param, p->scratch);

	while (!check(p, TOKEN_SEMICOLON) && !check(p, TOKEN_EQ) && !check(p, TOKEN_EOF)) {
		vec_append(&vars, *parse_param(p));

		if (!match(p, TOKEN_COMMA)) break;
	}

	Param *final_vars = vec_copy(vars, p->arena);
	arena_clear(p->scratch);

	Stmt *stmt = PUSH_STRUCT(p->arena, Stmt);
	stmt->kind = STMT_LOCAL;
	stmt->as.local.decls = final_vars;

	if (!match(p, TOKEN_EQ) || match(p, TOKEN_SEMICOLON)) return stmt;


	Expr *values = vec_new(Expr, p->scratch);

	while (!check(p, TOKEN_SEMICOLON) && !check(p, TOKEN_EOF)) {
		vec_append(&values, *parse_expression(p));
		if (!match(p, TOKEN_COMMA)) break;
	}

	Expr *final_values = vec_copy(vars, p->arena);
	arena_clear(p->scratch);

	stmt->as.local.values = final_values;
	return stmt;
}

static Stmt *breakStmt(Parser *p) {
	consume(p, TOKEN_BREAK, "Expected 'break' in break statement.");
	Stmt *stmt = PUSH_STRUCT(p->arena, Stmt);
	stmt->kind = STMT_BREAK;
	return stmt;
}

static Stmt *returnStmt(Parser *p) {
	consume(p, TOKEN_RETURN, "Expected 'return' in return statement.");

	Expr *values = vec_new(Expr, p->scratch);

	while (!check(p, TOKEN_SEMICOLON) && !check(p, TOKEN_EOF)) {
		vec_append(&values, *parse_expression(p));
		if (!match(p, TOKEN_COMMA))	break;
	}
	consume(p, TOKEN_SEMICOLON, "Expected ';' after return statement.");

	Expr *final_values = vec_copy(values, p->arena);
	arena_clear(p->scratch);

	Stmt *stmt = PUSH_STRUCT(p->arena, Stmt);
	stmt->kind = STMT_RETURN;
	stmt->as.return_stmt.values = final_values;
	return stmt;
}

static Stmt *block(Parser *p) {
	consume(p, TOKEN_LBRACE, "Expected '{' at start of block.");

	Stmt *stmts = vec_new(Stmt, p->scratch);

	while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
		Stmt *stmt = parse_statement(p);
		vec_append(&stmts, *parse_statement(p));
	}

	consume(p, TOKEN_RBRACE, "Expected '}' at end of block.");

	Stmt *final_stmts = vec_copy(stmts, p->arena);
	arena_clear(p->scratch);

	Stmt *stmt = PUSH_STRUCT(p->arena, Stmt);
	stmt->kind = STMT_BLOCK;
	stmt->as.block.stmts = final_stmts;
	return stmt;
}

static Stmt *expressionStmt(Parser *p) {
	Expr *expr = parse_expression(p);
	match(p, TOKEN_SEMICOLON);

	Stmt *stmt = PUSH_STRUCT(p->arena, Stmt);
	stmt->kind = STMT_EXPR;
	stmt->as.expression = expr;
	return stmt;
}

static Stmt *parse_statement(Parser *p) {
	switch(peek(p).kind) {
		case TOKEN_TYPE:     return typeAlias(p);
		case TOKEN_IMPL:     return implDecl(p);
		case TOKEN_TRAIT:    return traitDecl(p);
		case TOKEN_STRUCT:   return structDecl(p);
		case TOKEN_FUNCTION: return functionDecl(p);
		case TOKEN_FOR:      return forStmt(p);
		case TOKEN_REPEAT:   return repeatStmt(p);
		case TOKEN_WHILE:    return whileStmt(p);
		case TOKEN_IF:       return ifStmt(p);
		case TOKEN_LOCAL:    return localDecl(p);
		case TOKEN_BREAK:    return breakStmt(p);
		case TOKEN_RETURN:   return returnStmt(p);
		case TOKEN_LBRACE:   return block(p);
		default: return expressionStmt(p);
	}
}
