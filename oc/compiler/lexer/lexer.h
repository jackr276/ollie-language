/**
 * Expose all needed lexer APIs to anywhere else that needs it
*/

//Include guards
#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>
#include <sys/types.h>

//All tokens that we can possible see
//This list may grow as we go forward
typedef enum {
	BLANK = 0,
	IF,
	THEN,
	ELSE,
	DO,
	WHILE,
	FOR,
	TRUE,
	FALSE,
	FUNC,
	REGISTER,
	CONSTANT,
	LINK,
	COMPTIME,
	RET,
	JUMP,
	STATIC,
	EXTERNAL,
	CONDITIONAL_DEREF, /* ` */
	DOUBLE_AND, /* && */
	DOUBLE_OR,
	AND, /* & */
	OR,
	DOT,
	PLUS,
	DEFINED,
	ENUMERATED,
	SIZE,
	MINUS,
	STAR,
	F_SLASH,
	MOD,
	ON,
	L_NOT,
	B_NOT,
	IDENT,
	POUND, /* # */
	INT_CONST,
	FLOAT_CONST,
	STR_CONST,
	L_PAREN,
	R_PAREN,
	L_CURLY,
	R_CURLY,
	L_BRACKET,
	R_BRACKET,
	L_SHIFT,
	R_SHIFT,
	EQUALS,
	D_EQUALS, /* == */
	NOT_EQUALS,
	DIV_EQUALS,
	PLUS_EQUALS,
	MINUS_EQUALS,
	TIMES_EQUALS,
	G_THAN,
	L_THAN,
	COLON,
	DOUBLE_COLON,
	COMMA,
	SEMICOLON,
	ARROW,
	ERROR,
	DONE,
	U_INT8,
	S_INT8,
	U_INT16,
	S_INT16,
	U_INT32,
	S_INT32,
	U_INT64,
	S_INT64,
	FLOAT32,
	FLOAT64,
	CHAR,
	STR,
} Token;

typedef struct Lexer_item Lexer_item;

struct Lexer_item{
	//The token associated with this item
	Token tok;
	//The string(lexeme) that got us this token
	char* lexeme;
	//The line number of the source that we found it on
	u_int16_t line_num;
	//The number of characters in this token
	int16_t char_count;
};

//Grab the next token from this file
Lexer_item get_next_token(FILE* fl, u_int16_t* parser_line_num);

//Push a token back
void push_back_token(FILE* fl, Lexer_item l);

//Print a token out
void print_token(Lexer_item* l);
#endif /* LEXER_H */
