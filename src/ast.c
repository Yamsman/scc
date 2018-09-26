#include <stdio.h>
#include <stdlib.h>
#include "ast.h"

ast_n *astn_new(int kind, int s_kind) {
	ast_n *node = malloc(sizeof(struct AST_NODE));
	node->kind = kind;
	node->next = NULL;

	switch (kind) {
		case DECL:
			node->dat.decl.kind = s_kind;
			node->dat.decl.sym = NULL;
			node->dat.decl.init = NULL;
			node->dat.decl.block = NULL;
			node->dat.decl.next = NULL;
			break;
		case EXPR:
			node->dat.expr.kind = s_kind;
			node->dat.expr.sym = NULL;
			node->dat.expr.left = NULL;
			node->dat.expr.right = NULL;
			break;
		case STMT:
			node->dat.stmt.kind = s_kind;
			node->dat.stmt.expr = NULL;
			node->dat.stmt.body = NULL;
			node->dat.stmt.else_body = NULL;
			break;
	}

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
