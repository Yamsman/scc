#ifndef SYM_TABLE_H
#define SYM_TABLE_H

#include "err.h"
#include "util/map.h"
#include "util/vector.h"

enum TYPE_KIND {
	TYPE_UNDEF,
	TYPE_MACRO,
	TYPE_LABEL,
	TYPE_PTR,
	TYPE_FUNC,

	TYPE_VOID,
	TYPE_INT,
	TYPE_FLOAT,

	TYPE_STRUCT,
	TYPE_UNION
};

enum TYPE_SIZE {
	SIZE_CHAR = 1,
	SIZE_SHORT = 2,
	SIZE_LONG = 4,
	SIZE_LONG_LONG = 8
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
	int size;		//Variable size
	
	int s_class;		//Variable class
	int is_signed;		//Variable qualifiers
	int is_const;
	int is_volatile;

	//Pointer data
	struct TYPE *ref;	//Next in reference chain
	int ref_len;		//Length if array

	//Function/struct data
	struct VECTOR param;	//List of function parameters/struct members
	struct TYPE *ret;	//Return type
} s_type;

//Used for both function parameters & struct members
typedef struct PARAM {
	char *name;		//Parameter name
	struct TYPE *type;	//Parameter type
	struct TYPE *btype;	//Base type
} s_param;

typedef struct SYMBOL {
	char *name;		//Symbol name
	struct TYPE *type;	//Type (with reference chain)
	struct TYPE *btype;	//Base type (last node in reference chain)

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

//Symtable control functions
void symtable_init(struct SYMTABLE *stb);
void symtable_close(struct SYMTABLE *stb);
void symtable_scope_enter(struct SYMTABLE *stb);
void symtable_scope_leave(struct SYMTABLE *stb);
struct SYMBOL *symtable_def(struct SYMTABLE *stb, char *name, struct TYPE *type, struct SRC_POS *loc);
void symtable_def_label(struct SYMTABLE *stb, char *name, struct SRC_POS *loc);
void symtable_undef(struct SYMTABLE *stb, char *name);
struct SYMBOL *symtable_search(struct SYMTABLE *stb, char *name);

//Symbol-type helper functions
struct SYMBOL *sym_new();
void sym_del(struct SYMBOL *s);
struct TYPE *type_new(int kind);
void type_del(struct TYPE *t);
struct TYPE *type_clone(struct TYPE *from);
int type_compare(struct TYPE *a, struct TYPE *b);

struct PARAM *param_new();
void param_del(struct PARAM *p);
void memb_del(struct PARAM *m);

#endif
