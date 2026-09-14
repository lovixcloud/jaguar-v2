#ifndef JAG_TYPECHECK_H
#define JAG_TYPECHECK_H

#include "ast/ast.h"
#include <stdbool.h>

typedef struct Symbol {
    char *name;
    JagType *type;
    bool is_fixed;
    bool is_captured;
    struct Symbol *next;
} Symbol;

typedef struct Scope {
    Symbol *symbols;
    struct Scope *parent;
    bool is_async_context;
    bool is_worker_context;
} Scope;

typedef struct {
    Scope *current_scope;
    bool live_mode;
    bool had_error;
    char error_msg[512];
} TypeChecker;

void typechecker_init(TypeChecker *tc, bool live_mode);
void typechecker_cleanup(TypeChecker *tc);
bool typecheck_ast(TypeChecker *tc, ASTNode *node);

#endif
