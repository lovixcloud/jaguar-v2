#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void advance(Parser *p) {
    p->previous = p->current;
    for (;;) {
        p->current = lexer_next_token(&p->lexer);
        if (p->current.type != TOK_ERROR) break;
        printf("Syntax Error at line %d: %s\n", p->current.line, p->current.text);
        p->had_error = true;
    }
}

static bool check(Parser *p, TokenType type) {
    return p->current.type == type;
}

static bool match(Parser *p, TokenType type) {
    if (!check(p, type)) return false;
    advance(p);
    return true;
}

static void consume(Parser *p, TokenType type, const char *message) {
    if (p->current.type == type) {
        advance(p);
        return;
    }
    printf("Parser Error at line %d, col %d: %s (got %s)\n",
           p->current.line, p->current.column, message, token_type_name(p->current.type));
    p->had_error = true;
}

static JagType *parse_type(Parser *p);
static ASTNode *parse_expression(Parser *p);
static ASTNode *parse_statement(Parser *p);
static ASTNode *parse_block(Parser *p);

static JagType *parse_type(Parser *p) {
    JagTypeKind kind = JAG_TYPE_UNKNOWN;
    char *name = NULL;

    if (match(p, TOK_TYPE_STRING)) kind = JAG_TYPE_STRING;
    else if (match(p, TOK_TYPE_NUM)) kind = JAG_TYPE_NUM;
    else if (match(p, TOK_TYPE_DECIMAL)) kind = JAG_TYPE_DECIMAL;
    else if (match(p, TOK_TYPE_BOOL)) kind = JAG_TYPE_BOOL;
    else if (match(p, TOK_TYPE_SCIFI)) kind = JAG_TYPE_SCIFI;
    else if (match(p, TOK_TYPE_DATA)) kind = JAG_TYPE_DATA;
    else if (match(p, TOK_TYPE_MIXED_LIST)) kind = JAG_TYPE_MIXED_LIST;
    else if (match(p, TOK_TYPE_WORKER)) kind = JAG_TYPE_WORKER;
    else if (match(p, TOK_TYPE_SOCKET)) kind = JAG_TYPE_SOCKET;
    else if (match(p, TOK_TYPE_LIST)) kind = JAG_TYPE_LIST;
    else if (match(p, TOK_TYPE_TASK)) kind = JAG_TYPE_TASK;
    else if (match(p, TOK_VECTOR)) kind = JAG_TYPE_VECTOR;
    else if (match(p, TOK_MATRIX)) kind = JAG_TYPE_MATRIX;
    else if (match(p, TOK_IDENTIFIER)) {
        kind = JAG_TYPE_STRUCT;
        name = strdup(p->previous.text);
    } else {
        return NULL;
    }

    JagType *type = jag_type_new(kind);
    if (name) type->name = name;

    if (match(p, TOK_LT)) {
        JagType *elem = parse_type(p);
        consume(p, TOK_GT, "Expected '>' after generic type parameter");
        type->element_type = elem;
    }

    return type;
}

