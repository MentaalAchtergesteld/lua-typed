#include "debug.h"
#include <stdio.h>
#include "vec.h" // Nodig voor vec_size()

// ==========================================
// TOKEN PRINTING
// ==========================================

const char* token_kind_str(TokenKind kind) {
    switch (kind) {
        case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "ERROR";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_STRING: return "STRING";
        case TOKEN_NUMBER: return "NUMBER";
        case TOKEN_LOCAL: return "LOCAL";
        case TOKEN_FUNCTION: return "FUNCTION";
        case TOKEN_STRUCT: return "STRUCT";
        case TOKEN_TRAIT: return "TRAIT";
        case TOKEN_IMPL: return "IMPL";
        case TOKEN_RETURN: return "RETURN";
        case TOKEN_IF: return "IF";
        case TOKEN_THEN: return "THEN";
        case TOKEN_ELSE: return "ELSE";
        case TOKEN_ELSEIF: return "ELSEIF";
        case TOKEN_END: return "END";
        case TOKEN_WHILE: return "WHILE";
        case TOKEN_DO: return "DO";
        case TOKEN_REPEAT: return "REPEAT";
        case TOKEN_UNTIL: return "UNTIL";
        case TOKEN_FOR: return "FOR";
        case TOKEN_IN: return "IN";
        case TOKEN_BREAK: return "BREAK";
        case TOKEN_NIL: return "NIL";
        case TOKEN_TRUE: return "TRUE";
        case TOKEN_FALSE: return "FALSE";
        case TOKEN_AND: return "AND";
        case TOKEN_OR: return "OR";
        case TOKEN_NOT: return "NOT";
        case TOKEN_TYPE: return "TYPE";
        case TOKEN_LPAREN: return "LPAREN";
        case TOKEN_RPAREN: return "RPAREN";
        case TOKEN_LBRACE: return "LBRACE";
        case TOKEN_RBRACE: return "RBRACE";
        case TOKEN_LBRACK: return "LBRACK";
        case TOKEN_RBRACK: return "RBRACK";
        case TOKEN_COMMA: return "COMMA";
        case TOKEN_DOT: return "DOT";
        case TOKEN_COLON: return "COLON";
        case TOKEN_SEMICOLON: return "SEMICOLON";
        case TOKEN_PLUS: return "PLUS";
        case TOKEN_MINUS: return "MINUS";
        case TOKEN_STAR: return "STAR";
        case TOKEN_SLASH: return "SLASH";
        case TOKEN_PERCENT: return "PERCENT";
        case TOKEN_CARET: return "CARET";
        case TOKEN_HASH: return "HASH";
        case TOKEN_EQ: return "EQ";
        case TOKEN_EQ_EQ: return "EQ_EQ";
        case TOKEN_NOT_EQ: return "NOT_EQ";
        case TOKEN_LT: return "LT";
        case TOKEN_LTEQ: return "LTEQ";
        case TOKEN_GT: return "GT";
        case TOKEN_GTEQ: return "GTEQ";
        case TOKEN_DOT_DOT: return "DOT_DOT";
        case TOKEN_DOT_DOT_DOT: return "DOT_DOT_DOT";
        case TOKEN_PIPE: return "PIPE";
        default: return "UNKNOWN";
    }
}

void fprint_tokens(FILE *f, Token *tokens) {
    if (!tokens) return;
    int count = vec_size(tokens);

    fprintf(f, "--- TOKENS (%d) ---\n", count);
    fprintf(f, "%-4s %-15s %s\n", "LINE", "KIND", "TEXT");
    fprintf(f, "------------------------------\n");

    for (int i = 0; i < count; i++) {
        Token t = tokens[i];
        // %llu voor u64 (unsigned long long)
        fprintf(f, "%-4llu %-15s '%s'\n", (unsigned long long)t.line, token_kind_str(t.kind), t.text ? t.text : "");
    }
    fprintf(f, "------------------------------\n\n");
}

void print_tokens(Token *tokens) {
    fprint_tokens(stdout, tokens);
}

