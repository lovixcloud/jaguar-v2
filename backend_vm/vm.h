#ifndef JAG_VM_H
#define JAG_VM_H

#include "ast/ast.h"
#include "runtime/core/value.h"

typedef struct {
    bool live_mode;
} VM;

void vm_init(VM *vm, bool live_mode);
JagValue *vm_eval_ast(VM *vm, ASTNode *node);

#endif