static ASTNode *parse_primary(Parser *p) {
    if (match(p, TOK_NUM_LITERAL)) {
        ASTNode *node = ast_node_new(AST_LITERAL_NUM, p->previous.line, p->previous.column);
        node->num_val = p->previous.num_val;
        return node;
    }
    if (match(p, TOK_DECIMAL_LITERAL)) {
        ASTNode *node = ast_node_new(AST_LITERAL_DECIMAL, p->previous.line, p->previous.column);
        node->decimal_val = p->previous.decimal_val;
        return node;
    }
    if (match(p, TOK_SCIFI_LITERAL)) {
        ASTNode *node = ast_node_new(AST_LITERAL_SCIFI, p->previous.line, p->previous.column);
        node->decimal_val = p->previous.decimal_val;
        return node;
    }
    if (match(p, TOK_STRING_LITERAL)) {
        ASTNode *node = ast_node_new(AST_LITERAL_STRING, p->previous.line, p->previous.column);
        node->string_val = strdup(p->previous.text);
        return node;
    }
    if (match(p, TOK_TRUE) || match(p, TOK_FALSE)) {
        ASTNode *node = ast_node_new(AST_LITERAL_BOOL, p->previous.line, p->previous.column);
        node->bool_val = p->previous.type == TOK_TRUE;
        return node;
    }
    if (match(p, TOK_IDENTIFIER)) {
        ASTNode *node = ast_node_new(AST_IDENTIFIER, p->previous.line, p->previous.column);
        node->string_val = strdup(p->previous.text);
        return node;
    }
    if (match(p, TOK_LPAREN)) {
        ASTNode *expr = parse_expression(p);
        consume(p, TOK_RPAREN, "Expected ')' after expression");
        return expr;
    }
    if (match(p, TOK_LBRACK)) {
        ASTNode *node = ast_node_new(AST_LITERAL_LIST, p->previous.line, p->previous.column);
        ASTNode **elems = NULL;
        int count = 0;
        if (!check(p, TOK_RBRACK)) {
            do {
                elems = (ASTNode **)realloc(elems, sizeof(ASTNode *) * (count + 1));
                elems[count++] = parse_expression(p);
            } while (match(p, TOK_COMMA));
        }
        consume(p, TOK_RBRACK, "Expected ']' after list elements");
        node->list_literal.elements = elems;
        node->list_literal.element_count = count;
        return node;
    }
    if (match(p, TOK_LBRACE)) {
        ASTNode *node = ast_node_new(AST_LITERAL_DATA, p->previous.line, p->previous.column);
        NamedASTNode *entries = NULL;
        int count = 0;
        if (!check(p, TOK_RBRACE)) {
            do {
                char *key = NULL;
                if (match(p, TOK_STRING_LITERAL) || match(p, TOK_IDENTIFIER)) {
                    key = strdup(p->previous.text);
                } else {
                    consume(p, TOK_STRING_LITERAL, "Expected key string/identifier in map literal");
                }
                consume(p, TOK_COLON, "Expected ':' after key in map literal");
                ASTNode *val = parse_expression(p);
                entries = (NamedASTNode *)realloc(entries, sizeof(NamedASTNode) * (count + 1));
                entries[count].key = key;
                entries[count].value = val;
                entries[count].type = NULL;
                count++;
            } while (match(p, TOK_COMMA));
        }
        consume(p, TOK_RBRACE, "Expected '}' after map entries");
        node->data_literal.entries = entries;
        node->data_literal.entry_count = count;
        return node;
    }
    if (check(p, TOK_TEMPLATE_HEAD) || check(p, TOK_TEMPLATE_SINGLE_HEAD) || check(p, TOK_TEMPLATE_DOLLAR_HEAD)) {
        ASTNode *node = ast_node_new(AST_TEMPLATE_STR, p->current.line, p->current.column);
        char **parts = NULL;
        ASTNode **exprs = NULL;
        int *formats = NULL;
        int count = 0;

        advance(p);
        int fmt = p->previous.template_format;
        parts = (char **)realloc(parts, sizeof(char *) * (count + 1));
        parts[count] = strdup(p->previous.text);

        exprs = (ASTNode **)realloc(exprs, sizeof(ASTNode *) * (count + 1));
        exprs[count] = parse_expression(p);

        formats = (int *)realloc(formats, sizeof(int) * (count + 1));
        formats[count] = fmt;
        count++;

        while (check(p, TOK_TEMPLATE_MIDDLE) || check(p, TOK_TEMPLATE_SINGLE_MIDDLE) || check(p, TOK_TEMPLATE_DOLLAR_MIDDLE)) {
            advance(p);
            parts = (char **)realloc(parts, sizeof(char *) * (count + 1));
            parts[count] = strdup(p->previous.text);
            exprs = (ASTNode **)realloc(exprs, sizeof(ASTNode *) * (count + 1));
            exprs[count] = parse_expression(p);
            formats = (int *)realloc(formats, sizeof(int) * (count + 1));
            formats[count] = p->previous.template_format;
            count++;
        }

        if (check(p, TOK_TEMPLATE_TAIL) || check(p, TOK_TEMPLATE_SINGLE_TAIL) || check(p, TOK_TEMPLATE_DOLLAR_TAIL)) {
            advance(p);
            parts = (char **)realloc(parts, sizeof(char *) * (count + 1));
            parts[count] = strdup(p->previous.text);
        } else {
            consume(p, TOK_TEMPLATE_TAIL, "Expected template tail string");
        }

        node->template_str.parts = parts;
        node->template_str.exprs = exprs;
        node->template_str.format_types = formats;
        node->template_str.part_count = count + 1;
        return node;
    }
    if (match(p, TOK_FUN) || match(p, TOK_ASYNC)) {
        bool is_async = (p->previous.type == TOK_ASYNC);
        if (is_async) consume(p, TOK_FUN, "Expected 'fun' after 'async'");
        ASTNode *node = ast_node_new(AST_LAMBDA_EXPR, p->previous.line, p->previous.column);
        node->lambda.is_async = is_async;

        consume(p, TOK_LPAREN, "Expected '(' for lambda parameters");
        NamedASTNode *params = NULL;
        int pcount = 0;
        if (!check(p, TOK_RPAREN)) {
            do {
                consume(p, TOK_IDENTIFIER, "Expected parameter name");
                char *pname = strdup(p->previous.text);
                JagType *ptype = NULL;
                if (match(p, TOK_COLON)) {
                    ptype = parse_type(p);
                }
                params = (NamedASTNode *)realloc(params, sizeof(NamedASTNode) * (pcount + 1));
                params[pcount].key = pname;
                params[pcount].type = ptype;
                params[pcount].value = NULL;
                pcount++;
            } while (match(p, TOK_COMMA));
        }
        consume(p, TOK_RPAREN, "Expected ')' after lambda parameters");

        JagType *ret_type = NULL;
        if (match(p, TOK_COLON)) {
            ret_type = parse_type(p);
        }

        ASTNode *body = parse_block(p);
        node->lambda.params = params;
        node->lambda.param_count = pcount;
        node->lambda.return_type = ret_type;
        node->lambda.body = body;
        return node;
    }

    consume(p, TOK_IDENTIFIER, "Expected expression");
    return NULL;
}

