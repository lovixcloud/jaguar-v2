#include "vm.h"
#include "runtime/http/http.h"
#include "runtime/ws/ws.h"
#include "runtime/async/async.h"
#include "runtime/worker/worker.h"
#include "runtime/json/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct VMScope {
    char **keys;
    JagValue **vals;
    size_t count;
    size_t capacity;
    struct VMScope *parent;
} VMScope;

static VMScope *vm_scope_new(VMScope *parent) {
    VMScope *s = (VMScope *)calloc(1, sizeof(VMScope));
    s->parent = parent;
    return s;
}

static void vm_scope_set(VMScope *s, const char *key, JagValue *val) {
    for (size_t i = 0; i < s->count; i++) {
        if (strcmp(s->keys[i], key) == 0) {
            jag_val_free(s->vals[i]);
            s->vals[i] = jag_val_dup(val);
            return;
        }
    }
    if (s->count + 1 > s->capacity) {
        s->capacity = s->capacity == 0 ? 8 : s->capacity * 2;
        s->keys = (char **)realloc(s->keys, sizeof(char *) * s->capacity);
        s->vals = (JagValue **)realloc(s->vals, sizeof(JagValue *) * s->capacity);
    }
    s->keys[s->count] = strdup(key);
    s->vals[s->count] = jag_val_dup(val);
    s->count++;
}

static JagValue *vm_scope_get(VMScope *s, const char *key) {
    while (s) {
        for (size_t i = 0; i < s->count; i++) {
            if (strcmp(s->keys[i], key) == 0) {
                return s->vals[i];
            }
        }
        s = s->parent;
    }
    return NULL;
}

static VMScope *g_global_scope = NULL;

void vm_init(VM *vm, bool live_mode) {
    vm->live_mode = live_mode;
    if (!g_global_scope) {
        g_global_scope = vm_scope_new(NULL);
    }
}

static JagValue *eval_node(VMScope *scope, ASTNode *node) {
    if (!node) return jag_val_null();

    switch (node->kind) {
        case AST_PROGRAM: {
            JagValue *last = jag_val_null();
            for (size_t i = 0; i < node->program.stmts.count; i++) {
                jag_val_free(last);
                last = eval_node(scope, node->program.stmts.nodes[i]);
            }
            return last;
        }

        case AST_VAR_DECL:
        case AST_CONST_DECL: {
            JagValue *val = node->var_decl.init ? eval_node(scope, node->var_decl.init) : jag_val_null();
            vm_scope_set(scope, node->var_decl.name, val);
            jag_val_free(val);
            return jag_val_null();
        }

        case AST_LITERAL_NUM: return jag_val_num(node->num_val);
        case AST_LITERAL_DECIMAL: return jag_val_decimal(node->decimal_val);
        case AST_LITERAL_SCIFI: return jag_val_scifi(node->decimal_val);
        case AST_LITERAL_STRING: return jag_val_string(node->string_val);
        case AST_LITERAL_BOOL: return jag_val_bool(node->bool_val);

        case AST_IDENTIFIER: {
            JagValue *v = vm_scope_get(scope, node->string_val);
            return v ? jag_val_dup(v) : jag_val_null();
        }

        case AST_EXPR_STMT:
            return eval_node(scope, node->expr_stmt.expr);

        case AST_CALL_EXPR: {
            if (node->call.callee->kind == AST_MEMBER_EXPR) {
                ASTNode *obj = node->call.callee->member.object;
                const char *mem = node->call.callee->member.member;

                if (obj->kind == AST_IDENTIFIER && strcmp(obj->string_val, "live") == 0) {
                    if (strcmp(mem, "on") == 0 && node->call.arg_count > 0) {
                        JagValue *v = eval_node(scope, node->call.args[0]);
                        jag_live_on(v);
                        jag_val_free(v);
                        return jag_val_null();
                    }
                    if (strcmp(mem, "deg") == 0 && node->call.arg_count >= 3) {
                        JagValue *v1 = eval_node(scope, node->call.args[0]);
                        JagValue *v2 = eval_node(scope, node->call.args[1]);
                        JagValue *v3 = eval_node(scope, node->call.args[2]);
                        jag_live_deg(v1->string, v2->string, v3);
                        jag_val_free(v1); jag_val_free(v2); jag_val_free(v3);
                        return jag_val_null();
                    }
                }

                if (obj->kind == AST_IDENTIFIER && strcmp(obj->string_val, "env") == 0) {
                    if (strcmp(mem, "get") == 0) {
                        if (node->call.arg_count > 1) {
                            return eval_node(scope, node->call.args[1]);
                        }
                    }
                }

                if (obj->kind == AST_IDENTIFIER && strcmp(obj->string_val, "server") == 0) {
                    if (strcmp(mem, "on") == 0 && node->call.arg_count > 0) {
                        JagValue *v = eval_node(scope, node->call.args[0]);
                        jag_server_on((int)v->num, NULL);
                        jag_val_free(v);
                        return jag_val_null();
                    }
                    if (strcmp(mem, "listen") == 0) {
                        jag_server_listen();
                        return jag_val_null();
                    }
                }

                if (obj->kind == AST_IDENTIFIER && strcmp(obj->string_val, "socket") == 0) {
                    if (strcmp(mem, "on") == 0 && node->call.arg_count > 0) {
                        JagValue *v = eval_node(scope, node->call.args[0]);
                        jag_socket_on((int)v->num);
                        jag_val_free(v);
                        return jag_val_null();
                    }
                    if (strcmp(mem, "listen") == 0) {
                        jag_socket_listen();
                        return jag_val_null();
                    }
                }

                if (obj->kind == AST_IDENTIFIER && strcmp(obj->string_val, "http") == 0) {
                    if (strcmp(mem, "get") == 0 && node->call.arg_count > 0) {
                        JagValue *url = eval_node(scope, node->call.args[0]);
                        JagValue *res = jag_http_get(url->string);
                        jag_val_free(url);
                        return res;
                    }
                }
            }
            return jag_val_null();
        }

        default:
            return jag_val_null();
    }
}

JagValue *vm_eval_ast(VM *vm, ASTNode *node) {
    (void)vm;
    return eval_node(g_global_scope, node);
}