// ==========================================
// AST PRINTING HELPERS
// ==========================================

// Forward declarations
void print_type(FILE *f, Type *t);
void print_expr_recursive(FILE *f, Expr *expr);
void print_ast_recursive(FILE *f, Stmt *node, int indent);

void print_indent(FILE *f, int indent) {
    for (int i = 0; i < indent; i++) fprintf(f, "  ");
}

static const char* bin_op_str(BinaryOp op) {
    switch (op) {
        case OP_ADD: return "+"; case OP_SUB: return "-";
        case OP_MUL: return "*"; case OP_DIV: return "/";
        case OP_MOD: return "%"; case OP_POW: return "^";
        case OP_CONCAT: return "..";
        case OP_EQ: return "=="; case OP_NEQ: return "~=";
        case OP_LT: return "<";  case OP_LTE: return "<=";
        case OP_GT: return ">";  case OP_GTE: return ">=";
        case OP_AND: return "and"; case OP_OR: return "or";
        default: return "?";
    }
}

static const char* unary_op_str(UnaryOp op) {
    switch (op) {
        case OP_NEGATE: return "-";
        case OP_NOT: return "not ";
        case OP_LEN: return "#";
        default: return "?";
    }
}

static void print_generic_params(FILE *f, GenericParam *generics, int count) {
    if (count == 0 || !generics) return;
    fprintf(f, "<");
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s", generics[i].name);
        if (generics[i].constraint_count > 0) {
            fprintf(f, ": ");
            for (int j = 0; j < generics[i].constraint_count; j++) {
                print_type(f, generics[i].constraints[j]);
                if (j < generics[i].constraint_count - 1) fprintf(f, " + ");
            }
        }
        if (i < count - 1) fprintf(f, ", ");
    }
    fprintf(f, ">");
}

static void print_func_signature(FILE *f, FuncSignature *sig) {
    if (!sig) return;
    print_generic_params(f, sig->generics, sig->generic_count);

    fprintf(f, "(");
    for (int i = 0; i < sig->param_count; i++) {
        fprintf(f, "%s", sig->params[i].name);
        if (sig->params[i].type) {
            fprintf(f, ": ");
            print_type(f, sig->params[i].type);
        }
        if (i < sig->param_count - 1) fprintf(f, ", ");
    }
    fprintf(f, ")");

    if (sig->return_count > 0) {
        fprintf(f, " -> ");
        if (sig->return_count > 1) fprintf(f, "(");
        for (int i = 0; i < sig->return_count; i++) {
            print_type(f, sig->return_types[i]); // Let op: return_types is Type** in je laatste AST
            if (i < sig->return_count - 1) fprintf(f, ", ");
        }
        if (sig->return_count > 1) fprintf(f, ")");
    }
}

// ==========================================
// TYPE PRINTING
// ==========================================

void print_type(FILE *f, Type *t) {
    if (!t) { fprintf(f, "?"); return; }

    switch (t->kind) {
        case TYPE_VOID:   fprintf(f, "void"); break;
        case TYPE_NIL:    fprintf(f, "nil"); break;
        case TYPE_BOOL:   fprintf(f, "bool"); break;
        case TYPE_NUMBER: fprintf(f, "number"); break;
        case TYPE_STRING: fprintf(f, "string"); break;
        
        case TYPE_ARRAY:
            fprintf(f, "[");
            print_type(f, t->as.array.inner);
            fprintf(f, "]");
            break;

        case TYPE_STRUCT:
        case TYPE_TRAIT:
        case TYPE_GENERIC: 
            if (t->kind == TYPE_GENERIC) {
                fprintf(f, "%s", t->as.param_name);
            } else {
                fprintf(f, "%s", t->as.user_type.name);
                if (t->as.user_type.arg_count > 0) {
                    fprintf(f, "<");
                    for (int i = 0; i < t->as.user_type.arg_count; i++) {
                        print_type(f, t->as.user_type.args[i]);
                        if (i < t->as.user_type.arg_count - 1) fprintf(f, ", ");
                    }
                    fprintf(f, ">");
                }
            }
            break;

        case TYPE_FUNCTION:
            fprintf(f, "fn");
            print_func_signature(f, t->as.function.sig);
            break;
            
        default: fprintf(f, "UnknownType"); break;
    }
}