static ASTNode *parse_postfix(Parser *p) {
    ASTNode *expr = parse_primary(p);

    for (;;) {
        if (match(p, TOK_DOT)) {
            consume(p, TOK_IDENTIFIER, "Expected member name after '.'");
            ASTNode *mem = ast_node_new(AST_MEMBER_EXPR, p->previous.line, p->previous.column);
            mem->member.object = expr;
            mem->member.member = strdup(p->previous.text);
            expr = mem;
        } else if (match(p, TOK_LBRACK)) {
            ASTNode *idx_expr = parse_expression(p);
            consume(p, TOK_RBRACK, "Expected ']' after index");
            ASTNode *idx = ast_node_new(AST_INDEX_EXPR, p->previous.line, p->previous.column);
            idx->index.object = expr;
            idx->index.index = idx_expr;
            expr = idx;
        } else if (match(p, TOK_LPAREN)) {
            ASTNode **args = NULL;
            int arg_count = 0;
            if (!check(p, TOK_RPAREN)) {
                do {
                    args = (ASTNode **)realloc(args, sizeof(ASTNode *) * (arg_count + 1));
                    args[arg_count++] = parse_expression(p);
                } while (match(p, TOK_COMMA));
            }
            consume(p, TOK_RPAREN, "Expected ')' after arguments");
            ASTNode *call = ast_node_new(AST_CALL_EXPR, p->previous.line, p->previous.column);
            call->call.callee = expr;
            call->call.args = args;
            call->call.arg_count = arg_count;
            expr = call;
        } else {
            break;
        }
    }
    return expr;
}

static ASTNode *parse_unary(Parser *p) {
    if (match(p, TOK_AWAIT)) {
        ASTNode *node = ast_node_new(AST_AWAIT_EXPR, p->previous.line, p->previous.column);
        node->await_expr.expr = parse_unary(p);
        return node;
    }
    if (match(p, TOK_MINUS) || match(p, TOK_NEQ)) {
        ASTNode *node = ast_node_new(AST_UNARY_EXPR, p->previous.line, p->previous.column);
        node->unary.op = p->previous.type;
        node->unary.operand = parse_unary(p);
        return node;
    }
    return parse_postfix(p);
}

