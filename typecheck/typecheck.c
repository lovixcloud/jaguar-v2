#include "typecheck.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Scope *scope_new(Scope *parent, bool is_async, bool is_worker) {
    Scope *s = (Scope *)calloc(1, sizeof(Scope));
    s->parent = parent;
    s->is_async_context = is_async ? true : (parent ? parent->is_async_context : false);
    s->is_worker_context = is_worker ? true : (parent ? parent->is_worker_context : false);
    return s;
}

static void scope_free(Scope *s) {
    if (!s) return;
    Symbol *sym = s->symbols;
    while (sym) {
        Symbol *next = sym->next;
        if (sym->name) free(sym->name);
        if (sym->type) jag_type_free(sym->type);
        free(sym);
        sym = next;
    }
    free(s);
}

static Symbol *scope_lookup(Scope *s, const char *name) {
    while (s) {
        Symbol *sym = s->symbols;
        while (sym) {
            if (strcmp(sym->name, name) == 0) return sym;
            sym = sym->next;
        }
        s = s->parent;
    }
    return NULL;
}

static bool scope_define(Scope *s, const char *name, JagType *type, bool is_fixed) {
    Symbol *sym = (Symbol *)calloc(1, sizeof(Symbol));
    sym->name = strdup(name);
    sym->type = type;
    sym->is_fixed = is_fixed;
    sym->next = s->symbols;
    s->symbols = sym;
    return true;
}

void typechecker_init(TypeChecker *tc, bool live_mode) {
    tc->current_scope = scope_new(NULL, false, false);
    tc->live_mode = live_mode;
    tc->had_error = false;
    tc->error_msg[0] = '\0';

    scope_define(tc->current_scope, "env", jag_type_new(JAG_TYPE_DATA), true);
    scope_define(tc->current_scope, "http", jag_type_new(JAG_TYPE_DATA), true);
    scope_define(tc->current_scope, "server", jag_type_new(JAG_TYPE_DATA), true);
    scope_define(tc->current_scope, "socket", jag_type_new(JAG_TYPE_DATA), true);
    scope_define(tc->current_scope, "worker", jag_type_new(JAG_TYPE_DATA), true);
    scope_define(tc->current_scope, "json", jag_type_new(JAG_TYPE_DATA), true);
    scope_define(tc->current_scope, "live", jag_type_new(JAG_TYPE_DATA), true);
}

void typechecker_cleanup(TypeChecker *tc) {
    while (tc->current_scope) {
        Scope *parent = tc->current_scope->parent;
        scope_free(tc->current_scope);
        tc->current_scope = parent;
    }
}

static void report_error(TypeChecker *tc, ASTNode *node, const char *msg) {
    tc->had_error = true;
    snprintf(tc->error_msg, sizeof(tc->error_msg), "Type Error at line %d, col %d: %s",
             node ? node->line : 0, node ? node->column : 0, msg);
    printf("%s\n", tc->error_msg);
}

static bool typecheck_node(TypeChecker *tc, ASTNode *node);

static bool is_copyable_type(JagType *type) {
    if (!type) return true;
    switch (type->kind) {
        case JAG_TYPE_SOCKET:
        case JAG_TYPE_WORKER:
            return false;
        default:
            return true;
    }
}

