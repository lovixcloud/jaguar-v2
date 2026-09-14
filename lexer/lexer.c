#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void lexer_init(Lexer *lexer, const char *source) {
    lexer->source = source;
    lexer->length = source ? strlen(source) : 0;
    lexer->cursor = 0;
    lexer->line = 1;
    lexer->column = 1;
    lexer->in_template = false;
    lexer->template_depth = 0;
    lexer->template_mode = 0;
}

static char peek(Lexer *l) {
    if (l->cursor >= l->length) return '\0';
    return l->source[l->cursor];
}

static char peek_next(Lexer *l) {
    if (l->cursor + 1 >= l->length) return '\0';
    return l->source[l->cursor + 1];
}

static char advance(Lexer *l) {
    if (l->cursor >= l->length) return '\0';
    char c = l->source[l->cursor++];
    if (c == '\n') {
        l->line++;
        l->column = 1;
    } else {
        l->column++;
    }
    return c;
}

static Token make_token(Lexer *l, TokenType type, const char *text, int len, int start_line, int start_col) {
    (void)l;
    Token t;
    memset(&t, 0, sizeof(Token));
    t.type = type;
    t.line = start_line;
    t.column = start_col;
    t.length = len;
    if (text) {
        t.text = strndup(text, len);
    } else {
        t.text = NULL;
    }
    return t;
}

void token_free(Token *tok) {
    if (tok->text) {
        free(tok->text);
        tok->text = NULL;
    }
}

static TokenType check_keyword(const char *str, int len) {
    #define KEYWORD(s, tok) if (strlen(s) == (size_t)len && strncmp(str, s, len) == 0) return tok
    KEYWORD("var", TOK_VAR);
    KEYWORD("fixed", TOK_FIXED);
    KEYWORD("fun", TOK_FUN);
    KEYWORD("async", TOK_ASYNC);
    KEYWORD("await", TOK_AWAIT);
    KEYWORD("if", TOK_IF);
    KEYWORD("elif", TOK_ELIF);
    KEYWORD("else", TOK_ELSE);
    KEYWORD("loop", TOK_LOOP);
    KEYWORD("do", TOK_DO);
    KEYWORD("for", TOK_FOR);
    KEYWORD("in", TOK_IN);
    KEYWORD("iterate", TOK_ITERATE);
    KEYWORD("return", TOK_RETURN);
    KEYWORD("try", TOK_TRY);
    KEYWORD("catch", TOK_CATCH);
    KEYWORD("import", TOK_IMPORT);
    KEYWORD("export", TOK_EXPORT);
    KEYWORD("from", TOK_FROM);
    KEYWORD("class", TOK_CLASS);
    KEYWORD("public", TOK_PUBLIC);
    KEYWORD("private", TOK_PRIVATE);
    KEYWORD("enum", TOK_ENUM);
    KEYWORD("struct", TOK_STRUCT);
    KEYWORD("vector", TOK_VECTOR);
    KEYWORD("matrix", TOK_MATRIX);
    KEYWORD("true", TOK_TRUE);
    KEYWORD("false", TOK_FALSE);

    KEYWORD("string", TOK_TYPE_STRING);
    KEYWORD("num", TOK_TYPE_NUM);
    KEYWORD("decimal", TOK_TYPE_DECIMAL);
    KEYWORD("bool", TOK_TYPE_BOOL);
    KEYWORD("scifi", TOK_TYPE_SCIFI);
    KEYWORD("data", TOK_TYPE_DATA);
    KEYWORD("list", TOK_TYPE_LIST);
    KEYWORD("MixedList", TOK_TYPE_MIXED_LIST);
    KEYWORD("Task", TOK_TYPE_TASK);
    KEYWORD("Worker", TOK_TYPE_WORKER);
    KEYWORD("Socket", TOK_TYPE_SOCKET);
    #undef KEYWORD

    return TOK_IDENTIFIER;
}

static void skip_whitespace_and_comments(Lexer *l) {
    while (l->cursor < l->length) {
        char c = peek(l);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance(l);
        } else if (c == '/' && peek_next(l) == '/') {
            while (peek(l) != '\0' && peek(l) != '\n') advance(l);
        } else if (c == '/' && peek_next(l) == '*') {
            advance(l); advance(l);
            while (peek(l) != '\0') {
                if (peek(l) == '*' && peek_next(l) == '/') {
                    advance(l); advance(l);
                    break;
                }
                advance(l);
            }
        } else {
            break;
        }
    }
}

