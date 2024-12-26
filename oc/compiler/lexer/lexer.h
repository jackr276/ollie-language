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
	IF = 0,
	THEN,
	ELSE,
	DO,
	WHILE,
	FOR,
	TRUE,
	FALSE,
	FUNC,
	RET,
	STATIC,
	EXTERNAL,
	REF,
	DEREF,
	MEMADDR,
	D_AND, /* && */
	D_OR,
	S_AND, /* & */
	S_OR,
	DOT,
	PLUS,
	MINUS,
	STAR,
	F_SLASH,
	MOD,
	L_NOT,
	B_NOT,
	IDENT,
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


//We will use this to keep track of what the current lexer state is
typedef enum {
	START,
	IN_IDENT,
	IN_INT,
	IN_FLOAT,
	IN_STRING,
	IN_COMMENT
} Lex_state;

typedef struct Lexer_item Lexer_item;

struct Lexer_item{
	//The token associated with this item
	Token tok;
	//The string(lexeme) that got us this token
	const char* lexeme;
	//The line number of the source that we found it on
	u_int16_t line_num;
};

//Grab the next token from this file
Lexer_item get_next_token(FILE* fl);
//Print a token out
void print_token(Lexer_item* l);
#endif /* LEXER_H */