// ==========================================
// EXPRESSION PRINTING
// ==========================================

void print_expr_recursive(FILE *f, Expr *expr) {
    if (!expr) { fprintf(f, "nil"); return; }

    switch (expr->kind) {
        case EXPR_NIL:      fprintf(f, "nil"); break;
        case EXPR_BOOL:     fprintf(f, expr->as.boolean ? "true" : "false"); break;
        case EXPR_NUMBER:   fprintf(f, "%g", expr->as.number); break;
        case EXPR_STRING:   fprintf(f, "\"%s\"", expr->as.string); break;
        case EXPR_VARIABLE: fprintf(f, "%s", expr->as.variable); break;
        case EXPR_VARARG:   fprintf(f, "..."); break;

        case EXPR_BINARY:
            fprintf(f, "(");
            print_expr_recursive(f, expr->as.binary.left);
            fprintf(f, " %s ", bin_op_str(expr->as.binary.op));
            print_expr_recursive(f, expr->as.binary.right);
            fprintf(f, ")");
            break;

        case EXPR_UNARY:
            fprintf(f, "(%s", unary_op_str(expr->as.unary.op));
            print_expr_recursive(f, expr->as.unary.operand);
            fprintf(f, ")");
            break;

        case EXPR_CALL:
            print_expr_recursive(f, expr->as.call.callee);
            fprintf(f, "(");
            for (int i = 0; i < expr->as.call.arg_count; i++) {
                print_expr_recursive(f, expr->as.call.args[i]);
                if (i < expr->as.call.arg_count - 1) fprintf(f, ", ");
            }
            fprintf(f, ")");
            break;

        case EXPR_INDEX:
            print_expr_recursive(f, expr->as.index.target);
            fprintf(f, "[");
            print_expr_recursive(f, expr->as.index.index);
            fprintf(f, "]");
            break;

        case EXPR_FIELD:
            print_expr_recursive(f, expr->as.field.target);
            fprintf(f, ".%s", expr->as.field.field);
            break;

        case EXPR_FUNCTION:
            fprintf(f, "fn");
            print_func_signature(f, &expr->as.function.signature);
            fprintf(f, " { ... }");
            break;

        case EXPR_TABLE:
            fprintf(f, "{");
            for (int i = 0; i < expr->as.table.entry_count; i++) {
                if (expr->as.table.entries[i].key) {
                    fprintf(f, "[");
                    print_expr_recursive(f, expr->as.table.entries[i].key);
                    fprintf(f, "]=");
                }
                print_expr_recursive(f, expr->as.table.entries[i].value);
                if (i < expr->as.table.entry_count - 1) fprintf(f, ", ");
            }
            fprintf(f, "}");
            break;
            
        case EXPR_STRUCT:
						print_expr_recursive(f, expr->as.struct_init.name);
						fprintf(f, " { ");
            for (int i = 0; i < expr->as.struct_init.entry_count; i++) {
                if (expr->as.struct_init.entries[i].key) {
                   print_expr_recursive(f, expr->as.struct_init.entries[i].key); 
                   fprintf(f, " = ");
                }
                print_expr_recursive(f, expr->as.struct_init.entries[i].value);
                if (i < expr->as.struct_init.entry_count - 1) fprintf(f, ", ");
            }
            fprintf(f, " }");
            break;
    }
}

// ==========================================
// STATEMENT PRINTING
// ==========================================