static Token lex_string(Lexer *l, char quote) {
    int start_line = l->line;
    int start_col = l->column;
    advance(l);

    size_t start_pos = l->cursor;
    while (l->cursor < l->length) {
        char c = peek(l);
        if (c == quote) {
            int len = l->cursor - start_pos;
            const char *text = l->source + start_pos;
            advance(l);
            return make_token(l, TOK_STRING_LITERAL, text, len, start_line, start_col);
        }
        if (c == '\\') {
            advance(l);
            if (peek(l) != '\0') advance(l);
            continue;
        }
        if (c == '{' && peek_next(l) == '{') {
            int len = l->cursor - start_pos;
            const char *text = l->source + start_pos;
            advance(l); advance(l);
            l->in_template = true;
            l->template_depth = 1;
            l->template_mode = 0;
            Token tok = make_token(l, TOK_TEMPLATE_HEAD, text, len, start_line, start_col);
            tok.template_format = 0;
            return tok;
        } else if (c == '$' && peek_next(l) == '{') {
            int len = l->cursor - start_pos;
            const char *text = l->source + start_pos;
            advance(l); advance(l);
            l->in_template = true;
            l->template_depth = 1;
            l->template_mode = 2;
            Token tok = make_token(l, TOK_TEMPLATE_DOLLAR_HEAD, text, len, start_line, start_col);
            tok.template_format = 2;
            return tok;
        } else if (c == '{') {
            int len = l->cursor - start_pos;
            const char *text = l->source + start_pos;
            advance(l);
            l->in_template = true;
            l->template_depth = 1;
            l->template_mode = 1;
            Token tok = make_token(l, TOK_TEMPLATE_SINGLE_HEAD, text, len, start_line, start_col);
            tok.template_format = 1;
            return tok;
        }
        advance(l);
    }
    return make_token(l, TOK_ERROR, "Unterminated string", 19, start_line, start_col);
}

static Token lex_string_resume(Lexer *l, char quote) {
    int start_line = l->line;
    int start_col = l->column;
    size_t start_pos = l->cursor;

    while (l->cursor < l->length) {
        char c = peek(l);
        if (c == quote) {
            int len = l->cursor - start_pos;
            const char *text = l->source + start_pos;
            advance(l);
            l->in_template = false;
            Token tok = make_token(l, l->template_mode == 0 ? TOK_TEMPLATE_TAIL :
                                      l->template_mode == 1 ? TOK_TEMPLATE_SINGLE_TAIL : TOK_TEMPLATE_DOLLAR_TAIL,
                                      text, len, start_line, start_col);
            tok.template_format = l->template_mode;
            return tok;
        }
        if (c == '\\') {
            advance(l);
            if (peek(l) != '\0') advance(l);
            continue;
        }
        if (c == '{' && peek_next(l) == '{') {
            int len = l->cursor - start_pos;
            const char *text = l->source + start_pos;
            advance(l); advance(l);
            l->template_depth = 1;
            int old_mode = l->template_mode;
            l->template_mode = 0;
            Token tok = make_token(l, old_mode == 0 ? TOK_TEMPLATE_MIDDLE :
                                      old_mode == 1 ? TOK_TEMPLATE_SINGLE_MIDDLE : TOK_TEMPLATE_DOLLAR_MIDDLE,
                                      text, len, start_line, start_col);
            tok.template_format = old_mode;
            return tok;
        } else if (c == '$' && peek_next(l) == '{') {
            int len = l->cursor - start_pos;
            const char *text = l->source + start_pos;
            advance(l); advance(l);
            l->template_depth = 1;
            int old_mode = l->template_mode;
            l->template_mode = 2;
            Token tok = make_token(l, old_mode == 0 ? TOK_TEMPLATE_MIDDLE :
                                      old_mode == 1 ? TOK_TEMPLATE_SINGLE_MIDDLE : TOK_TEMPLATE_DOLLAR_MIDDLE,
                                      text, len, start_line, start_col);
            tok.template_format = old_mode;
            return tok;
        } else if (c == '{') {
            int len = l->cursor - start_pos;
            const char *text = l->source + start_pos;
            advance(l);
            l->template_depth = 1;
            int old_mode = l->template_mode;
            l->template_mode = 1;
            Token tok = make_token(l, old_mode == 0 ? TOK_TEMPLATE_MIDDLE :
                                      old_mode == 1 ? TOK_TEMPLATE_SINGLE_MIDDLE : TOK_TEMPLATE_DOLLAR_MIDDLE,
                                      text, len, start_line, start_col);
            tok.template_format = old_mode;
            return tok;
        }
        advance(l);
    }
    return make_token(l, TOK_ERROR, "Unterminated template string", 28, start_line, start_col);
}

