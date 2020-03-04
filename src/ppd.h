#ifndef PPD_H
#define PPD_H

void ppd_define(struct LEXER *lx);
void ppd_error(struct LEXER *lx, int is_warn);
void ppd_include(struct LEXER *lx);
void ppd_undef(struct LEXER *lx);

void ppd_if(struct LEXER *lx);
void ppd_ifdef(struct LEXER *lx);
void ppd_ifndef(struct LEXER *lx);
void ppd_elif(struct LEXER *lx);
void ppd_else(struct LEXER *lx);
void ppd_endif(struct LEXER *lx);

#endif
