/**
 * Expose all needed lexer APIs to anywhere else that needs it
*/

//Include guards
#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>
#include <sys/types.h>
#include "../dynamic_string/dynamic_string.h"

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
	REPLACE,
	//For preprocessor sections
	DEPENDENCIES,
	ELSE,
	DO,
	WHILE,
	UNION,
	FOR,
	AT,
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
	PLUSEQ, // +=
	MINUSEQ, // -=
	STAREQ, // *=
	SLASHEQ, // /=
	MODEQ, // %=
	OREQ, // |=
	ANDEQ, // &=
	XOREQ, // ^=
	LSHIFTEQ, // <<=
	RSHIFTEQ, // >>=
	DOT,
	PLUS,
	LIB,
	PLUSPLUS,
	DEFINE,
	AS,
	ENUM,
	STRUCT,
	MINUS,
	MINUSMINUS,
	STAR,
	F_SLASH,
	MOD,
	L_NOT,
	B_NOT,
	IDENT,
	POUND, /* # */
	FUNC_CONST,
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
	QUESTION, /* ? */
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
	DOLLAR, /* $ */
	ARROW,
	ERROR,
	DONE,
	VOID,
	SIGNED_INT_CONST, //Generic type as a signed integer constant
	UNSIGNED_INT_CONST, //Generic type as an unsigned integer constant
	U8,
	I8,
	U16,
	I16,
	U32,
	I32,
	U64,
	I64,
	F32,
	F64,
	CHAR,
	PUB,
} Token;

//The lexitem_t struct
typedef struct lexitem_t lexitem_t;


struct lexitem_t{
	//The string(lexeme) that got us this token
	dynamic_string_t lexeme;
	//The line number of the source that we found it on
	u_int16_t line_num;
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
lexitem_t get_next_assembly_statement(FILE* fl);

/**
 * Generic token grabbing function
 */
lexitem_t get_next_token(FILE* fl, u_int16_t* parser_line_num, const_search_t const_search);

/**
 * Push a token back to the stream
 */
void push_back_token(lexitem_t l);

/**
 * Developer utility for token printing
 */
void print_token(lexitem_t* l);

/**
 * A utility function for error printing that converts an operator to a string
 */
char* operator_to_string(Token op);

#endif /* LEXER_H */
