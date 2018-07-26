#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "sym.h"
#include "lex.h"
#include "ppd.h"
#include "util/map.h"

void ppd_define(lexer *lx) {
	//Read macro name
	token mname = lex_peek(lx);
	if (mname.type != TOK_IDENT)
		puts("invalid macro name");

	//Get bounds of macro body
	char *cur = lx->tgt->pos;
	char *start, *end;
	while (isspace(*cur)) cur++;
	start = cur;
	while (*cur != '\n') {
		if (*cur == '\\' && *(cur+1) == '\n')
			cur++;
		cur++;
	}
	end = cur;
	while (isspace(*(cur-1))) cur--;

	//Copy body to buffer
	int len = cur - start;
	char *buf = malloc(len+1);
	memcpy(buf, start, len);
	buf[len] = '\0';
	for (int i=0; i<len; i++) {
		if (buf[0] == '#' && buf[1] == '#' ||
				buf[len-2] == '#' && buf[len-1] == '#')
			puts("'##' at beginning or end of macro");
	}

	//Add to symtable
	symbol *sym = symtable_def(&lx->stb, mname.str, type_new(TYPE_MACRO));
	sym->mac_exp = buf;
	lx->tgt->pos = end;
	return;
}

void ppd_if(lexer *lx) {

}

void ppd_error(lexer *lx) {

}

void ppd_ifdef(lexer *lx) {
	
}

void ppd_ifndef(lexer *lx) {

}

void ppd_include(lexer *lx) {
	char *cur = lx->tgt->pos;
	while (*cur == ' ') cur++;

	//Get include type
	char type = *cur++;
	if (type != '<' && type != '"')
		printf("ERROR: #include expects <fname> or \"fname\"\n");

	//Get filename
	char *begin = cur;
	char endc = (type == '"') ? '"' : '>';
	while (*cur != endc) {
		if (*cur == '\n') {
			printf("ERROR: Expected '%c' before end of line\n", endc);
			return;
		} else if (*cur == '\0') {
			printf("ERROR: Expected '%c' before end of file\n", endc);
			return;
		}
		cur++;
	}
	int fnlen = (cur++) - begin;
	char *fname = malloc(fnlen+1);
	memcpy(fname, begin, fnlen);

	//Attempt to locate and open the included file
	FILE *input_f = NULL;
	char *fdir = malloc(fnlen + sizeof("/usr/include/") + 1);
	fdir[0] = '\0';
	if (type == '"') {
		char *slash = strrchr(fname, '/');
		if (!slash) {
			strcpy(fdir, "./");
			strcat(fdir, fname);
		} else {
			memcpy(fdir, fname, slash - fname);
			fdir[slash-fname] = '\0';
			strcat(fdir, slash);
		}
		printf("%s\n", fdir);
		if (lex_open_file(lx, fdir)) return;
	}
	fdir[0] = '\0';

	//Attempt system include directory
	strcpy(fdir, "/usr/include/");
	strcat(fdir, fname);
	printf("%s\n", fdir);
	if (!lex_open_file(lx, fdir)) {
		printf("ERROR: Unable to open file '%s': ", fname);
		fflush(stdout);
		perror(NULL);
	}
	return;
}

void ppd_undef(lexer *lx) {
	//Read macro name
	token mname = lex_peek(lx);
	if (mname.type != TOK_IDENT)
		printf("ERROR: Invalid macro name '%s'\n", mname.str);

	symbol *s = symtable_search(&lx->stb, mname.str);
	if (s == NULL) {
		printf("ERROR: Macro '%s' is not defined\n", mname.str);
		return;
	}

}
