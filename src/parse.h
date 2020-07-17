#ifndef PARSE_H
#define PARSE_H

struct AST_NODE *parse();
struct AST_NODE *parse_decl(struct LEXER *lx);

#endif
