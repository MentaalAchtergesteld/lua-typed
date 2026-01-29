#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "parser.h"
#include "arena.h"
#include "token.h"
#include "vec.h"

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
	e->kind = EXPR_NUMBER;
	e->as.number  = strtod(previous(p).text, NULL);
	return e;
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

static Expr *call(Parser *p, Expr *left) {
	Expr *e = new_expr(p, EXPR_CALL);
	e->as.call.callee = left;

	Expr **args = NULL;
	while (!check(p, TOKEN_RPAREN) && !check(p, TOKEN_EOF)) {
		vec_push(args, parse_expression(p));
		if (!match(p, TOKEN_COMMA)) break;
	}
	consume(p, TOKEN_RPAREN, "Expected ')' after call arguments.");

	e->as.call.arg_count = vec_size(args);
	if (args) {
		e->as.call.args = PUSH_ARRAY(p->arena, Expr*, vec_size(args));
		memcpy(e->as.call.args, args, vec_size(args) * sizeof(Expr*));
		vec_free(args);
	} else e->as.call.args = NULL;

	return e;
}

static Expr *array(Parser *p, Expr *left) {
	Expr *e = new_expr(p, EXPR_INDEX);
	e->as.index.target = left;

	Expr *index = parse_expression(p);
	consume(p, TOKEN_RBRACK, "Expected ']' after array index.");
	e->as.index.index = index;
	return e;
}

static Expr *field(Parser *p, Expr *left) {
	Expr *e = new_expr(p, EXPR_FIELD);
	e->as.field.target = left;

	consume(p, TOKEN_IDENTIFIER, "Expected field name.");
	e->as.field.field = previous(p).text;
	return e;
}

static Expr *struct_init(Parser *p, Expr *left) {
	Expr *e = new_expr(p, EXPR_STRUCT);
	e->as.struct_init.name = left;

	TableEntry *entries= NULL;
	while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {

		Expr *key = parse_expression(p);
		consume(p, TOKEN_COLON, "Expected ':' after field name.");
		Expr *value = parse_expression(p);
		TableEntry entry = { key, value };
		vec_push(entries, entry);
		if (!match(p, TOKEN_COMMA)) break;
	}
	consume(p, TOKEN_RBRACE, "Expected '}' after struct init");

	e->as.struct_init.entry_count = vec_size(entries);
	if (entries) {
		e->as.struct_init.entries = PUSH_ARRAY(p->arena, TableEntry, vec_size(entries));
		memcpy(e->as.struct_init.entries, entries, vec_size(entries) * sizeof(TableEntry));
		vec_free(entries);
	} else e->as.struct_init.entries = NULL;

	return e;
}

