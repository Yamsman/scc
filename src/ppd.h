#ifndef PPD_H
#define PPD_H

void ppd_define(lexer *lx);
void ppd_error(lexer *lx);
void ppd_include(lexer *lx);
void ppd_undef(lexer *lx);

void ppd_if(lexer *lx);
void ppd_ifdef(lexer *lx);
void ppd_ifndef(lexer *lx);
void ppd_elif(lexer *lx);
void ppd_else(lexer *lx);
void ppd_endif(lexer *lx);

#endif
