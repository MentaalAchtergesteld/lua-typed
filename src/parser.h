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

typedef struct {
	const char *name;
	Expr *constraints;
} GenericParam;

typedef struct {
	GenericParam *generics;
	Param *params;
	Expr *return_types;
} FuncSignature;

typedef struct {
	Expr *key;
	Expr *value;
} TableEntry;

typedef enum {
	EXPR_NIL, EXPR_BOOL, EXPR_NUMBER, EXPR_STRING,
	EXPR_VARARG, EXPR_VARIABLE,
	EXPR_BINARY, EXPR_UNARY,
	EXPR_CALL, EXPR_INDEX, EXPR_FIELD,
	EXPR_FUNCTION, EXPR_TABLE, EXPR_STRUCT
} ExprKind;

#define EXPR_HEADER ExprKind kind

struct Expr {
	EXPR_HEADER;
};

typedef struct {
	EXPR_HEADER;
	bool value;
} BoolLit;

typedef struct {
	EXPR_HEADER;
	float value;
} NumberLit;

typedef struct {
	EXPR_HEADER;
	const char* value;
} StringLit;

typedef struct {
	EXPR_HEADER;
	const char *name;
} VariableExpr;

typedef struct {
	EXPR_HEADER;
	Expr *left;
	Expr *right;
	BinaryOp op;
} BinaryExpr;

typedef struct {
	EXPR_HEADER;
	Expr *operand;
	UnaryOp op;
} UnaryExpr;

typedef struct {
	EXPR_HEADER;
	Expr *callee;
	Expr *args;
} CallExpr;

typedef struct {
	EXPR_HEADER;
	Expr *target;
	Expr *index;
} IndexExpr;

typedef struct {
	EXPR_HEADER;
	Expr *target;
	const char *field;
} FieldExpr;

typedef struct {
	FuncSignature signature;
	Stmt *body;
} FuncExpr;

typedef struct {
	TableEntry *entries;
} TableExpr;

typedef struct {
	const char *name;
	TableEntry *fields;
	int count;
} StructExpr;

typedef enum {
	STMT_BLOCK, STMT_EXPR, STMT_RETURN, STMT_BREAK,
	STMT_ASSIGN, STMT_LOCAL,
	STMT_IF, STMT_WHILE, STMT_REPEAT, STMT_FOR_NUM, STMT_FOR_GEN,
	STMT_FUNCTION, STMT_STRUCT, STMT_TRAIT, STMT_IMPL, STMT_TYPE_ALIAS,
} StmtKind;

#define STMT_HEADER StmtKind kind

struct Stmt {
	STMT_HEADER;
};

typedef struct {
	Stmt *statements;
} BlockStmt;

typedef struct {
	Expr *expression;
} ExprStmt;

typedef struct {
	Expr *values;
} ReturnStmt;

typedef struct {
	Expr *targets;
	Expr *values;
} AssignStmt;

typedef struct {
	Param *decls;
	Expr *values;
} LocalStmt;

typedef struct {
	Expr *condition;
	Stmt *then_branch;
	Stmt *else_branch;
} IfStmt;

typedef struct {
	Expr *condition;
	Stmt *body;
} WhileStmt;

typedef struct {
	Stmt *body;
	Expr *condition;
} RepeatStmt;

typedef struct {
	const char *name;
	Expr *start;
	Expr *end;
	Expr *step;
	Stmt *body;
} ForNumStmt;

typedef struct {
	const char *names;
	Expr *iterator;
	Stmt *body;
} ForGenStmt;

typedef struct {
	const char *name;
	FuncSignature *signature;
	Stmt *body;
} FuncDecl;

typedef struct {
	const char *name;
	GenericParam *generics;
	Param *fields;
} StructDecl;

typedef struct {
	const char *name;
	GenericParam *generics;
	const char *func_names;
	FuncSignature *functions;
} TraitDecl;

typedef struct {
	const char *target_name;
	GenericParam *generics;

	const char *trait_name;
	Expr *trait_args;

	Stmt *functions;
} ImplStmt;

typedef struct {
	const char *name;
	Expr *type;
} TypeAliasStmt;
