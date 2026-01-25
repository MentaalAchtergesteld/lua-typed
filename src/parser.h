#pragma once
#include <stdbool.h>

typedef struct Expr Expr;
typedef struct Stmt Stmt;

typedef enum {
	OP_ADD,
	OP_SUB,
	OP_MUL,
	OP_DIV,
	OP_MOD,
	OP_POW,

	OP_CONCAT,

	OP_EQ,
	OP_NEQ,
	OP_LT,
	OP_LTE,
	OP_GT,
	OP_GTE,

	OP_AND,
	OP_OR
} BinaryOp;

typedef enum {
	OP_NEGATE,
	OP_NOT,
	OP_LEN,
} UnaryOp;

typedef enum {
	EXPR_NIL,
	EXPR_BOOL,
	EXPR_NUMBER,
	EXPR_STRING,
	EXPR_VARARG,
	EXPR_VARIABLE,
	EXPR_CALL,
	EXPR_INDEX,
	EXPR_BINARY,
	EXPR_UNARY,
	EXPR_FUNCTION,
	EXPR_TABLE,
} ExprKind;

typedef struct {
	Expr *key;
	Expr *value;
} TableEntry;

struct Expr {
	ExprKind kind;

	union {
		bool boolean;
		double number;
		const char *string;
		const char *variable;

		struct {
			BinaryOp op;
			Expr *left;
			Expr *right;
		} binary;

		struct {
			UnaryOp op;
			Expr *operand;
		} unary;

		struct {
			Expr *target;
			Expr *key;
		} index;

		struct {
			Expr *callee;
			Expr **args;
			int arg_count;
		} call;

		struct {
			const char **params;
			int param_count;
			bool is_vararg;
			Stmt *body;
		} function;

		struct {
			TableEntry *entries;
			int count;
		} table;
	} as;
};

typedef enum {
	STMT_BLOCK,
	STMT_ASSIGN,
	STMT_LOCAL,
	STMT_EXPR,
	STMT_IF,
	STMT_WHILE,
	STMT_REPEAT,
	STMT_FOR_NUM,
	STMT_FOR_GEN,
	STMT_RETURN,
	STMT_BREAK
} StmtKind;

struct Stmt {
	StmtKind kind;

	union {
		struct {
			Stmt **statements;
			int count;
		} block;

		struct {
			Expr **targets;
			int target_count;
			Expr **values;
			int value_count;
		} assign;

		struct {
			const char **names;
			int name_count;
			Expr **values;
			int value_count;
		} local;

		struct {
			Expr *expr;
		} expr;

		struct {
			Expr *condition;
			Stmt *then_branch;
			Stmt *else_branch;
		} if_stmt;

		struct {
			Expr *condition;
			Stmt *body;
		} while_stmt;

		struct {
			Stmt *body;
			Expr *condition;
		} repeat_stmt;

		struct {
			const char *iter_name;
			Expr *start;
			Expr *end;
			Expr *step;
			Stmt *body;
		} for_num;

		struct {
			const char **var_names;
			int var_count;
			Expr *iterator;
			Stmt *body;
		} for_gen;

		struct {
			Expr **values;
			int count;
		} return_stmt;
	} as;
};
