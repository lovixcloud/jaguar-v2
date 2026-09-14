#ifndef JAG_AST_H
#define JAG_AST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    JAG_TYPE_UNKNOWN,
    JAG_TYPE_VOID,
    JAG_TYPE_STRING,
    JAG_TYPE_NUM,
    JAG_TYPE_DECIMAL,
    JAG_TYPE_BOOL,
    JAG_TYPE_SCIFI,
    JAG_TYPE_DATA,
    JAG_TYPE_LIST,
    JAG_TYPE_MIXED_LIST,
    JAG_TYPE_ENUM,
    JAG_TYPE_STRUCT,
    JAG_TYPE_VECTOR,
    JAG_TYPE_MATRIX,
    JAG_TYPE_TASK,
    JAG_TYPE_WORKER,
    JAG_TYPE_SOCKET,
    JAG_TYPE_FUNCTION,
    JAG_TYPE_ANY
} JagTypeKind;

typedef struct JagType {
    JagTypeKind kind;
    char *name;
    struct JagType *element_type;
    struct JagType *return_type;
    struct JagType **param_types;
    int param_count;
} JagType;

JagType *jag_type_new(JagTypeKind kind);
JagType *jag_type_new_generic(JagTypeKind kind, JagType *elem_type);
void jag_type_free(JagType *type);
bool jag_type_equals(JagType *a, JagType *b);
char *jag_type_to_string(JagType *type);

typedef enum {
    AST_PROGRAM,
    AST_VAR_DECL,
    AST_CONST_DECL,
    AST_FUNC_DECL,
    AST_CLASS_DECL,
    AST_ENUM_DECL,
    AST_STRUCT_DECL,
    AST_IMPORT_DECL,
    AST_EXPORT_DECL,

    AST_BLOCK,
    AST_EXPR_STMT,
    AST_IF_STMT,
    AST_LOOP_STMT,
    AST_DO_LOOP_STMT,
    AST_FOR_IN_STMT,
    AST_ITERATE_STMT,
    AST_RETURN_STMT,
    AST_TRY_CATCH_STMT,
    AST_LIVE_ON_STMT,
    AST_LIVE_DEG_STMT,
    AST_SERVER_ON,
    AST_SERVER_ROUTE,
    AST_SERVER_LISTEN,
    AST_SOCKET_ON,
    AST_SOCKET_ROUTE,
    AST_SOCKET_LISTEN,

    AST_LITERAL_NUM,
    AST_LITERAL_DECIMAL,
    AST_LITERAL_STRING,
    AST_LITERAL_BOOL,
    AST_LITERAL_SCIFI,
    AST_LITERAL_DATA,
    AST_LITERAL_LIST,
    AST_IDENTIFIER,
    AST_BINARY_EXPR,
    AST_UNARY_EXPR,
    AST_TERNARY_EXPR,
    AST_ASSIGNMENT,
    AST_CALL_EXPR,
    AST_MEMBER_EXPR,
    AST_INDEX_EXPR,
    AST_AWAIT_EXPR,
    AST_LAMBDA_EXPR,
    AST_BETWEEN_EXPR,
    AST_TEMPLATE_STR
} ASTNodeKind;

typedef struct ASTNode ASTNode;

typedef struct ASTNodeList {
    ASTNode **nodes;
    size_t count;
    size_t capacity;
} ASTNodeList;

void ast_node_list_init(ASTNodeList *list);
void ast_node_list_append(ASTNodeList *list, ASTNode *node);
void ast_node_list_free(ASTNodeList *list);

typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_POW,
    OP_EQ, OP_NEQ, OP_GT, OP_LT, OP_GTE, OP_LTE,
    OP_AND, OP_OR, OP_BIT_SHL, OP_BIT_SHR
} BinaryOp;

typedef struct {
    char *key;
    ASTNode *value;
    JagType *type;
} NamedASTNode;

struct ASTNode {
    ASTNodeKind kind;
    int line;
    int column;
    JagType *inferred_type;

    union {
        struct { ASTNodeList stmts; } program;
        struct { ASTNodeList stmts; } block;
        struct { char *name; JagType *type; ASTNode *init; bool is_fixed; } var_decl;
        struct { char *name; bool is_async; NamedASTNode *params; int param_count; JagType *return_type; ASTNode *body; } func_decl;
        struct { char *name; int visibility; ASTNodeList fields; } class_decl;
        struct { char *name; char **values; int value_count; } enum_decl;
        struct { char *name; NamedASTNode *fields; int field_count; } struct_decl;
        struct { char *path; char *imported_symbol; } import_decl;
        struct { char *symbol; } export_decl;
        struct { ASTNode *cond; ASTNode *then_branch; ASTNode *else_branch; } if_stmt;
        struct { ASTNode *cond; ASTNode *body; } loop_stmt;
        struct { ASTNode *cond; ASTNode *body; } do_loop_stmt;
        struct { char *var_name; ASTNode *collection; ASTNode *guard_cond; ASTNode *body; } for_in_stmt;
        struct { ASTNode *collection; char *item_name; ASTNode *body; } iterate_stmt;
        struct { ASTNode *expr; } return_stmt;
        struct { ASTNode *try_block; char *err_var_name; JagType *err_type; ASTNode *catch_block; } try_catch_stmt;
        struct { ASTNode *expr; } live_on;
        struct { char *expected_type; char *label; ASTNode *expr; } live_deg;
        struct { ASTNode *port_expr; ASTNode *options_data; } server_on;
        struct { ASTNode *method_expr; ASTNode *path_expr; ASTNode *handler_expr; } server_route;
        struct { ASTNode *port_expr; } socket_on;
        struct { ASTNode *path_expr; ASTNode *handler_expr; } socket_route;

        int64_t num_val;
        double decimal_val;
        char *string_val;
        bool bool_val;

        struct { NamedASTNode *entries; int entry_count; } data_literal;
        struct { ASTNode **elements; int element_count; JagType *element_type; } list_literal;

        char *identifier;

        struct { BinaryOp op; ASTNode *left; ASTNode *right; } binary;
        struct { int op; ASTNode *operand; } unary;
        struct { ASTNode *cond; ASTNode *then_expr; ASTNode *else_expr; } ternary;
        struct { ASTNode *target; ASTNode *value; int aug_op; } assignment;
        struct { ASTNode *callee; ASTNode **args; int arg_count; } call;
        struct { ASTNode *object; char *member; } member;
        struct { ASTNode *object; ASTNode *index; } index;
        struct { ASTNode *expr; } await_expr;
        struct { bool is_async; NamedASTNode *params; int param_count; JagType *return_type; ASTNode *body; } lambda;
        struct { ASTNode *val; ASTNode *low; ASTNode *high; } between;
        struct { char **parts; ASTNode **exprs; int part_count; int *format_types; } template_str;
        struct { ASTNode *expr; } expr_stmt;
    };
};

ASTNode *ast_node_new(ASTNodeKind kind, int line, int col);
void ast_node_free(ASTNode *node);

#endif