void print_ast_recursive(FILE *f, Stmt *node, int indent) {
    if (!node) return;

    print_indent(f, indent);

    switch (node->kind) {
        case STMT_EXPR:
            fprintf(f, "EXPR ");
            print_expr_recursive(f, node->as.expression);
            fprintf(f, "\n");
            break;

        case STMT_BLOCK:
            fprintf(f, "BLOCK\n");
            for (int i = 0; i < node->as.block.stmt_count; i++) {
                print_ast_recursive(f, node->as.block.stmts[i], indent + 1);
            }
            print_indent(f, indent);
            fprintf(f, "END BLOCK\n");
            break;

        case STMT_RETURN:
            fprintf(f, "RETURN ");
            for (int i = 0; i < node->as.return_stmt.value_count; i++) {
                print_expr_recursive(f, node->as.return_stmt.values[i]);
                if (i < node->as.return_stmt.value_count - 1) fprintf(f, ", ");
            }
            fprintf(f, "\n");
            break;

        case STMT_BREAK:
            fprintf(f, "BREAK\n");
            break;

        case STMT_ASSIGN:
            fprintf(f, "ASSIGN ");
            for (int i = 0; i < node->as.assign.target_count; i++) {
                print_expr_recursive(f, node->as.assign.targets[i]);
                if (i < node->as.assign.target_count - 1) fprintf(f, ", ");
            }
            fprintf(f, " = ");
            for (int i = 0; i < node->as.assign.value_count; i++) {
                print_expr_recursive(f, node->as.assign.values[i]);
                if (i < node->as.assign.value_count - 1) fprintf(f, ", ");
            }
            fprintf(f, "\n");
            break;

        case STMT_LOCAL:
            fprintf(f, "LOCAL ");
            for (int i = 0; i < node->as.local.decl_count; i++) {
                fprintf(f, "%s", node->as.local.decls[i].name);
                if (node->as.local.decls[i].type) {
                    fprintf(f, ": ");
                    print_type(f, node->as.local.decls[i].type);
                }
                if (i < node->as.local.decl_count - 1) fprintf(f, ", ");
            }
            if (node->as.local.value_count > 0) {
                fprintf(f, " = ");
                for (int i = 0; i < node->as.local.value_count; i++) {
                    print_expr_recursive(f, node->as.local.values[i]);
                    if (i < node->as.local.value_count - 1) fprintf(f, ", ");
                }
            }
            fprintf(f, "\n");
            break;

        case STMT_IF:
            fprintf(f, "IF ");
            print_expr_recursive(f, node->as.if_stmt.condition);
            fprintf(f, " THEN\n");
            print_ast_recursive(f, node->as.if_stmt.then_branch, indent + 1);
            
            if (node->as.if_stmt.else_branch) {
                print_indent(f, indent);
                fprintf(f, "ELSE\n");
                print_ast_recursive(f, node->as.if_stmt.else_branch, indent + 1);
            }
            break;

        case STMT_WHILE:
            fprintf(f, "WHILE ");
            print_expr_recursive(f, node->as.while_stmt.condition);
            fprintf(f, " DO\n");
            print_ast_recursive(f, node->as.while_stmt.body, indent + 1);
            break;

        case STMT_REPEAT:
            fprintf(f, "REPEAT\n");
            print_ast_recursive(f, node->as.repeat_stmt.body, indent + 1);
            print_indent(f, indent);
            fprintf(f, "UNTIL ");
            print_expr_recursive(f, node->as.repeat_stmt.condition);
            fprintf(f, "\n");
            break;

        case STMT_FOR_NUM:
            fprintf(f, "FOR %s = ", node->as.for_num.name);
            print_expr_recursive(f, node->as.for_num.start);
            fprintf(f, ", ");
            print_expr_recursive(f, node->as.for_num.end);
            if (node->as.for_num.step) {
                fprintf(f, ", ");
                print_expr_recursive(f, node->as.for_num.step);
            }
            fprintf(f, " DO\n");
            print_ast_recursive(f, node->as.for_num.body, indent + 1);
            break;

        case STMT_FOR_GEN:
            fprintf(f, "FOR ");
            for(int i=0; i < node->as.for_gen.name_count; i++) {
                fprintf(f, "%s", node->as.for_gen.names[i]);
                if (i < node->as.for_gen.name_count - 1) fprintf(f, ", ");
            }
            fprintf(f, " IN ");
            print_expr_recursive(f, node->as.for_gen.iter);
            fprintf(f, " DO\n");
            print_ast_recursive(f, node->as.for_gen.body, indent + 1);
            break;

        case STMT_FUNCTION:
            fprintf(f, "FUNCTION %s", node->as.func_decl.name);
            print_func_signature(f, node->as.func_decl.signature);
            fprintf(f, "\n");
            print_ast_recursive(f, node->as.func_decl.body, indent + 1);
            print_indent(f, indent);
            fprintf(f, "END FUNC\n");
            break;

        case STMT_STRUCT:
            fprintf(f, "STRUCT %s", node->as.struct_decl.name);
            print_generic_params(f, node->as.struct_decl.generics, node->as.struct_decl.generic_count);
            fprintf(f, "\n");
            for (int i = 0; i < node->as.struct_decl.field_count; i++) {
                print_indent(f, indent + 1);
                fprintf(f, "%s: ", node->as.struct_decl.fields[i].name);
                print_type(f, node->as.struct_decl.fields[i].type);
                fprintf(f, "\n");
            }
            print_indent(f, indent);
            fprintf(f, "END STRUCT\n");
            break;

        case STMT_TRAIT:
            fprintf(f, "TRAIT %s", node->as.trait_decl.name);
            print_generic_params(f, node->as.trait_decl.generics, node->as.trait_decl.generic_count);
            fprintf(f, "\n");
            for (int i = 0; i < node->as.trait_decl.func_count; i++) {
                print_indent(f, indent + 1);
                fprintf(f, "fn %s", node->as.trait_decl.func_names[i]);
                print_func_signature(f, node->as.trait_decl.functions[i]);
                fprintf(f, "\n");
            }
            print_indent(f, indent);
            fprintf(f, "END TRAIT\n");
            break;

        case STMT_IMPL:
            fprintf(f, "IMPL");
            print_generic_params(f, node->as.impl_stmt.generics, node->as.impl_stmt.generic_count);
            fprintf(f, " ");
            
            if (node->as.impl_stmt.trait_name) {
                fprintf(f, "%s", node->as.impl_stmt.trait_name);
                if (node->as.impl_stmt.trait_arg_count > 0) {
                    fprintf(f, "<");
                    for(int i=0; i<node->as.impl_stmt.trait_arg_count; i++) {
                        print_type(f, node->as.impl_stmt.trait_args[i]);
                        if (i < node->as.impl_stmt.trait_arg_count - 1) fprintf(f, ", ");
                    }
                    fprintf(f, ">");
                }
                fprintf(f, " FOR ");
            }

            fprintf(f, "%s", node->as.impl_stmt.target_name);
            if (node->as.impl_stmt.target_arg_count > 0) {
                fprintf(f, "<");
                for(int i=0; i<node->as.impl_stmt.target_arg_count; i++) {
                    print_type(f, node->as.impl_stmt.target_args[i]);
                    if (i < node->as.impl_stmt.target_arg_count - 1) fprintf(f, ", ");
                }
                fprintf(f, ">");
            }
            fprintf(f, "\n");
            
            for (int i = 0; i < node->as.impl_stmt.func_count; i++) {
                print_ast_recursive(f, node->as.impl_stmt.functions[i], indent + 1);
            }
            print_indent(f, indent);
            fprintf(f, "END IMPL\n");
            break;

        case STMT_TYPE_ALIAS:
            fprintf(f, "TYPE %s = ", node->as.type_alias.name);
            print_type(f, node->as.type_alias.type);
            fprintf(f, "\n");
            break;
    }
}

void fprint_ast(FILE *f, Stmt *root) {
    if (!root) {
        fprintf(f, "(Empty AST)\n");
        return;
    }
    print_ast_recursive(f, root, 0);
}

void print_ast(Stmt *root) {
    fprint_ast(stdout, root);
}
