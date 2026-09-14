#include "transpiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_node(ASTNode *node, FILE *out);

static void emit_node(ASTNode *node, FILE *out) {
    if (!node) return;

    switch (node->kind) {
        case AST_PROGRAM:
            fprintf(out, "#define _GNU_SOURCE\n");
            fprintf(out, "#include \"runtime/core/value.h\"\n");
            fprintf(out, "#include \"runtime/http/http.h\"\n");
            fprintf(out, "#include \"runtime/ws/ws.h\"\n");
            fprintf(out, "#include \"runtime/async/async.h\"\n");
            fprintf(out, "#include \"runtime/worker/worker.h\"\n");
            fprintf(out, "#include \"runtime/json/json.h\"\n");
            fprintf(out, "#include <stdio.h>\n");
            fprintf(out, "#include <stdlib.h>\n\n");
            fprintf(out, "int main(void) {\n");
            fprintf(out, "    event_loop_init();\n");

            for (size_t i = 0; i < node->program.stmts.count; i++) {
                emit_node(node->program.stmts.nodes[i], out);
            }

            fprintf(out, "    return 0;\n");
            fprintf(out, "}\n");
            break;

        case AST_VAR_DECL:
        case AST_CONST_DECL:
            fprintf(out, "    JagValue *%s = ", node->var_decl.name);
            if (node->var_decl.init) {
                emit_node(node->var_decl.init, out);
            } else {
                fprintf(out, "jag_val_null()");
            }
            fprintf(out, ";\n");
            break;

        case AST_EXPR_STMT:
            fprintf(out, "    ");
            emit_node(node->expr_stmt.expr, out);
            fprintf(out, ";\n");
            break;

        case AST_CALL_EXPR:
            if (node->call.callee->kind == AST_MEMBER_EXPR) {
                ASTNode *obj = node->call.callee->member.object;
                const char *mem = node->call.callee->member.member;

                if (obj->kind == AST_IDENTIFIER && strcmp(obj->string_val, "live") == 0) {
                    if (strcmp(mem, "on") == 0 && node->call.arg_count > 0) {
                        fprintf(out, "jag_live_on(");
                        emit_node(node->call.args[0], out);
                        fprintf(out, ")");
                        return;
                    }
                }
                if (obj->kind == AST_IDENTIFIER && strcmp(obj->string_val, "env") == 0) {
                    if (strcmp(mem, "get") == 0 && node->call.arg_count > 1) {
                        emit_node(node->call.args[1], out);
                        return;
                    }
                }
                if (obj->kind == AST_IDENTIFIER && strcmp(obj->string_val, "server") == 0) {
                    if (strcmp(mem, "on") == 0 && node->call.arg_count > 0) {
                        fprintf(out, "jag_server_on(");
                        emit_node(node->call.args[0], out);
                        fprintf(out, "->num, NULL)");
                        return;
                    }
                    if (strcmp(mem, "listen") == 0) {
                        fprintf(out, "jag_server_listen()");
                        return;
                    }
                }
            }
            fprintf(out, "jag_val_null()");
            break;

        case AST_LITERAL_STRING:
            fprintf(out, "jag_val_string(\"%s\")", node->string_val);
            break;

        case AST_LITERAL_NUM:
            fprintf(out, "jag_val_num(%ld)", (long)node->num_val);
            break;

        case AST_IDENTIFIER:
            fprintf(out, "%s", node->string_val);
            break;

        default:
            fprintf(out, "jag_val_null()");
            break;
    }
}

bool transpile_ast_to_c(ASTNode *node, FILE *out) {
    if (!node || !out) return false;
    emit_node(node, out);
    return true;
}
