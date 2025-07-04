/**
 * Expose all needed lexer APIs to anywhere else that needs it
*/

//Include guards
#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>
#include <sys/types.h>

//The maximum token length is 500 
#define MAX_TOKEN_LENGTH 500
#define MAX_IDENT_LENGTH 100

//Are we hunting for a constant?
typedef enum {
	SEARCHING_FOR_CONSTANT,
	NOT_SEARCHING_FOR_CONSTANT
} const_search_t;

//All tokens that we can possible see
//This list may grow as we go forward
typedef enum {
	BLANK = 0,
	START, /* start token */
	LET,
	DECLARE,
	ALIAS,
	WHEN,
	IDLE,
	MUT,
	DEFER,
	ASM,
	ASM_STATEMENT,
	IF,
	THEN,
	REPLACE,
	//For expansion of func params
	DOTDOTDOT,
	//For preprocessor sections
	DEPENDENCIES,
	ELSE,
	DO,
	WHILE,
	FOR,
	AT,
	ARROW_EQ,
	CASE,
	BREAK,
	CONTINUE,
	DEFAULT,
	SWITCH,
	FN,
	REGISTER,
	WITH,
	CONSTANT,
	TYPESIZE,
	SIZEOF,
	REQUIRE,
	RETURN,
	JUMP,
	STATIC,
	EXTERNAL,
	DOUBLE_AND,
	DOUBLE_OR,
	SINGLE_AND, /* & */
	SINGLE_OR,
	COLONEQ, /* := */
	DOT,
	PLUS,
	LIB,
	PLUSPLUS,
	DEFINE,
	AS,
	ENUM,
	CONSTRUCT,
	MINUS,
	MINUSMINUS,
	STAR,
	F_SLASH,
	MOD,
	ON,
	L_NOT,
	B_NOT,
	IDENT,
	LABEL_IDENT, /* Label idents always start with $ */
	POUND, /* # */
	INT_CONST,
	//Forced to unsigned
	INT_CONST_FORCE_U,
	LONG_CONST_FORCE_U,
	LONG_CONST,
	FLOAT_CONST,
	STR_CONST,
	CHAR_CONST,
	HEX_CONST,
	L_PAREN,
	R_PAREN,
	L_CURLY,
	R_CURLY,
	L_BRACKET,
	R_BRACKET,
	L_SHIFT,
	R_SHIFT,
	EQUALS,
	CARROT,
	DOUBLE_EQUALS, /* == */
	NOT_EQUALS,
	G_THAN,
	L_THAN,
	G_THAN_OR_EQ,
	L_THAN_OR_EQ,
	COLON,
	DOUBLE_COLON,
	COMMA,
	SEMICOLON,
	ARROW,
	ERROR,
	DONE,
	VOID,
	SIGNED_INT_CONST, //Generic type as a signed integer constant
	UNSIGNED_INT_CONST, //Generic type as an unsigned integer constant
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
} Token;

typedef struct Lexer_item Lexer_item;

struct Lexer_item{
	//The string(lexeme) that got us this token
	char lexeme[MAX_TOKEN_LENGTH];
	//The line number of the source that we found it on
	u_int16_t line_num;
	//The number of characters in this token
	int16_t char_count;
	//The token associated with this item
	Token tok;
};

/**
 * Reset the entire file for reprocessing
 */
void reset_file(FILE* fl);

/**
 * Special case -- hunting for assembly statements
 */
Lexer_item get_next_assembly_statement(FILE* fl, u_int16_t* parser_line_num);

/**
 * Generic token grabbing function
 */
Lexer_item get_next_token(FILE* fl, u_int16_t* parser_line_num, const_search_t const_search);

/**
 * Push a token back to the stream
 */
void push_back_token(Lexer_item l);

/**
 * Developer utility for token printing
 */
void print_token(Lexer_item* l);

#endif /* LEXER_H */
