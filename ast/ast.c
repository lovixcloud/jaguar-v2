#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

JagType *jag_type_new(JagTypeKind kind) {
    JagType *t = (JagType *)calloc(1, sizeof(JagType));
    t->kind = kind;
    return t;
}

JagType *jag_type_new_generic(JagTypeKind kind, JagType *elem_type) {
    JagType *t = jag_type_new(kind);
    t->element_type = elem_type;
    return t;
}

void jag_type_free(JagType *type) {
    if (!type) return;
    if (type->name) free(type->name);
    if (type->element_type) jag_type_free(type->element_type);
    if (type->return_type) jag_type_free(type->return_type);
    if (type->param_types) {
        for (int i = 0; i < type->param_count; i++) {
            jag_type_free(type->param_types[i]);
        }
        free(type->param_types);
    }
    free(type);
}

bool jag_type_equals(JagType *a, JagType *b) {
    if (!a && !b) return true;
    if (!a || !b) return false;
    if (a->kind != b->kind) return false;

    if (a->element_type || b->element_type) {
        if (!jag_type_equals(a->element_type, b->element_type)) return false;
    }

    if (a->name || b->name) {
        if (!a->name || !b->name) return false;
        if (strcmp(a->name, b->name) != 0) return false;
    }

    return true;
}

char *jag_type_to_string(JagType *type) {
    if (!type) return strdup("unknown");
    char buf[256];
    switch (type->kind) {
        case JAG_TYPE_STRING: return strdup("string");
        case JAG_TYPE_NUM: return strdup("num");
        case JAG_TYPE_DECIMAL: return strdup("decimal");
        case JAG_TYPE_BOOL: return strdup("bool");
        case JAG_TYPE_SCIFI: return strdup("scifi");
        case JAG_TYPE_DATA: return strdup("data");
        case JAG_TYPE_MIXED_LIST: return strdup("MixedList");
        case JAG_TYPE_WORKER: return strdup("Worker");
        case JAG_TYPE_SOCKET: return strdup("Socket");
        case JAG_TYPE_LIST:
            if (type->element_type) {
                char *elem = jag_type_to_string(type->element_type);
                snprintf(buf, sizeof(buf), "list<%s>", elem);
                free(elem);
                return strdup(buf);
            }
            return strdup("list");
        case JAG_TYPE_TASK:
            if (type->element_type) {
                char *elem = jag_type_to_string(type->element_type);
                snprintf(buf, sizeof(buf), "Task<%s>", elem);
                free(elem);
                return strdup(buf);
            }
            return strdup("Task");
        case JAG_TYPE_VECTOR:
            if (type->element_type) {
                char *elem = jag_type_to_string(type->element_type);
                snprintf(buf, sizeof(buf), "vector<%s>", elem);
                free(elem);
                return strdup(buf);
            }
            return strdup("vector");
        case JAG_TYPE_MATRIX:
            if (type->element_type) {
                char *elem = jag_type_to_string(type->element_type);
                snprintf(buf, sizeof(buf), "matrix<%s>", elem);
                free(elem);
                return strdup(buf);
            }
            return strdup("matrix");
        case JAG_TYPE_STRUCT:
            return strdup(type->name ? type->name : "struct");
        case JAG_TYPE_ENUM:
            return strdup(type->name ? type->name : "enum");
        default:
            return strdup("any");
    }
}

void ast_node_list_init(ASTNodeList *list) {
    list->nodes = NULL;
    list->count = 0;
    list->capacity = 0;
}

void ast_node_list_append(ASTNodeList *list, ASTNode *node) {
    if (list->count + 1 > list->capacity) {
        list->capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        list->nodes = (ASTNode **)realloc(list->nodes, sizeof(ASTNode *) * list->capacity);
    }
    list->nodes[list->count++] = node;
}

void ast_node_list_free(ASTNodeList *list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; i++) {
        ast_node_free(list->nodes[i]);
    }
    if (list->nodes) free(list->nodes);
    list->nodes = NULL;
    list->count = list->capacity = 0;
}

ASTNode *ast_node_new(ASTNodeKind kind, int line, int col) {
    ASTNode *node = (ASTNode *)calloc(1, sizeof(ASTNode));
    node->kind = kind;
    node->line = line;
    node->column = col;
    return node;
}

