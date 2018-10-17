#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "err.h"

int c_errflag = 0;

void c_error(s_pos *pos, const char *msg, ...) {
	if (pos != NULL) {
		//fprintf(stderr, "%s:%i:%i: ",
		//	pos->fname, pos->line, pos->col);
		fprintf(stderr, "NOFILE:%i:%i: ",
			pos->line, pos->col);
	}
	va_list args;
	va_start(args, msg);
	fputs("ERROR: ", stderr);
	vfprintf(stderr, msg, args);
	va_end(args);

	c_errflag = 1;
	return;
}

void c_warn(s_pos *pos, const char *msg, ...) {
	if (pos != NULL) {
		fprintf(stderr, "%s:%i:%i: ",
			pos->fname, pos->line, pos->col);
	}
	va_list args;
	va_start(args, msg);
	fputs("WARNING: ", stderr);
	vfprintf(stderr, msg, args);
	va_end(args);

	return;
}
