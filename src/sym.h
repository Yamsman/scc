#ifndef SYM_TABLE_H
#define SYM_TABLE_H

#include "util/map.h"
#include "util/vector.h"

enum TYPE_KIND {
	TYPE_UNDEF,
	TYPE_MACRO,
	TYPE_LABEL,
	TYPE_PTR,
	TYPE_FUNC,

	TYPE_VOID,
	TYPE_CHAR,
	TYPE_SHORT,
	TYPE_INT,
	TYPE_LONG,
	TYPE_FLOAT,
	TYPE_DOUBLE,

	TYPE_STRUCT,
	TYPE_UNION
};

enum TYPE_CLASS {
	CLASS_UNDEF,
	//CLASS_TYPEDEF
	CLASS_EXTERN,
	CLASS_STATIC,
	CLASS_AUTO,
	CLASS_REGISTER
};

typedef struct TYPE {
	int kind;		//Variable kind
	int s_class;		//Variable class
	
	int is_signed;		//Variable qualifiers
	int is_const;
	int is_volatile;

	//Pointer data
	struct TYPE *ref;	//Next in reference chain
	int ref_len;		//Length if array

	//Function data
	struct PARAM *param;	//Beginning of parameter list
	struct TYPE *ret;	//Return type

	//Struct/union data
	struct PARAM *memb;	//List of struct members
} s_type;

//Layout is identical for both function parameters & struct members
typedef struct PARAM {
	char *name;		//Parameter name
	struct TYPE *type;	//Parameter type
	struct PARAM *next;	//Next parameter in list
} s_param;

typedef struct SYMBOL {
	char *name;		//Symbol name
	struct TYPE *type;	//Type (with reference chain)
	struct TYPE *btype;	//Base type

	//Data
	struct AST_N *fbody;	//Function body
	struct VECTOR lvars;	//Function variables
	struct MAP labels;	//Function labels

	char *mac_exp;		//Macro expansion
} symbol;

typedef struct SCOPE {
	struct MAP table;	//Map containing symbols
	struct SCOPE *prev;	//Previous scope
} scope;

typedef struct SYMTABLE {
	struct SCOPE *s_cur;	//Current scope
	struct SCOPE *s_global;	//Global scope
	struct SYMBOL *func;	//Current function
} symtable;

void symtable_init(struct SYMTABLE *stb);
void symtable_close(struct SYMTABLE *stb);
void symtable_scope_enter(struct SYMTABLE *stb);
void symtable_scope_leave(struct SYMTABLE *stb);
struct SYMBOL *symtable_def(struct SYMTABLE *stb, char *name, struct TYPE *type);
struct SYMBOL *symtable_def_label(symtable *stb, char *name);
void symtable_undef(symtable *stb, char *name);
struct SYMBOL *symtable_search(struct SYMTABLE *stb, char *name);

//Symbol-type helper functions
struct TYPE *type_new(int kind);
struct TYPE *type_clone(struct TYPE *from);
int type_compare(struct TYPE *a, struct TYPE *b);
void type_del(struct TYPE *t);

struct PARAM *param_new();
void param_del(struct PARAM *p);

#endif
