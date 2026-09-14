#ifndef JAG_LEXER_H
#define JAG_LEXER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    TOK_EOF,
    TOK_ERROR,

    TOK_IDENTIFIER,
    TOK_NUM_LITERAL,
    TOK_DECIMAL_LITERAL,
    TOK_SCIFI_LITERAL,
    TOK_STRING_LITERAL,

    TOK_TEMPLATE_HEAD,
    TOK_TEMPLATE_MIDDLE,
    TOK_TEMPLATE_TAIL,
    TOK_TEMPLATE_SINGLE_HEAD,
    TOK_TEMPLATE_SINGLE_MIDDLE,
    TOK_TEMPLATE_SINGLE_TAIL,
    TOK_TEMPLATE_DOLLAR_HEAD,
    TOK_TEMPLATE_DOLLAR_MIDDLE,
    TOK_TEMPLATE_DOLLAR_TAIL,

    TOK_VAR, TOK_FIXED, TOK_FUN, TOK_ASYNC, TOK_AWAIT,
    TOK_IF, TOK_ELIF, TOK_ELSE, TOK_LOOP, TOK_DO,
    TOK_FOR, TOK_IN, TOK_ITERATE, TOK_RETURN, TOK_TRY, TOK_CATCH,
    TOK_IMPORT, TOK_EXPORT, TOK_FROM, TOK_CLASS, TOK_PUBLIC, TOK_PRIVATE,
    TOK_ENUM, TOK_STRUCT, TOK_VECTOR, TOK_MATRIX,
    TOK_TRUE, TOK_FALSE,

    TOK_TYPE_STRING, TOK_TYPE_NUM, TOK_TYPE_DECIMAL, TOK_TYPE_BOOL,
    TOK_TYPE_SCIFI, TOK_TYPE_DATA, TOK_TYPE_LIST, TOK_TYPE_MIXED_LIST,
    TOK_TYPE_TASK, TOK_TYPE_WORKER, TOK_TYPE_SOCKET,

    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT, TOK_POW,
    TOK_PLUS_ASSIGN, TOK_MINUS_ASSIGN, TOK_STAR_ASSIGN, TOK_SLASH_ASSIGN,
    TOK_PERCENT_ASSIGN, TOK_POW_ASSIGN,
    TOK_ASSIGN, TOK_EQ, TOK_NEQ, TOK_GT, TOK_LT, TOK_GTE, TOK_LTE,
    TOK_BETWEEN,
    TOK_SHL, TOK_SHR,
    TOK_QUESTION, TOK_COLON, TOK_SEMICOLON, TOK_COMMA, TOK_DOT,

    TOK_LPAREN, TOK_RPAREN,
    TOK_LBRACE, TOK_RBRACE,
    TOK_LBRACK, TOK_RBRACK
} TokenType;

typedef struct {
    TokenType type;
    char *text;
    int line;
    int column;
    int length;
    union {
        int64_t num_val;
        double decimal_val;
        bool bool_val;
    };
    int template_format;
} Token;

typedef struct {
    const char *source;
    size_t length;
    size_t cursor;
    int line;
    int column;

    bool in_template;
    int template_depth;
    int template_mode;
} Lexer;

void lexer_init(Lexer *lexer, const char *source);
Token lexer_next_token(Lexer *lexer);
void token_free(Token *tok);
const char *token_type_name(TokenType type);

#endif
