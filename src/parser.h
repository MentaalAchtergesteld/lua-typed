#pragma once
#include <stdbool.h>

typedef struct Expr Expr;
typedef struct Stmt Stmt;

typedef enum {
	OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_POW, OP_CONCAT,
	OP_EQ, OP_NEQ, OP_LT, OP_LTE, OP_GT, OP_GTE, OP_AND, OP_OR
} BinaryOp;

typedef enum {
	OP_NEGATE, OP_NOT, OP_LEN
} UnaryOp;

typedef struct {
	const char *name;
	Expr *type;
} Param;
#define PARAM(name, type) (Param){ name, type }

typedef struct {
	const char *name;
	Expr *constraints;
} GenericParam;
#define GENERIC_PARAM(name, constraints) (GenericParam){ name, constraints }

typedef struct {
	GenericParam *generics;
	Param *params;
	Expr *return_types;
} FuncSignature;
#define FUNC_SIGNATURE(generics, params, return_types) (FuncSignature){ generics, params, return_types }

typedef struct {
	Expr *key;
	Expr *value;
} TableEntry;
#define TABLE_ENRTY(key, value) (TableEntry){ key, value }

typedef enum {
	EXPR_NIL, EXPR_BOOL, EXPR_NUMBER, EXPR_STRING,
	EXPR_VARARG, EXPR_VARIABLE,
	EXPR_BINARY, EXPR_UNARY,
	EXPR_CALL, EXPR_INDEX, EXPR_FIELD,
	EXPR_FUNCTION, EXPR_TABLE, EXPR_STRUCT
} ExprKind;

#define EXPR_HEADER ExprKind kind

struct Expr {
	ExprKind kind;

	union {
		bool boolean;
		double number;
		const char *string;
		const char *variable;

		struct { Expr *left; Expr *right; BinaryOp op; } binary;
		struct { Expr *operand; UnaryOp op; } unary;

		struct { Expr *callee; Expr *args; } call;
		struct { Expr *target; Expr *index; } index;
		struct { Expr *target; const char *field; } field;

		struct { FuncSignature signature; Stmt *body; } function;
		struct { TableEntry *entries; } table;
		struct { const char *name; TableEntry *entries; } struct_init;
	} as;
};

typedef enum {
	STMT_EXPR, STMT_BLOCK, STMT_RETURN, STMT_BREAK,
	STMT_ASSIGN, STMT_LOCAL,
	STMT_IF, STMT_WHILE, STMT_REPEAT, STMT_FOR_NUM, STMT_FOR_GEN,
	STMT_FUNCTION, STMT_STRUCT, STMT_TRAIT, STMT_IMPL, STMT_TYPE_ALIAS,
} StmtKind;

#define STMT_HEADER StmtKind kind

struct Stmt {
	StmtKind kind;

	union {
		Expr *expression;
		struct { Stmt *stmts; } block;
		struct { Expr *values; } return_stmt;

		struct { Expr *targets; Expr *values; } assign;
		struct { Param *decls; Expr *values; } local;

		struct { Expr *condition; Stmt *then_branch; Stmt *else_branch; } if_stmt;
		struct { Expr *condition; Stmt *body; } while_stmt;
		struct { Stmt *body; Expr *condition; } repeat_stmt;
		struct {
			const char *name; Expr *start; Expr *end; Expr *step; Stmt *body;
		} for_num;
		struct {
			const char *names; Expr *iter; Stmt *body;
		} for_gen;

		struct {
			const char *name;
			FuncSignature *signature;
			Stmt *body;
		} func_decl;

		struct {
			const char *name;
			GenericParam *generics;
			Param *fields;
		} struct_decl;

		struct {
			const char *name;
			GenericParam *generics;
			const char *func_names;
			FuncSignature *functions;
		} trait_decl;

		struct {
			const char *target_name;
			GenericParam *generics;

			const char *trait_name;
			Expr *trait_args;

			Stmt *functions;
		} impl_stmt;

		struct { const char *name; Expr *type; } type_alias;
	} as;

};
