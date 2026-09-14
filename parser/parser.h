#ifndef JAG_PARSER_H
#define JAG_PARSER_H

#include "lexer/lexer.h"
#include "ast/ast.h"

typedef struct {
    Lexer lexer;
    Token current;
    Token previous;
    bool had_error;
    bool panic_mode;
} Parser;

void parser_init(Parser *parser, const char *source);
ASTNode *parse_program(Parser *parser);

#endif
