#ifndef ERR_H
#define ERR_H

typedef struct SRC_POS {
	int line;	//Line number
	int col;	//Column number
	char *lbegin;	//Pointer to beginning of line
	char *fname;	//Filename
} s_pos;

extern int c_errflag;
void c_error(struct SRC_POS *pos, const char *msg, ...);
void c_warn(struct SRC_POS *pos, const char *msg, ...);

#endif