static ASTNode *parse_binary(Parser *p, int min_prec) {
    ASTNode *left = parse_unary(p);

    for (;;) {
        BinaryOp op;
        int prec = 0;

        if (check(p, TOK_PLUS) || check(p, TOK_MINUS)) { prec = 10; op = (p->current.type == TOK_PLUS) ? OP_ADD : OP_SUB; }
        else if (check(p, TOK_STAR) || check(p, TOK_SLASH) || check(p, TOK_PERCENT)) {
            prec = 20;
            if (p->current.type == TOK_STAR) op = OP_MUL;
            else if (p->current.type == TOK_SLASH) op = OP_DIV;
            else op = OP_MOD;
        } else if (check(p, TOK_POW)) { prec = 30; op = OP_POW; }
        else if (check(p, TOK_GT) || check(p, TOK_LT) || check(p, TOK_GTE) || check(p, TOK_LTE) || check(p, TOK_EQ) || check(p, TOK_NEQ)) {
            prec = 5;
            if (p->current.type == TOK_GT) op = OP_GT;
            else if (p->current.type == TOK_LT) op = OP_LT;
            else if (p->current.type == TOK_GTE) op = OP_GTE;
            else if (p->current.type == TOK_LTE) op = OP_LTE;
            else if (p->current.type == TOK_EQ) op = OP_EQ;
            else op = OP_NEQ;
        } else if (check(p, TOK_BETWEEN)) {
            advance(p);
            consume(p, TOK_LPAREN, "Expected '(' after '<<<'");
            ASTNode *low = parse_expression(p);
            consume(p, TOK_COMMA, "Expected ',' in '<<<' range");
            ASTNode *high = parse_expression(p);
            consume(p, TOK_RPAREN, "Expected ')' after '<<<' range");
            ASTNode *bnode = ast_node_new(AST_BETWEEN_EXPR, p->previous.line, p->previous.column);
            bnode->between.val = left;
            bnode->between.low = low;
            bnode->between.high = high;
            left = bnode;
            continue;
        }

        if (prec < min_prec) break;

        advance(p);
        ASTNode *right = parse_binary(p, prec + 1);
        ASTNode *bnode = ast_node_new(AST_BINARY_EXPR, p->previous.line, p->previous.column);
        bnode->binary.op = op;
        bnode->binary.left = left;
        bnode->binary.right = right;
        left = bnode;
    }

    return left;
}

static ASTNode *parse_expression(Parser *p) {
    ASTNode *expr = parse_binary(p, 1);

    if (match(p, TOK_QUESTION)) {
        ASTNode *then_expr = parse_expression(p);
        consume(p, TOK_COLON, "Expected ':' in ternary expression");
        ASTNode *else_expr = parse_expression(p);
        ASTNode *ternary = ast_node_new(AST_TERNARY_EXPR, p->previous.line, p->previous.column);
        ternary->ternary.cond = expr;
        ternary->ternary.then_expr = then_expr;
        ternary->ternary.else_expr = else_expr;
        return ternary;
    }

    if (match(p, TOK_ASSIGN) || match(p, TOK_PLUS_ASSIGN) || match(p, TOK_MINUS_ASSIGN) ||
        match(p, TOK_STAR_ASSIGN) || match(p, TOK_SLASH_ASSIGN) || match(p, TOK_PERCENT_ASSIGN) || match(p, TOK_POW_ASSIGN)) {
        TokenType assign_tok = p->previous.type;
        ASTNode *val = parse_expression(p);
        ASTNode *assign = ast_node_new(AST_ASSIGNMENT, p->previous.line, p->previous.column);
        assign->assignment.target = expr;
        assign->assignment.value = val;
        assign->assignment.aug_op = assign_tok;
        return assign;
    }

    return expr;
}

static ASTNode *parse_block(Parser *p) {
    consume(p, TOK_LBRACE, "Expected '{' before block");
    ASTNode *block = ast_node_new(AST_BLOCK, p->previous.line, p->previous.column);
    ast_node_list_init(&block->block.stmts);

    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        ast_node_list_append(&block->block.stmts, parse_statement(p));
    }

    consume(p, TOK_RBRACE, "Expected '}' after block");
    return block;
}