void ast_node_free(ASTNode *node) {
    if (!node) return;
    if (node->inferred_type) jag_type_free(node->inferred_type);

    switch (node->kind) {
        case AST_PROGRAM:
            ast_node_list_free(&node->program.stmts);
            break;
        case AST_BLOCK:
            ast_node_list_free(&node->block.stmts);
            break;
        case AST_VAR_DECL:
            if (node->var_decl.name) free(node->var_decl.name);
            if (node->var_decl.type) jag_type_free(node->var_decl.type);
            if (node->var_decl.init) ast_node_free(node->var_decl.init);
            break;
        case AST_FUNC_DECL:
            if (node->func_decl.name) free(node->func_decl.name);
            if (node->func_decl.params) {
                for (int i = 0; i < node->func_decl.param_count; i++) {
                    if (node->func_decl.params[i].key) free(node->func_decl.params[i].key);
                    if (node->func_decl.params[i].type) jag_type_free(node->func_decl.params[i].type);
                }
                free(node->func_decl.params);
            }
            if (node->func_decl.return_type) jag_type_free(node->func_decl.return_type);
            if (node->func_decl.body) ast_node_free(node->func_decl.body);
            break;
        case AST_EXPR_STMT:
            if (node->expr_stmt.expr) ast_node_free(node->expr_stmt.expr);
            break;
        case AST_IF_STMT:
            if (node->if_stmt.cond) ast_node_free(node->if_stmt.cond);
            if (node->if_stmt.then_branch) ast_node_free(node->if_stmt.then_branch);
            if (node->if_stmt.else_branch) ast_node_free(node->if_stmt.else_branch);
            break;
        case AST_LOOP_STMT:
            if (node->loop_stmt.cond) ast_node_free(node->loop_stmt.cond);
            if (node->loop_stmt.body) ast_node_free(node->loop_stmt.body);
            break;
        case AST_DO_LOOP_STMT:
            if (node->do_loop_stmt.cond) ast_node_free(node->do_loop_stmt.cond);
            if (node->do_loop_stmt.body) ast_node_free(node->do_loop_stmt.body);
            break;
        case AST_FOR_IN_STMT:
            if (node->for_in_stmt.var_name) free(node->for_in_stmt.var_name);
            if (node->for_in_stmt.collection) ast_node_free(node->for_in_stmt.collection);
            if (node->for_in_stmt.guard_cond) ast_node_free(node->for_in_stmt.guard_cond);
            if (node->for_in_stmt.body) ast_node_free(node->for_in_stmt.body);
            break;
        case AST_ITERATE_STMT:
            if (node->iterate_stmt.item_name) free(node->iterate_stmt.item_name);
            if (node->iterate_stmt.collection) ast_node_free(node->iterate_stmt.collection);
            if (node->iterate_stmt.body) ast_node_free(node->iterate_stmt.body);
            break;
        case AST_RETURN_STMT:
            if (node->return_stmt.expr) ast_node_free(node->return_stmt.expr);
            break;
        case AST_TRY_CATCH_STMT:
            if (node->try_catch_stmt.try_block) ast_node_free(node->try_catch_stmt.try_block);
            if (node->try_catch_stmt.err_var_name) free(node->try_catch_stmt.err_var_name);
            if (node->try_catch_stmt.err_type) jag_type_free(node->try_catch_stmt.err_type);
            if (node->try_catch_stmt.catch_block) ast_node_free(node->try_catch_stmt.catch_block);
            break;
        case AST_LIVE_ON_STMT:
            if (node->live_on.expr) ast_node_free(node->live_on.expr);
            break;
        case AST_LIVE_DEG_STMT:
            if (node->live_deg.expected_type) free(node->live_deg.expected_type);
            if (node->live_deg.label) free(node->live_deg.label);
            if (node->live_deg.expr) ast_node_free(node->live_deg.expr);
            break;
        case AST_SERVER_ON:
            if (node->server_on.port_expr) ast_node_free(node->server_on.port_expr);
            if (node->server_on.options_data) ast_node_free(node->server_on.options_data);
            break;
        case AST_SERVER_ROUTE:
            if (node->server_route.method_expr) ast_node_free(node->server_route.method_expr);
            if (node->server_route.path_expr) ast_node_free(node->server_route.path_expr);
            if (node->server_route.handler_expr) ast_node_free(node->server_route.handler_expr);
            break;
        case AST_SOCKET_ON:
            if (node->socket_on.port_expr) ast_node_free(node->socket_on.port_expr);
            break;
        case AST_SOCKET_ROUTE:
            if (node->socket_route.path_expr) ast_node_free(node->socket_route.path_expr);
            if (node->socket_route.handler_expr) ast_node_free(node->socket_route.handler_expr);
            break;
        case AST_LITERAL_STRING:
        case AST_IDENTIFIER:
            if (node->string_val) free(node->string_val);
            break;
        case AST_LITERAL_DATA:
            for (int i = 0; i < node->data_literal.entry_count; i++) {
                if (node->data_literal.entries[i].key) free(node->data_literal.entries[i].key);
                if (node->data_literal.entries[i].value) ast_node_free(node->data_literal.entries[i].value);
            }
            if (node->data_literal.entries) free(node->data_literal.entries);
            break;
        case AST_LITERAL_LIST:
            for (int i = 0; i < node->list_literal.element_count; i++) {
                if (node->list_literal.elements[i]) ast_node_free(node->list_literal.elements[i]);
            }
            if (node->list_literal.elements) free(node->list_literal.elements);
            if (node->list_literal.element_type) jag_type_free(node->list_literal.element_type);
            break;
        case AST_BINARY_EXPR:
            if (node->binary.left) ast_node_free(node->binary.left);
            if (node->binary.right) ast_node_free(node->binary.right);
            break;
        case AST_UNARY_EXPR:
            if (node->unary.operand) ast_node_free(node->unary.operand);
            break;
        case AST_TERNARY_EXPR:
            if (node->ternary.cond) ast_node_free(node->ternary.cond);
            if (node->ternary.then_expr) ast_node_free(node->ternary.then_expr);
            if (node->ternary.else_expr) ast_node_free(node->ternary.else_expr);
            break;
        case AST_ASSIGNMENT:
            if (node->assignment.target) ast_node_free(node->assignment.target);
            if (node->assignment.value) ast_node_free(node->assignment.value);
            break;
        case AST_CALL_EXPR:
            if (node->call.callee) ast_node_free(node->call.callee);
            for (int i = 0; i < node->call.arg_count; i++) {
                if (node->call.args[i]) ast_node_free(node->call.args[i]);
            }
            if (node->call.args) free(node->call.args);
            break;
        case AST_MEMBER_EXPR:
            if (node->member.object) ast_node_free(node->member.object);
            if (node->member.member) free(node->member.member);
            break;
        case AST_INDEX_EXPR:
            if (node->index.object) ast_node_free(node->index.object);
            if (node->index.index) ast_node_free(node->index.index);
            break;
        case AST_AWAIT_EXPR:
            if (node->await_expr.expr) ast_node_free(node->await_expr.expr);
            break;
        case AST_LAMBDA_EXPR:
            if (node->lambda.params) {
                for (int i = 0; i < node->lambda.param_count; i++) {
                    if (node->lambda.params[i].key) free(node->lambda.params[i].key);
                    if (node->lambda.params[i].type) jag_type_free(node->lambda.params[i].type);
                }
                free(node->lambda.params);
            }
            if (node->lambda.return_type) jag_type_free(node->lambda.return_type);
            if (node->lambda.body) ast_node_free(node->lambda.body);
            break;
        case AST_BETWEEN_EXPR:
            if (node->between.val) ast_node_free(node->between.val);
            if (node->between.low) ast_node_free(node->between.low);
            if (node->between.high) ast_node_free(node->between.high);
            break;
        case AST_TEMPLATE_STR:
            if (node->template_str.parts) {
                for (int i = 0; i < node->template_str.part_count; i++) {
                    if (node->template_str.parts[i]) free(node->template_str.parts[i]);
                }
                free(node->template_str.parts);
            }
            if (node->template_str.exprs) {
                for (int i = 0; i < node->template_str.part_count - 1; i++) {
                    if (node->template_str.exprs[i]) ast_node_free(node->template_str.exprs[i]);
                }
                free(node->template_str.exprs);
            }
            if (node->template_str.format_types) free(node->template_str.format_types);
            break;
        default:
            break;
    }
    free(node);
}
