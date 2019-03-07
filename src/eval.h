#ifndef EVAL_H
#define EVAL_H

#include "ast.h"

int eval_constexpr(struct LEXER *lx, struct VECTOR *input, int *err);
int eval_constexpr_ast(struct AST_NODE *root, int *err);

#endif
