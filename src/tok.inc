/*
 * Token types
 */
tok( TOK_END,		"TOK_END",		"EOF"		)
tok( TOK_SEM,		"TOK_SEM",		";"		)
tok( TOK_COL,		"TOK_COL",		":"		)
tok( TOK_CMM,		"TOK_CMM",		","		)
tok( TOK_PRD,		"TOK_PRD",		"."		)
tok( TOK_LBR,		"TOK_LBR",		"{"		)
tok( TOK_RBR,		"TOK_RBR",		"}"		)
tok( TOK_LPR,		"TOK_LPR",		"("		)
tok( TOK_RPR,		"TOK_RPR",		")"		)
tok( TOK_LBK,		"TOK_LBK",		"["		)
tok( TOK_RBK,		"TOK_RBK",		"]"		)
tok( TOK_SQT,		"TOK_SQT",		"'"		)
tok( TOK_DQT,		"TOK_DQT",		"\""		)
tok( TOK_SNS,		"TOK_SNS",		"#"		)
tok( TOK_DNS,		"TOK_DNS",		"##"		)
tok( TOK_QMK,		"TOK_QMK",		"?"		)
tok( TOK_ADD,		"TOK_ADD",		"+"		)
tok( TOK_SUB,		"TOK_SUB",		"-"		)
tok( TOK_ASR,		"TOK_ASR",		"*"		)
tok( TOK_DIV,		"TOK_DIV",		"/"		)
tok( TOK_MOD,		"TOK_MOD",		"%"		)
tok( TOK_AND,		"TOK_AND",		"&"		)
tok( TOK_OR,		"TOK_OR",		"|"		)
tok( TOK_XOR,		"TOK_XOR",		"^"		)
tok( TOK_TLD,		"TOK_TLD",		"~"		)
tok( TOK_LTH,		"TOK_LTH",		"<"		)
tok( TOK_GTH,		"TOK_GTH",		">"		)
tok( TOK_LEQ,		"TOK_LEQ",		"<="		)
tok( TOK_GEQ,		"TOK_GEQ",		">="		)
tok( TOK_EQ,		"TOK_EQ",		"=="		)
tok( TOK_NEQ,		"TOK_NEQ",		"!="		)
tok( TOK_LOGIC_OR,	"TOK_LOGIC_OR",		"||"		)
tok( TOK_LOGIC_AND,	"TOK_LOGIC_AND",	"&&"		)
tok( TOK_NOT,		"TOK_NOT",		"!"		)
tok( TOK_INC,		"TOK_INC",		"++"		)
tok( TOK_DEC,		"TOK_DEC",		"--"		)
tok( TOK_PTR,		"TOK_PTR",		"->"		)
tok( TOK_LSHIFT,	"TOK_LSHIFT",		"<<"		)
tok( TOK_RSHIFT,	"TOK_RSHIFT",		">>"		)
tok( TOK_ASSIGN,	"TOK_ASSIGN",		"="		)
tok( TOK_ASSIGN_ADD,	"TOK_ASSIGN_ADD",	"+="		)
tok( TOK_ASSIGN_SUB,	"TOK_ASSIGN_SUB",	"-="		)
tok( TOK_ASSIGN_MUL,	"TOK_ASSIGN_MUL",	"*="		)
tok( TOK_ASSIGN_DIV,	"TOK_ASSIGN_DIV",	"/="		)
tok( TOK_ASSIGN_MOD,	"TOK_ASSIGN_MOD",	"%="		)
tok( TOK_ASSIGN_AND,	"TOK_ASSIGN_AND",	"&="		)
tok( TOK_ASSIGN_OR,	"TOK_ASSIGN_OR",	"|="		)
tok( TOK_ASSIGN_XOR,	"TOK_ASSIGN_XOR",	"^="		)
tok( TOK_ASSIGN_LSHIFT,	"TOK_ASSIGN_LSHIFT",	"<<="		)
tok( TOK_ASSIGN_RSHIFT,	"TOK_ASSIGN_RSHIFT",	">>="		)
tok( TOK_DEFINED,	"TOK_DEFINED",		"defined"	)
tok( TOK_CONST,		"TOK_CONST",		NULL		)
tok( TOK_STR,		"TOK_STR",		NULL		)
tok( TOK_IDENT,		"TOK_IDENT",		NULL		)