Token lexer_next_token(Lexer *l) {
    if (l->in_template && l->template_depth == 0) {
        return lex_string_resume(l, '"');
    }

    skip_whitespace_and_comments(l);

    int start_line = l->line;
    int start_col = l->column;

    if (l->cursor >= l->length) {
        return make_token(l, TOK_EOF, NULL, 0, start_line, start_col);
    }

    char c = peek(l);

    if (l->in_template) {
        if (c == '{') l->template_depth++;
        else if (c == '}') {
            if (l->template_mode == 0 && peek_next(l) == '}') {
                l->template_depth--;
                if (l->template_depth == 0) {
                    advance(l); advance(l);
                    return lex_string_resume(l, '"');
                }
            } else if (l->template_mode != 0) {
                l->template_depth--;
                if (l->template_depth == 0) {
                    advance(l);
                    return lex_string_resume(l, '"');
                }
            }
        }
    }

    if (isalpha(c) || c == '_') {
        size_t start_pos = l->cursor;
        while (isalnum(peek(l)) || peek(l) == '_') {
            advance(l);
        }
        int len = l->cursor - start_pos;
        const char *text = l->source + start_pos;
        TokenType type = check_keyword(text, len);
        Token tok = make_token(l, type, text, len, start_line, start_col);
        if (type == TOK_TRUE) tok.bool_val = true;
        if (type == TOK_FALSE) tok.bool_val = false;
        return tok;
    }

    if (isdigit(c)) {
        size_t start_pos = l->cursor;
        bool is_decimal = false;
        bool is_scifi = false;

        while (isdigit(peek(l))) advance(l);

        if (peek(l) == '.' && isdigit(peek_next(l))) {
            is_decimal = true;
            advance(l);
            while (isdigit(peek(l))) advance(l);
        }

        if (peek(l) == 'E' || peek(l) == 'e') {
            is_scifi = true;
            advance(l);
            if (peek(l) == '+' || peek(l) == '-') advance(l);
            while (isdigit(peek(l))) advance(l);
        }

        int len = l->cursor - start_pos;
        const char *text = l->source + start_pos;

        if (is_scifi) {
            Token tok = make_token(l, TOK_SCIFI_LITERAL, text, len, start_line, start_col);
            tok.decimal_val = atof(tok.text);
            return tok;
        } else if (is_decimal) {
            Token tok = make_token(l, TOK_DECIMAL_LITERAL, text, len, start_line, start_col);
            tok.decimal_val = atof(tok.text);
            return tok;
        } else {
            Token tok = make_token(l, TOK_NUM_LITERAL, text, len, start_line, start_col);
            tok.num_val = atoll(tok.text);
            return tok;
        }
    }

    if (c == '"' || c == '\'') {
        return lex_string(l, c);
    }

    advance(l);
    switch (c) {
        case '+':
            if (peek(l) == '=') { advance(l); return make_token(l, TOK_PLUS_ASSIGN, "+=", 2, start_line, start_col); }
            return make_token(l, TOK_PLUS, "+", 1, start_line, start_col);
        case '-':
            if (peek(l) == '=') { advance(l); return make_token(l, TOK_MINUS_ASSIGN, "-=", 2, start_line, start_col); }
            return make_token(l, TOK_MINUS, "-", 1, start_line, start_col);
        case '*':
            if (peek(l) == '*') {
                advance(l);
                if (peek(l) == '=') { advance(l); return make_token(l, TOK_POW_ASSIGN, "**=", 3, start_line, start_col); }
                return make_token(l, TOK_POW, "**", 2, start_line, start_col);
            }
            if (peek(l) == '=') { advance(l); return make_token(l, TOK_STAR_ASSIGN, "*=", 2, start_line, start_col); }
            return make_token(l, TOK_STAR, "*", 1, start_line, start_col);
        case '/':
            if (peek(l) == '=') { advance(l); return make_token(l, TOK_SLASH_ASSIGN, "/=", 2, start_line, start_col); }
            return make_token(l, TOK_SLASH, "/", 1, start_line, start_col);
        case '%':
            if (peek(l) == '=') { advance(l); return make_token(l, TOK_PERCENT_ASSIGN, "%=", 2, start_line, start_col); }
            return make_token(l, TOK_PERCENT, "%", 1, start_line, start_col);
        case '=':
            if (peek(l) == '=') { advance(l); return make_token(l, TOK_EQ, "==", 2, start_line, start_col); }
            return make_token(l, TOK_ASSIGN, "=", 1, start_line, start_col);
        case '!':
            if (peek(l) == '=') { advance(l); return make_token(l, TOK_NEQ, "!=", 2, start_line, start_col); }
            return make_token(l, TOK_ERROR, "Unexpected token !", 1, start_line, start_col);
        case '<':
            if (peek(l) == '<') {
                advance(l);
                if (peek(l) == '<') { advance(l); return make_token(l, TOK_BETWEEN, "<<<", 3, start_line, start_col); }
                return make_token(l, TOK_SHL, "<<", 2, start_line, start_col);
            }
            if (peek(l) == '=') { advance(l); return make_token(l, TOK_LTE, "<=", 2, start_line, start_col); }
            return make_token(l, TOK_LT, "<", 1, start_line, start_col);
        case '>':
            if (peek(l) == '>') { advance(l); return make_token(l, TOK_SHR, ">>", 2, start_line, start_col); }
            if (peek(l) == '=') { advance(l); return make_token(l, TOK_GTE, ">=", 2, start_line, start_col); }
            return make_token(l, TOK_GT, ">", 1, start_line, start_col);
        case '?': return make_token(l, TOK_QUESTION, "?", 1, start_line, start_col);
        case ':': return make_token(l, TOK_COLON, ":", 1, start_line, start_col);
        case ';': return make_token(l, TOK_SEMICOLON, ";", 1, start_line, start_col);
        case ',': return make_token(l, TOK_COMMA, ",", 1, start_line, start_col);
        case '.': return make_token(l, TOK_DOT, ".", 1, start_line, start_col);
        case '(': return make_token(l, TOK_LPAREN, "(", 1, start_line, start_col);
        case ')': return make_token(l, TOK_RPAREN, ")", 1, start_line, start_col);
        case '{': return make_token(l, TOK_LBRACE, "{", 1, start_line, start_col);
        case '}': return make_token(l, TOK_RBRACE, "}", 1, start_line, start_col);
        case '[': return make_token(l, TOK_LBRACK, "[", 1, start_line, start_col);
        case ']': return make_token(l, TOK_RBRACK, "]", 1, start_line, start_col);
        default:
            return make_token(l, TOK_ERROR, "Unknown character", 17, start_line, start_col);
    }
}

const char *token_type_name(TokenType type) {
    switch (type) {
        case TOK_EOF: return "EOF";
        case TOK_IDENTIFIER: return "IDENTIFIER";
        case TOK_NUM_LITERAL: return "NUM_LITERAL";
        case TOK_DECIMAL_LITERAL: return "DECIMAL_LITERAL";
        case TOK_SCIFI_LITERAL: return "SCIFI_LITERAL";
        case TOK_STRING_LITERAL: return "STRING_LITERAL";
        case TOK_TEMPLATE_HEAD: return "TEMPLATE_HEAD";
        case TOK_TEMPLATE_TAIL: return "TEMPLATE_TAIL";
        case TOK_VAR: return "var";
        case TOK_FIXED: return "fixed";
        case TOK_FUN: return "fun";
        case TOK_ASYNC: return "async";
        case TOK_AWAIT: return "await";
        case TOK_BETWEEN: return "<<<";
        default: return "TOKEN";
    }
}
