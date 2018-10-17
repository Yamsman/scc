#include <stdio.h>
#include <stdlib.h>
#include "ast.h"

struct SRC_POS tmp = {0, 0, NULL, NULL}; //temporary 
ast_n *astn_new(int kind, int s_kind) {
	ast_n *node = calloc(1, sizeof(struct AST_NODE));
	node->kind = kind;

	switch (kind) {
		case DECL:
			node->dat.decl.kind = s_kind;
			break;
		case EXPR:
			node->dat.expr.kind = s_kind;
			break;
		case STMT:
			node->dat.stmt.kind = s_kind;
			break;
	}
	node->loc = tmp;

	return node;
}

void astn_del(ast_n *node) {
	if (node == NULL)
		return;

	//note: don't delete symbols here
	switch (node->kind) {
		case DECL:
			astn_del(node->dat.decl.init);
			astn_del(node->dat.decl.block);
			astn_del(node->dat.decl.next);
			break;
		case EXPR:
			astn_del(node->dat.expr.left);
			astn_del(node->dat.expr.right);
			break;
		case STMT:
			if (node->dat.stmt.lbl != NULL)
				free(node->dat.stmt.lbl);
			astn_del(node->dat.stmt.expr);
			astn_del(node->dat.stmt.body);
			astn_del(node->dat.stmt.else_body);
			break;
	}
	astn_del(node->next);

	free(node);
	return;
}
