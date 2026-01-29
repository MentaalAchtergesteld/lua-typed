#pragma once
#include "arena.h"
#include "token.h"
#include <stdbool.h>

typedef struct Type Type;
typedef struct Expr Expr;
typedef struct Stmt Stmt;

typedef struct {
	const char *name;
	Type *type;
} Param;
#define PARAM(name, type) (Param){ name, type }

typedef struct {
	const char *name;
	Type **constraints;
	int constraint_count;
} GenericParam;
#define GENERIC_PARAM(name, constraints) (GenericParam){ name, constraints }

typedef struct {
	GenericParam *generics; int generic_count;
	Param *params; int param_count;
	Type **return_types; int return_count;
} FuncSignature;
#define FUNC_SIGNATURE(generics, params, return_types) (FuncSignature){ generics, params, return_types }

typedef struct {
	Expr *key;
	Expr *value;
} TableEntry;
#define TABLE_ENTRY(key, value) (TableEntry){ key, value }

typedef enum {
	TYPE_VOID, TYPE_NIL, TYPE_BOOL, TYPE_NUMBER, TYPE_STRING,
	TYPE_STRUCT,
	TYPE_TRAIT,
	TYPE_GENERIC,
	TYPE_FUNCTION,
	TYPE_ARRAY
} TypeKind;

struct Type {
	TypeKind kind;

	union {
		const char *param_name;
		struct {
			const char *name;
			Type **args; int arg_count;
		} user_type;

		struct {
			Type *inner;
		} array;

		struct {
			FuncSignature *sig;
		} function;
	} as;
};

typedef enum {
	OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_POW, OP_CONCAT,
	OP_EQ, OP_NEQ, OP_LT, OP_LTE, OP_GT, OP_GTE, OP_AND, OP_OR
} BinaryOp;

typedef enum {
	OP_NEGATE, OP_NOT, OP_LEN
} UnaryOp;

typedef enum {
	EXPR_NIL, EXPR_BOOL, EXPR_NUMBER, EXPR_STRING,
	EXPR_VARARG, EXPR_VARIABLE,
	EXPR_BINARY, EXPR_UNARY,
	EXPR_CALL, EXPR_INDEX, EXPR_FIELD,
	EXPR_FUNCTION, EXPR_TABLE, EXPR_STRUCT
} ExprKind;

struct Expr {
	ExprKind kind;

	union {
		bool boolean;
		double number;
		const char *string;
		const char *variable;

		struct { Expr *left; Expr *right; BinaryOp op; } binary;
		struct { Expr *operand; UnaryOp op; } unary;

		struct { Expr *callee; Expr **args; int arg_count; } call;
		struct { Expr *target; Expr *index; } index;
		struct { Expr *target; const char *field; } field;

		struct { FuncSignature signature; Stmt *body; } function;
		struct { TableEntry *entries; int entry_count; } table;
		struct { Expr *name; TableEntry *entries; int entry_count; } struct_init;
	} as;
};

typedef enum {
	STMT_EXPR, STMT_BLOCK, STMT_RETURN, STMT_BREAK,
	STMT_ASSIGN, STMT_LOCAL,
	STMT_IF, STMT_WHILE, STMT_REPEAT, STMT_FOR_NUM, STMT_FOR_GEN,
	STMT_FUNCTION, STMT_STRUCT, STMT_TRAIT, STMT_IMPL, STMT_TYPE_ALIAS,
} StmtKind;

struct Stmt {
	StmtKind kind;

	union {
		Expr *expression;
		struct { Stmt **stmts; int stmt_count; } block;
		struct { Expr **values; int value_count; } return_stmt;

		struct { Expr **targets; int target_count; Expr **values; int value_count; } assign;
		struct { Param *decls; int decl_count; Expr **values; int value_count; } local;

		struct { Expr *condition; Stmt *then_branch; Stmt *else_branch; } if_stmt;
		struct { Expr *condition; Stmt *body; } while_stmt;
		struct { Stmt *body; Expr *condition; } repeat_stmt;
		struct {
			const char *name; Expr *start; Expr *end; Expr *step; Stmt *body;
		} for_num;
		struct {
			const char **names; int name_count;
			Expr *iter;
			Stmt *body;
		} for_gen;

		struct {
			const char *name;
			FuncSignature *signature;
			Stmt *body;
		} func_decl;

		struct {
			const char *name;
			GenericParam *generics; int generic_count;
			Param *fields; int field_count;
		} struct_decl;

		struct {
			const char *name;
			GenericParam *generics; int generic_count;
			const char **func_names;
			FuncSignature **functions;
			int func_count;
		} trait_decl;

		struct {
			GenericParam *generics; int generic_count;

			const char *target_name;
			Type **target_args; int target_arg_count;

			const char *trait_name;
			Type **trait_args; int trait_arg_count;

			Stmt **functions;
			int func_count;
		} impl_stmt;

		struct { const char *name; Type *type; } type_alias;
	} as;
};

typedef struct {
	Stmt *root;
	bool success;
} ParseResult;

ParseResult parse(Token *tokens, MemArena *arena);
