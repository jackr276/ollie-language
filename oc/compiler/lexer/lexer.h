/**
 * Expose all needed lexer APIs to anywhere else that needs it
*/

//Include guards
#ifndef LEXER_H
#define LEXER_H

//All tokens that we can possible see
//This list may grow as we go forward
#include <stdio.h>
typedef enum {
	IF,
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
	L_AND,
	L_OR,
	B_AND,
	B_OR,
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
	L_SHIFT,
	R_SHIFT,
	EQUALS,
	NOT_EQUALS,
	G_THAN,
	L_THAN,
	COLON,
	ERROR,
	DONE,
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
	char* lexeme;
	//The line number of the source that we found it on
	int line_num;
};

//Is this lexeme an identifier or a reserved keyword
Lexer_item identifier_or_keyword(const char* lexeme, int line_number);
//Grab the next token from this file
Lexer_item get_next_token(FILE* fl);

#endif /* LEXER_H */