static bool typecheck_node(TypeChecker *tc, ASTNode *node) {
    if (!node) return true;

    switch (node->kind) {
        case AST_PROGRAM:
            for (size_t i = 0; i < node->program.stmts.count; i++) {
                if (!typecheck_node(tc, node->program.stmts.nodes[i])) return false;
            }
            break;

        case AST_BLOCK: {
            tc->current_scope = scope_new(tc->current_scope, false, false);
            for (size_t i = 0; i < node->block.stmts.count; i++) {
                if (!typecheck_node(tc, node->block.stmts.nodes[i])) {
                    Scope *p = tc->current_scope->parent;
                    scope_free(tc->current_scope);
                    tc->current_scope = p;
                    return false;
                }
            }
            Scope *p = tc->current_scope->parent;
            scope_free(tc->current_scope);
            tc->current_scope = p;
            break;
        }

        case AST_VAR_DECL:
        case AST_CONST_DECL: {
            if (node->var_decl.init) {
                if (!typecheck_node(tc, node->var_decl.init)) return false;
                if (!node->var_decl.type && node->var_decl.init->inferred_type) {
                    node->var_decl.type = jag_type_new(node->var_decl.init->inferred_type->kind);
                }
            }
            scope_define(tc->current_scope, node->var_decl.name,
                         node->var_decl.type ? jag_type_new(node->var_decl.type->kind) : jag_type_new(JAG_TYPE_ANY),
                         node->var_decl.is_fixed);
            break;
        }

        case AST_ASSIGNMENT: {
            if (!typecheck_node(tc, node->assignment.target)) return false;
            if (!typecheck_node(tc, node->assignment.value)) return false;

            if (node->assignment.target->kind == AST_IDENTIFIER) {
                Symbol *sym = scope_lookup(tc->current_scope, node->assignment.target->string_val);
                if (sym && sym->is_fixed) {
                    report_error(tc, node, "Cannot reassign to constant (fixed) variable");
                    return false;
                }
            }
            break;
        }

        case AST_FUNC_DECL: {
            tc->current_scope = scope_new(tc->current_scope, node->func_decl.is_async, false);
            for (int i = 0; i < node->func_decl.param_count; i++) {
                scope_define(tc->current_scope, node->func_decl.params[i].key,
                             node->func_decl.params[i].type ? jag_type_new(node->func_decl.params[i].type->kind) : jag_type_new(JAG_TYPE_ANY),
                             false);
            }
            if (!typecheck_node(tc, node->func_decl.body)) {
                Scope *p = tc->current_scope->parent;
                scope_free(tc->current_scope);
                tc->current_scope = p;
                return false;
            }
            Scope *p = tc->current_scope->parent;
            scope_free(tc->current_scope);
            tc->current_scope = p;

            scope_define(tc->current_scope, node->func_decl.name, jag_type_new(JAG_TYPE_FUNCTION), true);
            break;
        }

        case AST_AWAIT_EXPR: {
            if (!tc->current_scope->is_async_context && !tc->live_mode) {
                report_error(tc, node, "'await' is only allowed inside an 'async fun' body or top-level live mode");
                return false;
            }
            if (!typecheck_node(tc, node->await_expr.expr)) return false;
            node->inferred_type = jag_type_new(JAG_TYPE_ANY);
            break;
        }

        case AST_LAMBDA_EXPR: {
            tc->current_scope = scope_new(tc->current_scope, node->lambda.is_async, false);
            for (int i = 0; i < node->lambda.param_count; i++) {
                scope_define(tc->current_scope, node->lambda.params[i].key,
                             node->lambda.params[i].type ? jag_type_new(node->lambda.params[i].type->kind) : jag_type_new(JAG_TYPE_ANY),
                             false);
            }
            if (!typecheck_node(tc, node->lambda.body)) {
                Scope *p = tc->current_scope->parent;
                scope_free(tc->current_scope);
                tc->current_scope = p;
                return false;
            }
            Scope *p = tc->current_scope->parent;
            scope_free(tc->current_scope);
            tc->current_scope = p;
            node->inferred_type = jag_type_new(JAG_TYPE_FUNCTION);
            break;
        }

        case AST_CALL_EXPR: {
            if (!typecheck_node(tc, node->call.callee)) return false;
            for (int i = 0; i < node->call.arg_count; i++) {
                if (!typecheck_node(tc, node->call.args[i])) return false;
            }

            if (node->call.callee->kind == AST_MEMBER_EXPR &&
                node->call.callee->member.object->kind == AST_IDENTIFIER &&
                strcmp(node->call.callee->member.object->string_val, "worker") == 0) {

                if (strcmp(node->call.callee->member.member, "spawn") == 0 ||
                    strcmp(node->call.callee->member.member, "run") == 0) {
                    ASTNode *arg = node->call.args[0];
                    if (arg && arg->kind == AST_LAMBDA_EXPR) {
                        Symbol *sym = tc->current_scope->symbols;
                        while (sym) {
                            if (!is_copyable_type(sym->type)) {
                                char buf[256];
                                snprintf(buf, sizeof(buf), "Cannot capture non-copyable variable '%s' across worker thread boundary", sym->name);
                                report_error(tc, arg, buf);
                                return false;
                            }
                            sym = sym->next;
                        }
                    }
                }
            }

            node->inferred_type = jag_type_new(JAG_TYPE_ANY);
            break;
        }

        case AST_IDENTIFIER: {
            Symbol *sym = scope_lookup(tc->current_scope, node->string_val);
            if (sym && sym->type) {
                node->inferred_type = jag_type_new(sym->type->kind);
            } else {
                node->inferred_type = jag_type_new(JAG_TYPE_ANY);
            }
            break;
        }

        case AST_LITERAL_NUM:
            node->inferred_type = jag_type_new(JAG_TYPE_NUM);
            break;
        case AST_LITERAL_DECIMAL:
            node->inferred_type = jag_type_new(JAG_TYPE_DECIMAL);
            break;
        case AST_LITERAL_SCIFI:
            node->inferred_type = jag_type_new(JAG_TYPE_SCIFI);
            break;
        case AST_LITERAL_STRING:
            node->inferred_type = jag_type_new(JAG_TYPE_STRING);
            break;
        case AST_LITERAL_BOOL:
            node->inferred_type = jag_type_new(JAG_TYPE_BOOL);
            break;

        case AST_EXPR_STMT:
            if (!typecheck_node(tc, node->expr_stmt.expr)) return false;
            break;

        case AST_IF_STMT:
            if (!typecheck_node(tc, node->if_stmt.cond)) return false;
            if (!typecheck_node(tc, node->if_stmt.then_branch)) return false;
            if (node->if_stmt.else_branch && !typecheck_node(tc, node->if_stmt.else_branch)) return false;
            break;

        case AST_LOOP_STMT:
            if (!typecheck_node(tc, node->loop_stmt.cond)) return false;
            if (!typecheck_node(tc, node->loop_stmt.body)) return false;
            break;

        case AST_RETURN_STMT:
            if (node->return_stmt.expr && !typecheck_node(tc, node->return_stmt.expr)) return false;
            break;

        case AST_TRY_CATCH_STMT: {
            if (!typecheck_node(tc, node->try_catch_stmt.try_block)) return false;
            tc->current_scope = scope_new(tc->current_scope, false, false);
            scope_define(tc->current_scope, node->try_catch_stmt.err_var_name, jag_type_new(JAG_TYPE_STRING), false);
            if (!typecheck_node(tc, node->try_catch_stmt.catch_block)) {
                Scope *p = tc->current_scope->parent;
                scope_free(tc->current_scope);
                tc->current_scope = p;
                return false;
            }
            Scope *p = tc->current_scope->parent;
            scope_free(tc->current_scope);
            tc->current_scope = p;
            break;
        }

        default:
            break;
    }

    return true;
}

bool typecheck_ast(TypeChecker *tc, ASTNode *node) {
    return typecheck_node(tc, node);
}
