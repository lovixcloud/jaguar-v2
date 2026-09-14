#ifndef JAG_TRANSPILER_H
#define JAG_TRANSPILER_H

#include "ast/ast.h"
#include <stdio.h>

bool transpile_ast_to_c(ASTNode *node, FILE *out);

#endif