static ASTNode *parse_statement(Parser *p) {
    if (match(p, TOK_IMPORT)) {
        char *path = NULL;
        char *imported_sym = NULL;

        if (match(p, TOK_STRING_LITERAL)) {
            path = strdup(p->previous.text);
        } else if (match(p, TOK_IDENTIFIER)) {
            imported_sym = strdup(p->previous.text);
            consume(p, TOK_FROM, "Expected 'from' after import symbol");
            consume(p, TOK_STRING_LITERAL, "Expected import file path");
            path = strdup(p->previous.text);
        }
        match(p, TOK_SEMICOLON);

        ASTNode *node = ast_node_new(AST_IMPORT_DECL, p->previous.line, p->previous.column);
        node->import_decl.path = path;
        node->import_decl.imported_symbol = imported_sym;
        return node;
    }

    if (match(p, TOK_VAR) || match(p, TOK_FIXED)) {
        bool is_fixed = (p->previous.type == TOK_FIXED);
        consume(p, TOK_IDENTIFIER, "Expected variable name");
        char *vname = strdup(p->previous.text);

        JagType *type = NULL;
        if (match(p, TOK_COLON)) {
            type = parse_type(p);
        }

        ASTNode *init = NULL;
        if (match(p, TOK_ASSIGN)) {
            init = parse_expression(p);
        }

        match(p, TOK_SEMICOLON);

        ASTNode *node = ast_node_new(is_fixed ? AST_CONST_DECL : AST_VAR_DECL, p->previous.line, p->previous.column);
        node->var_decl.name = vname;
        node->var_decl.type = type;
        node->var_decl.init = init;
        node->var_decl.is_fixed = is_fixed;
        return node;
    }

    if (match(p, TOK_FUN) || match(p, TOK_ASYNC)) {
        bool is_async = (p->previous.type == TOK_ASYNC);
        if (is_async) consume(p, TOK_FUN, "Expected 'fun' after 'async'");

        consume(p, TOK_IDENTIFIER, "Expected function name");
        char *fname = strdup(p->previous.text);

        consume(p, TOK_LPAREN, "Expected '(' for function parameters");
        NamedASTNode *params = NULL;
        int pcount = 0;
        if (!check(p, TOK_RPAREN)) {
            do {
                consume(p, TOK_IDENTIFIER, "Expected parameter name");
                char *pname = strdup(p->previous.text);
                JagType *ptype = NULL;
                if (match(p, TOK_COLON)) {
                    ptype = parse_type(p);
                }
                params = (NamedASTNode *)realloc(params, sizeof(NamedASTNode) * (pcount + 1));
                params[pcount].key = pname;
                params[pcount].type = ptype;
                params[pcount].value = NULL;
                pcount++;
            } while (match(p, TOK_COMMA));
        }
        consume(p, TOK_RPAREN, "Expected ')' after parameters");

        JagType *ret_type = NULL;
        if (match(p, TOK_COLON)) {
            ret_type = parse_type(p);
        }

        ASTNode *body = parse_block(p);

        ASTNode *node = ast_node_new(AST_FUNC_DECL, p->previous.line, p->previous.column);
        node->func_decl.name = fname;
        node->func_decl.is_async = is_async;
        node->func_decl.params = params;
        node->func_decl.param_count = pcount;
        node->func_decl.return_type = ret_type;
        node->func_decl.body = body;
        return node;
    }

    if (match(p, TOK_IF)) {
        consume(p, TOK_LPAREN, "Expected '(' after 'if'");
        ASTNode *cond = parse_expression(p);
        consume(p, TOK_RPAREN, "Expected ')' after condition");
        ASTNode *then_branch = parse_block(p);
        ASTNode *else_branch = NULL;

        if (match(p, TOK_ELIF)) {
            p->current.type = TOK_IF;
            else_branch = parse_statement(p);
        } else if (match(p, TOK_ELSE)) {
            else_branch = parse_block(p);
        }

        ASTNode *node = ast_node_new(AST_IF_STMT, p->previous.line, p->previous.column);
        node->if_stmt.cond = cond;
        node->if_stmt.then_branch = then_branch;
        node->if_stmt.else_branch = else_branch;
        return node;
    }

    if (match(p, TOK_LOOP)) {
        consume(p, TOK_LPAREN, "Expected '(' after 'loop'");
        ASTNode *cond = parse_expression(p);
        consume(p, TOK_RPAREN, "Expected ')' after condition");
        ASTNode *body = parse_block(p);
        ASTNode *node = ast_node_new(AST_LOOP_STMT, p->previous.line, p->previous.column);
        node->loop_stmt.cond = cond;
        node->loop_stmt.body = body;
        return node;
    }

    if (match(p, TOK_DO)) {
        consume(p, TOK_LOOP, "Expected 'loop' after 'do'");
        ASTNode *body = parse_block(p);
        consume(p, TOK_ELSE, "Expected 'while' after 'do loop' body");
        consume(p, TOK_LPAREN, "Expected '(' for do loop condition");
        ASTNode *cond = parse_expression(p);
        consume(p, TOK_RPAREN, "Expected ')' after do loop condition");
        match(p, TOK_SEMICOLON);

        ASTNode *node = ast_node_new(AST_DO_LOOP_STMT, p->previous.line, p->previous.column);
        node->do_loop_stmt.cond = cond;
        node->do_loop_stmt.body = body;
        return node;
    }

    if (match(p, TOK_FOR)) {
        consume(p, TOK_LPAREN, "Expected '(' after 'for'");
        consume(p, TOK_IDENTIFIER, "Expected loop variable name");
        char *vname = strdup(p->previous.text);
        consume(p, TOK_IN, "Expected 'in' in for loop");
        ASTNode *coll = parse_expression(p);
        consume(p, TOK_RPAREN, "Expected ')' after for loop specification");
        ASTNode *body = parse_block(p);

        ASTNode *node = ast_node_new(AST_FOR_IN_STMT, p->previous.line, p->previous.column);
        node->for_in_stmt.var_name = vname;
        node->for_in_stmt.collection = coll;
        node->for_in_stmt.body = body;
        return node;
    }

    if (match(p, TOK_ITERATE)) {
        consume(p, TOK_LPAREN, "Expected '(' after 'iterate'");
        ASTNode *coll = parse_expression(p);
        consume(p, TOK_COMMA, "Expected ',' after collection in iterate");
        consume(p, TOK_IDENTIFIER, "Expected item variable name in iterate");
        char *iname = strdup(p->previous.text);
        consume(p, TOK_RPAREN, "Expected ')' after iterate specification");
        ASTNode *body = parse_block(p);

        ASTNode *node = ast_node_new(AST_ITERATE_STMT, p->previous.line, p->previous.column);
        node->iterate_stmt.collection = coll;
        node->iterate_stmt.item_name = iname;
        node->iterate_stmt.body = body;
        return node;
    }

    if (match(p, TOK_RETURN)) {
        ASTNode *expr = NULL;
        if (!check(p, TOK_SEMICOLON) && !check(p, TOK_RBRACE)) {
            expr = parse_expression(p);
        }
        match(p, TOK_SEMICOLON);
        ASTNode *node = ast_node_new(AST_RETURN_STMT, p->previous.line, p->previous.column);
        node->return_stmt.expr = expr;
        return node;
    }

    if (match(p, TOK_TRY)) {
        ASTNode *try_block = parse_block(p);
        consume(p, TOK_CATCH, "Expected 'catch' after 'try' block");
        consume(p, TOK_LPAREN, "Expected '(' after 'catch'");
        consume(p, TOK_IDENTIFIER, "Expected error variable name in catch");
        char *err_name = strdup(p->previous.text);
        JagType *err_type = NULL;
        if (match(p, TOK_COLON)) {
            err_type = parse_type(p);
        }
        consume(p, TOK_RPAREN, "Expected ')' after catch parameter");
        ASTNode *catch_block = parse_block(p);

        ASTNode *node = ast_node_new(AST_TRY_CATCH_STMT, p->previous.line, p->previous.column);
        node->try_catch_stmt.try_block = try_block;
        node->try_catch_stmt.err_var_name = err_name;
        node->try_catch_stmt.err_type = err_type;
        node->try_catch_stmt.catch_block = catch_block;
        return node;
    }

    ASTNode *expr = parse_expression(p);
    match(p, TOK_SEMICOLON);
    ASTNode *node = ast_node_new(AST_EXPR_STMT, p->previous.line, p->previous.column);
    node->expr_stmt.expr = expr;
    return node;
}

void parser_init(Parser *p, const char *source) {
    lexer_init(&p->lexer, source);
    p->had_error = false;
    p->panic_mode = false;
    advance(p);
}

ASTNode *parse_program(Parser *p) {
    ASTNode *prog = ast_node_new(AST_PROGRAM, 1, 1);
    ast_node_list_init(&prog->program.stmts);

    while (!check(p, TOK_EOF)) {
        ASTNode *stmt = parse_statement(p);
        if (stmt) {
            ast_node_list_append(&prog->program.stmts, stmt);
        }
    }
    return prog;
}