ParseRule rules[] = {
	[TOKEN_LPAREN]    = {grouping, call,   PREC_CALL},
	[TOKEN_LBRACE]    = {NULL,     struct_init,PREC_CALL},
	[TOKEN_LBRACK]    = {NULL,     array,  PREC_CALL},
	[TOKEN_DOT]       = {NULL,     field,  PREC_CALL},
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
static Stmt *parse_statement(Parser *p);

static bool is_block_end(TokenKind kind) {
	return kind == TOKEN_END ||
				 kind == TOKEN_ELSE ||
	       kind == TOKEN_ELSEIF ||
	       kind == TOKEN_UNTIL ||
	       kind == TOKEN_EOF;
}

static FuncSignature *parse_func_signature(Parser *p);

static Type *parse_type(Parser *p) {
	Type *t = PUSH_STRUCT(p->arena, Type);

	switch(peek(p).kind) {
		case TOKEN_LBRACK:
			advance(p);
			t->kind = TYPE_ARRAY;
			t->as.array.inner = parse_type(p);
			consume(p, TOKEN_RBRACK, "Expected ']' after array type.");
			return t;
		case TOKEN_FUNCTION:
			advance(p);
			t->kind = TYPE_FUNCTION;
			t->as.function.sig = parse_func_signature(p);
			return t;
		case TOKEN_IDENTIFIER: {
			advance(p);
			const char *name = previous(p).text;

			if (strcmp(name, "void") == 0)   { t->kind = TYPE_VOID; return t; }
			if (strcmp(name, "nil") == 0)    { t->kind = TYPE_NIL; return t; }
			if (strcmp(name, "bool") == 0)   { t->kind = TYPE_BOOL; return t; }
			if (strcmp(name, "number") == 0) { t->kind = TYPE_NUMBER; return t; }
			if (strcmp(name, "string") == 0) { t->kind = TYPE_STRING; return t; }

			t->kind = TYPE_STRUCT;
			t->as.user_type.name = name;

			if (match(p, TOKEN_LT)) {
				Type **args = NULL;
				do {
					vec_push(args, parse_type(p));
	 			} while (match(p, TOKEN_COMMA));
				consume(p, TOKEN_GT, "Expected '>' after type arguments.");

				t->as.user_type.arg_count = vec_size(args);
				if (args) {
					t->as.user_type.args = PUSH_ARRAY(p->arena, Type*, vec_size(args));
					memcpy(t->as.user_type.args, args, vec_size(args) * sizeof(Type*));
					vec_free(args);
				} else t->as.user_type.args = NULL;
			}

			return t;
		}
		default:
			error_at(p, peek(p), "Expected type.");
			return NULL;
	}

	error_at(p, peek(p), "Expected type.");
	return NULL;
}

static GenericParam parse_generic(Parser *p) {
	consume(p, TOKEN_IDENTIFIER, "Expected generic name.");
	
	GenericParam gp = {0};
	gp.name = previous(p).text;

	if (match(p, TOKEN_COLON)) {
		Type **constraints = NULL;
		do {
			vec_push(constraints, parse_type(p));
	 	} while (match(p, TOKEN_PLUS));

		gp.constraint_count = vec_size(constraints);
		if (constraints) {
			gp.constraints = PUSH_ARRAY(p->arena, Type*, vec_size(constraints));
			memcpy(gp.constraints, constraints, gp.constraint_count * sizeof(Type*));
			vec_free(constraints);
		} else gp.constraints = NULL;
	}

	return gp;
}

static Param parse_param(Parser *p) {
	consume(p, TOKEN_IDENTIFIER, "Expected param name.");
	const char *name = previous(p).text;
	consume(p, TOKEN_COLON, "Expected ':' after param name.");
	Type *type = parse_type(p);

	Param param = {0};
	param.name = name;
	param.type = type;
	return param;
}

static Stmt *parse_block(Parser *p) {
	Stmt **list = NULL;

	while (!is_block_end(peek(p).kind)) {
		Stmt *stmt = parse_statement(p);
		vec_push(list, stmt);
	}

	Stmt *node = PUSH_STRUCT(p->arena, Stmt);
	node->kind = STMT_BLOCK;
	node->as.block.stmt_count = vec_size(list);

	if (list) {
		node->as.block.stmts = PUSH_ARRAY(p->arena, Stmt*, vec_size(list));
		memcpy(node->as.block.stmts, list, vec_size(list) * sizeof(Stmt*));
		vec_free(list);
	} else node->as.block.stmts = NULL;

	return node;
}

static GenericParam *parse_generics(Parser *p) {
	GenericParam *generics = NULL;
	if (match(p, TOKEN_LT)) {
		while (!check(p, TOKEN_GT) && !check(p, TOKEN_EOF)) {
			vec_push(generics, parse_generic(p));
			if (!match(p, TOKEN_COMMA)) break;
		}
		consume(p, TOKEN_GT, "Expected '>' after generic params.");
	}

	return generics;
}

static FuncSignature *parse_func_signature(Parser *p) {
	GenericParam *generics = parse_generics(p);

	consume(p, TOKEN_LPAREN, "Expected '(' before function params.");
	Param *params = NULL;

	while (!check(p, TOKEN_RPAREN) && !check(p, TOKEN_EOF)) {
		vec_push(params, parse_param(p));
		if (!match(p, TOKEN_COMMA)) break;
	}
	consume(p, TOKEN_RPAREN, "Expected ')' after function params.");

	Type **return_types = NULL;
	if (match(p, TOKEN_COLON)) {
		do {
			Type *type = parse_type(p);
			vec_push(return_types, type);
		} while (match(p, TOKEN_COMMA));
	}

	FuncSignature *sig = PUSH_STRUCT(p->arena, FuncSignature);
	sig->generic_count = vec_size(generics);
	if (generics) {
		sig->generics = PUSH_ARRAY(p->arena, GenericParam, vec_size(generics));
		memcpy(sig->generics, generics, vec_size(generics) * sizeof(GenericParam));
		vec_free(generics);
	} else sig->generics = NULL;

	sig->param_count = vec_size(params);
	if (params) {
		sig->params = PUSH_ARRAY(p->arena, Param, vec_size(params));
		memcpy(sig->params, params, vec_size(params) * sizeof(Param));
		vec_free(params);
	}

	sig->return_count = vec_size(return_types);
	if (return_types) {
		sig->return_types = PUSH_ARRAY(p->arena, Type*, vec_size(return_types));
		memcpy(sig->return_types, return_types, vec_size(return_types) * sizeof(Type*));
		vec_free(return_types);
	}

	return sig;
}

static Stmt *type_alias(Parser *p) {
	consume(p, TOKEN_TYPE, "Expected 'type'.");
	consume(p, TOKEN_IDENTIFIER, "Expected type alias name.");
	const char *name = previous(p).text;

	consume(p, TOKEN_EQ, "Expected '=' after type alias name.");

	Type *type = parse_type(p);
	consume(p, TOKEN_SEMICOLON, "Expected ';' after type alias.");

	Stmt *node = PUSH_STRUCT(p->arena, Stmt);
	node->kind = STMT_TYPE_ALIAS;
	node->as.type_alias.name = name;
	node->as.type_alias.type = type;

	return node;
}

static Stmt *function_decl(Parser *p) {
	consume(p, TOKEN_FUNCTION, "Expected 'function'.");
	consume(p, TOKEN_IDENTIFIER, "Expected function name.");

	const char *name = previous(p).text;

	FuncSignature *sig = parse_func_signature(p);

	Stmt *body = parse_block(p);
	consume(p, TOKEN_END, "Expected 'end' after function.");

	Stmt *node = PUSH_STRUCT(p->arena, Stmt);
	node->kind = STMT_FUNCTION;
	node->as.func_decl.name = name;
	node->as.func_decl.body = body;
	node->as.func_decl.signature = sig;
	return node;
}

static Stmt *impl_decl(Parser *p) {
	consume(p, TOKEN_IMPL, "Expected 'impl'.");
	GenericParam *generics = parse_generics(p);

	consume(p, TOKEN_IDENTIFIER, "Expected trait or struct name.");
	const char *first_name = previous(p).text;

	Type **first_args = NULL;
	if (match(p, TOKEN_LT)) {
		while (!match(p, TOKEN_GT) && !match(p, TOKEN_EOF)) {
			vec_push(first_args, parse_type(p));
			if (!match(p, TOKEN_COMMA)) break;
		}
		consume(p, TOKEN_GT, "Expected '>' after trait args.");
	}

	const char *second_name = NULL;
	Type **second_args = NULL;
	if (match(p, TOKEN_FOR)) {
		consume(p, TOKEN_IDENTIFIER, "Expected struct name.");
		second_name = previous(p).text;
		if (match(p, TOKEN_LT)) {
			while (!match(p, TOKEN_GT) && !match(p, TOKEN_EOF)) {
				vec_push(second_args, parse_type(p));
				if (!match(p, TOKEN_COMMA)) break;
			}
			consume(p, TOKEN_GT, "Expected '>' after trait args.");
		}
	}

	const char *trait_name = second_name ? first_name : NULL;
	const char *target_name = second_name ? second_name : first_name;

	Type **trait_args = second_name ? first_args : NULL;
	Type **target_args = second_name ? second_args : first_args;

	Stmt **functions = NULL;

	while (!check(p, TOKEN_END) && !check(p, TOKEN_EOF)) {
		vec_push(functions, function_decl(p));
	}

	consume(p, TOKEN_END, "Expected 'end' after impl.");

	Stmt *node = PUSH_STRUCT(p->arena, Stmt);
	node->kind = STMT_IMPL;

	node->as.impl_stmt.trait_name = trait_name;
	node->as.impl_stmt.target_name = target_name;

	node->as.impl_stmt.generic_count = vec_size(generics);
	if (generics) {
		node->as.impl_stmt.generics = PUSH_ARRAY(p->arena, GenericParam, vec_size(generics));
		memcpy(node->as.impl_stmt.generics, generics, vec_size(generics) * sizeof(GenericParam));
		vec_free(generics);
	} else node->as.impl_stmt.generics = NULL;

	node->as.impl_stmt.target_arg_count = vec_size(target_args);
	if (target_args) {
		node->as.impl_stmt.target_args = PUSH_ARRAY(p->arena, Type*, vec_size(target_args));
		memcpy(node->as.impl_stmt.target_args, target_args, vec_size(target_args) * sizeof(Type*));
	} else node->as.impl_stmt.target_args = NULL;

	node->as.impl_stmt.trait_arg_count = vec_size(trait_args);
	if (trait_args) {
		node->as.impl_stmt.trait_args = PUSH_ARRAY(p->arena, Type*, vec_size(trait_args));
		memcpy(node->as.impl_stmt.trait_args , trait_args, vec_size(trait_args) * sizeof(Type*));
	} else node->as.impl_stmt.trait_args = NULL;

	node->as.impl_stmt.func_count = vec_size(functions);
	if (functions) {
		node->as.impl_stmt.functions = PUSH_ARRAY(p->arena, Stmt*, vec_size(functions));
		memcpy(node->as.impl_stmt.functions, functions, vec_size(functions) * sizeof(Stmt*));
		vec_free(functions);
	} else node->as.impl_stmt.functions = NULL;

	vec_free(first_args);
	vec_free(second_args);

	return node;
}

static Stmt *trait_decl(Parser *p) {
	consume(p, TOKEN_TRAIT, "Expected 'trait'.");
	consume(p, TOKEN_IDENTIFIER, "Expected trait name.");

	const char *name = previous(p).text;

	GenericParam *generics = parse_generics(p);

	const char **func_names = NULL;
	FuncSignature **functions = NULL;

	while (!check(p, TOKEN_END) && !check(p, TOKEN_EOF)) {
		consume(p, TOKEN_FUNCTION, "Expected 'function' in trait declaration.");
		consume(p, TOKEN_IDENTIFIER, "Expected function name in trait declaration.");
		const char *name = previous(p).text;
		FuncSignature *sig = parse_func_signature(p);

		vec_push(func_names, name);
		vec_push(functions, sig);
	};
	consume(p, TOKEN_END, "Expected 'end' after trait declaration.");

	Stmt *node = PUSH_STRUCT(p->arena, Stmt);
	node->kind = STMT_TRAIT;
	node->as.trait_decl.name = name;

	node->as.trait_decl.generic_count = vec_size(generics);
	if (generics) {
		node->as.trait_decl.generics = PUSH_ARRAY(p->arena, GenericParam, vec_size(generics));
		memcpy(node->as.trait_decl.generics, generics, vec_size(generics) * sizeof(GenericParam));
		vec_free(generics);
	} else node->as.trait_decl.generics = NULL;

	node->as.trait_decl.func_count = vec_size(func_names);
	if (func_names) {
		node->as.trait_decl.func_names = PUSH_ARRAY(p->arena, const char*, vec_size(func_names));
		node->as.trait_decl.functions =  PUSH_ARRAY(p->arena, FuncSignature*, vec_size(functions));

		memcpy(node->as.trait_decl.func_names, func_names, vec_size(func_names) * sizeof(const char*));
		memcpy(node->as.trait_decl.functions,  functions,  vec_size(functions) * sizeof(FuncSignature*));

		vec_free(func_names);
		vec_free(functions);
	} else {
		node->as.trait_decl.func_names = NULL;
		node->as.trait_decl.functions = NULL;
	}

	return node;
}

static Stmt *struct_decl(Parser *p) {
	consume(p, TOKEN_STRUCT, "Expected 'struct'.");
	consume(p, TOKEN_IDENTIFIER, "Expected struct name.");

	const char *name = previous(p).text;

	GenericParam *generics = parse_generics(p);

	Param *fields = NULL;

	while (!check(p, TOKEN_END) && !check(p, TOKEN_EOF)) {
		vec_push(fields, parse_param(p));
		if (!match(p, TOKEN_COMMA)) break;
	}
	consume(p, TOKEN_END, "Expected 'end' after struct declaration.");

	Stmt *node = PUSH_STRUCT(p->arena, Stmt);
	node->kind = STMT_STRUCT;
	node->as.struct_decl.name = name;
	node->as.struct_decl.generic_count = vec_size(generics);
	if (generics) {
		node->as.struct_decl.generics = PUSH_ARRAY(p->arena, GenericParam, vec_size(generics));
		memcpy(node->as.struct_decl.generics, generics, vec_size(generics) * sizeof(GenericParam));
		vec_free(generics);
	} else node->as.struct_decl.generics = NULL;

	node->as.struct_decl.field_count = vec_size(fields);
	if (fields) {
		node->as.struct_decl.fields = PUSH_ARRAY(p->arena, Param, vec_size(fields));
		memcpy(node->as.struct_decl.fields, fields, vec_size(fields) * sizeof(Param));
		vec_free(fields);
	} else node->as.struct_decl.fields = NULL;

	return node;
}

static Stmt *local_decl(Parser *p) {
	consume(p, TOKEN_LOCAL, "Expected 'local'");

	Param *params = NULL;

	do {
		vec_push(params, parse_param(p));
	} while (match(p, TOKEN_COMMA));

	Expr **values = NULL;
	if (match(p, TOKEN_EQ)) {
		do {
			vec_push(values, parse_expression(p));
	 	} while (match(p, TOKEN_COMMA));
	}

	consume(p, TOKEN_SEMICOLON, "Expected ';' after local declaration.");

	Stmt *node = PUSH_STRUCT(p->arena, Stmt);
	node->kind = STMT_LOCAL;

	node->as.local.decl_count = vec_size(params);
	if (params) {
		node->as.local.decls = PUSH_ARRAY(p->arena, Param, vec_size(params));
		memcpy(node->as.local.decls, params, vec_size(params) * sizeof(Param));
		vec_free(params);
	} else node->as.local.decls = NULL;

	node->as.local.value_count = vec_size(values);
	if (values) {
		node->as.local.values = PUSH_ARRAY(p->arena, Expr*, vec_size(values));
		memcpy(node->as.local.values, values, vec_size(values) * sizeof(Expr*));
		vec_free(values);
	} else node->as.local.values = NULL;

	return node;
}

static Stmt *numeric_for(Parser *p) {
	const char *variable = previous(p).text;
	consume(p, TOKEN_EQ, "Expected '=' after variable name.");
	Expr *start = parse_expression(p);

	consume(p, TOKEN_COMMA, "Expected ',' after start value.");
	Expr *end = parse_expression(p);

	Expr *step = NULL;
	if (match(p, TOKEN_COMMA)) {
		step = parse_expression(p);
	}

	consume(p, TOKEN_DO, "Expected 'do' after for arguments.");
	Stmt *body = parse_block(p);
	consume(p, TOKEN_END, "Expected 'end' after for loop.");

	Stmt *node = PUSH_STRUCT(p->arena, Stmt);
	node->kind = STMT_FOR_NUM;
	node->as.for_num.name = variable;
	node->as.for_num.start = start;
	node->as.for_num.end = end;
	node->as.for_num.step = step;
	node->as.for_num.body = body;

	return node;
}

static Stmt *generic_for(Parser *p) {
	const char **names = NULL;

	vec_push(names, previous(p).text);

	while (match(p, TOKEN_COMMA)) {
		consume(p, TOKEN_IDENTIFIER, "Expected variable name.");
		const char *next_name = previous(p).text;
		vec_push(names, next_name);
	}

	consume(p, TOKEN_IN, "Expected 'in' after for loop variables.");
	Expr *iter = parse_expression(p);

	consume(p, TOKEN_DO, "Expectd 'do' after for loop iterator.");
	Stmt *body = parse_block(p);
	consume(p, TOKEN_END, "Expected 'end' after for loop.");

	Stmt *node = PUSH_STRUCT(p->arena, Stmt);
	node->kind = STMT_FOR_GEN;
	node->as.for_gen.iter = iter;
	node->as.for_gen.body = body;
	node->as.for_gen.name_count = vec_size(names);

	if (names) {
		node->as.for_gen.names = PUSH_ARRAY(p->arena, const char*, vec_size(names));
		memcpy(node->as.for_gen.names, names, vec_size(names) * sizeof(const char*));
		vec_free(names);
	}

	return node;
}

static Stmt *for_stmt(Parser *p) {
	consume(p, TOKEN_FOR, "Expected 'for'.");

	consume(p, TOKEN_IDENTIFIER, "Expected variable name after 'for'.");

	if (check(p, TOKEN_EQ)) return numeric_for(p);
	else return generic_for(p);
}

static Stmt *repeat_stmt(Parser *p) {
	consume(p, TOKEN_REPEAT, "Expected 'repeat'.");
	Stmt *body = parse_block(p);

	consume(p, TOKEN_UNTIL, "Expected 'until' after repeat body.");
	Expr *condition = parse_expression(p);

	Stmt *node = PUSH_STRUCT(p->arena, Stmt);
	node->kind = STMT_REPEAT;
	node->as.repeat_stmt.body = body;
	node->as.repeat_stmt.condition = condition;
	return node;
}

static Stmt *while_stmt(Parser *p) {
	consume(p, TOKEN_WHILE, "Expected 'while'.");

	Expr *condition = parse_expression(p);
	consume(p, TOKEN_DO, "Expected 'do' after while condition.");

	Stmt *body = parse_block(p);
	consume(p, TOKEN_END, "Expected 'end' after while statement.");

	Stmt *node = PUSH_STRUCT(p->arena, Stmt);
	node->kind = STMT_WHILE;
	node->as.while_stmt.condition = condition;
	node->as.while_stmt.body = body;
	return node;
}

static Stmt *if_stmt(Parser *p) {
	consume(p, TOKEN_IF, "Expected 'if'.");

	Stmt *root = PUSH_STRUCT(p->arena, Stmt);
	root->kind = STMT_IF;

	root->as.if_stmt.condition = parse_expression(p);
	consume(p, TOKEN_THEN, "Expected 'then' after if condition.");

	root->as.if_stmt.then_branch = parse_block(p);
	root->as.if_stmt.else_branch = NULL;

	Stmt **tail = &root->as.if_stmt.else_branch;

	while (match(p, TOKEN_ELSEIF)) {
		Stmt *elseif = PUSH_STRUCT(p->arena, Stmt);
		elseif->kind = STMT_IF;
		elseif->as.if_stmt.condition = parse_expression(p);
	consume(p, TOKEN_THEN, "Expected 'then' after elseif condition.");

		elseif->as.if_stmt.then_branch = parse_block(p);
		elseif->as.if_stmt.else_branch = NULL;

		*tail = elseif;
		tail = &elseif->as.if_stmt.else_branch;
	}

	if (match(p, TOKEN_ELSE)) {
		*tail = parse_block(p);
	}

	consume(p, TOKEN_END, "Expected 'end' after if statement.");

	return root;
}

static Stmt *break_stmt(Parser *p) {
	consume(p, TOKEN_BREAK, "Expected 'break'.");
	consume(p, TOKEN_SEMICOLON, "Expected ';' after break.");
	Stmt *node = PUSH_STRUCT(p->arena, Stmt);
	node->kind = STMT_BREAK;
	return node;
}

static Stmt *return_stmt(Parser *p) {
	consume(p, TOKEN_RETURN, "Expected 'return'.");

	Expr **list = NULL;

	while (!check(p, TOKEN_SEMICOLON) && !check(p, TOKEN_EOF)) {
		Expr *expr = parse_expression(p);
		vec_push(list, expr);
		if (!match(p, TOKEN_COMMA)) break;
	}	
	consume(p, TOKEN_SEMICOLON, "Expected ';' after return statement.");

	Stmt *node = PUSH_STRUCT(p->arena, Stmt);
	node->kind = STMT_RETURN;
	node->as.return_stmt.value_count = vec_size(list);

	if (list) {
		node->as.return_stmt.values = PUSH_ARRAY(p->arena, Expr*, vec_size(list));
		memcpy(node->as.return_stmt.values, list, vec_size(list) * sizeof(Expr*));
		vec_free(list);
	} else node->as.return_stmt.values = NULL;

	return node;
}

static Stmt *expression_or_assignment(Parser *p) {
	Expr **targets = NULL;

	do {
		vec_push(targets, parse_expression(p));
	} while (match(p, TOKEN_COMMA));

	if (match(p, TOKEN_EQ)) {
		Expr **values = NULL;
		do {
			vec_push(values, parse_expression(p));
		} while (match(p, TOKEN_COMMA));

		consume(p, TOKEN_SEMICOLON, "Expected ';' after assignment.");

		Stmt *node = PUSH_STRUCT(p->arena, Stmt);
		node->kind = STMT_ASSIGN;

		node->as.assign.target_count = vec_size(targets);
		if (targets) {
			node->as.assign.targets = PUSH_ARRAY(p->arena, Expr*, vec_size(targets));
			memcpy(node->as.assign.targets, targets, vec_size(targets) * sizeof(Expr*));
			vec_free(targets);
		} else node->as.assign.targets = NULL;

		node->as.assign.value_count = vec_size(values);
		if (values) {
			node->as.assign.values = PUSH_ARRAY(p->arena, Expr*, vec_size(values));
			memcpy(node->as.assign.values, values, vec_size(values) * sizeof(Expr*));
			vec_free(values);
		} else node->as.assign.values = NULL;

		return node;
	} else {
		if (vec_size(targets) > 1) {
			error_at(p, previous(p), "Unexpected ',' in expression statement.");
		}

		consume(p, TOKEN_SEMICOLON, "Expected ';' after expression.");

		Stmt *node = PUSH_STRUCT(p->arena, Stmt);
		node->kind = STMT_EXPR;
		node->as.expression = targets[0];
		vec_free(targets);
		return node;
	}
}

static Stmt *parse_statement(Parser *p) {
	switch(peek(p).kind) {
		case TOKEN_TYPE:     return type_alias(p);
		case TOKEN_IMPL:     return impl_decl(p);
		case TOKEN_TRAIT:    return trait_decl(p);
		case TOKEN_STRUCT:   return struct_decl(p);
		case TOKEN_FUNCTION: return function_decl(p);
		case TOKEN_LOCAL:    return local_decl(p);
		case TOKEN_FOR:      return for_stmt(p);
		case TOKEN_REPEAT:   return repeat_stmt(p);
		case TOKEN_WHILE:    return while_stmt(p);
		case TOKEN_IF:       return if_stmt(p);
		case TOKEN_BREAK:    return break_stmt(p);
		case TOKEN_RETURN:   return return_stmt(p);
		default: return expression_or_assignment(p);
	}
}

ParseResult parse(Token *tokens, MemArena *arena) {
	Parser parser = {
		tokens, 0, vec_size(tokens),
		arena, false, false
	};

	Stmt *root = parse_block(&parser);

	ParseResult result;
	result.root = root;
	result.success = !parser.had_error;

	return result;
}
