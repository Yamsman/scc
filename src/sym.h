#ifndef SYM_TABLE_H
#define SYM_TABLE_H

//TODO: store string hashes alongside symbols for faster comparison
enum SCOPE_RANGE {
	SCOPE_CURRENT,
	SCOPE_VISIBLE,
	SCOPE_ALL
};

enum TYPE_KIND {
	TYPE_UNDEF,
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
	int kind;
	int s_class;
	
	int is_signed;
	int is_const;
	int is_volatile;

	//Pointer data
	struct TYPE *ref;
	int ref_len;

	//Function data
	struct PARAM *param;
	struct TYPE *ret;

	//Struct/union data
	struct PARAM *memb;	
} s_type;

//Layout is identical for both function parameters & struct members
typedef struct PARAM {
	struct TYPE *type;
	char *name;
	struct PARAM *next;
} s_param;

typedef struct SYMBOL {
	int scope_id;
	char *name;
	struct TYPE *type;
	struct TYPE *btype;
} symbol;

struct SYMBOL_TABLE {
	struct SYMBOL **table;
	int count;
	int max;

	//Stack for managing scope IDs
	int *scope;
	int scope_index;
	int scope_max;
	int scope_nextid;
};

extern struct SYMBOL_TABLE sym;
void symtable_init();
void symtable_close();
void symtable_print();

void symtable_scope_enter();
void symtable_scope_leave();

struct SYMBOL *symtable_add(char *name/*, type */);
struct SYMBOL *symtable_search(char *name, int range);

//Symbol-type helper functions
struct TYPE *type_new();
struct TYPE *type_clone();
void type_del(struct TYPE *t);
struct PARAM *param_new();
void param_del(struct PARAM *p);

#endif