/*
 * Keywords
 */
kw( TOK_KW_IF, 		"TOK_KW_IF",		"if"		)
kw( TOK_KW_ELSE, 	"TOK_KW_ELSE",		"else"		)
kw( TOK_KW_SWITCH, 	"TOK_KW_SWITCH",	"switch"	)
kw( TOK_KW_WHILE, 	"TOK_KW_WHILE",		"while"		)
kw( TOK_KW_FOR, 	"TOK_KW_FOR",		"for"		)
kw( TOK_KW_DO, 		"TOK_KW_DO",		"do"		)
kw( TOK_KW_CASE, 	"TOK_KW_CASE",		"case"		)
kw( TOK_KW_DEFAULT, 	"TOK_KW_DEFAULT",	"default"	)
kw( TOK_KW_GOTO, 	"TOK_KW_GOTO",		"goto"		)
kw( TOK_KW_CONTINUE, 	"TOK_KW_CONTINUE",	"continue"	)
kw( TOK_KW_BREAK, 	"TOK_KW_BREAK",		"break"		)
kw( TOK_KW_RETURN, 	"TOK_KW_RETURN",	"return"	)
kw( TOK_KW_VOID, 	"TOK_KW_VOID",		"void"		)
kw( TOK_KW_CHAR, 	"TOK_KW_CHAR",		"char"		)
kw( TOK_KW_SHORT, 	"TOK_KW_SHORT",		"short"		)
kw( TOK_KW_INT, 	"TOK_KW_INT",		"int"		)
kw( TOK_KW_LONG, 	"TOK_KW_LONG",		"long"		)
kw( TOK_KW_FLOAT, 	"TOK_KW_FLOAT",		"float"		)
kw( TOK_KW_DOUBLE, 	"TOK_KW_DOUBLE",	"double"	)
kw( TOK_KW_STRUCT, 	"TOK_KW_STRUCT",	"struct"	)
kw( TOK_KW_UNION, 	"TOK_KW_UNION",		"union"		)
kw( TOK_KW_ENUM, 	"TOK_KW_ENUM",		"enum"		)
kw( TOK_KW_TYPEDEF, 	"TOK_KW_TYPEDEF",	"typedef"	)
kw( TOK_KW_EXTERN, 	"TOK_KW_EXTERN",	"extern"	)
kw( TOK_KW_STATIC, 	"TOK_KW_STATIC",	"static"	)
kw( TOK_KW_AUTO, 	"TOK_KW_AUTO",		"auto"		)
kw( TOK_KW_REGISTER, 	"TOK_KW_REGISTER",	"register"	)
kw( TOK_KW_SIGNED, 	"TOK_KW_SIGNED",	"signed"	)
kw( TOK_KW_UNSIGNED, 	"TOK_KW_UNSIGNED",	"unsigned"	)
kw( TOK_KW_CONST, 	"TOK_KW_CONST",		"const"		)
kw( TOK_KW_VOLATILE, 	"TOK_KW_VOLATILE",	"volatile"	)
kw( TOK_KW_DEFINE, 	"TOK_KW_DEFINE",	"define"	)
kw( TOK_KW_ELIF, 	"TOK_KW_ELIF",		"elif"		)
kw( TOK_KW_ENDIF, 	"TOK_KW_ENDIF",		"endif"		)
kw( TOK_KW_ERROR, 	"TOK_KW_ERROR",		"error"		)
kw( TOK_KW_IFDEF, 	"TOK_KW_IFDEF",		"ifdef"		)
kw( TOK_KW_IFNDEF, 	"TOK_KW_IFNDEF",	"ifndef"	)
kw( TOK_KW_INCLUDE, 	"TOK_KW_INCLUDE",	"include"	)
kw( TOK_KW_LINE, 	"TOK_KW_LINE",		"line"		)
kw( TOK_KW_PRAGMA, 	"TOK_KW_PRAGMA",	"pragma"	)
kw( TOK_KW_UNDEF, 	"TOK_KW_UNDEF",		"undef"		)
kw( TOK_KW_WARNING,	"TOK_KW_WARNING",	"warning"	)
