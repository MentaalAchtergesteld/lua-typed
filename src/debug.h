#pragma once
#include <stdio.h>
#include "parser.h"
#include "token.h" // Zorg dat hier je Token typedefs in staan

// --- Token Printing ---
const char* token_kind_str(TokenKind kind);
void fprint_tokens(FILE *f, Token *tokens);
void print_tokens(Token *tokens); // Wrapper voor stdout

// --- AST Printing ---
void fprint_ast(FILE *f, Stmt *root);
void print_ast(Stmt *root);       // Wrapper voor stdout
