/**
 * Author: Jack Robbins
 *
 * This structure defines all of the tokens used in the language
*/

#ifndef OLLIE_TOKEN_H
#define OLLIE_TOKEN_H

#include <sys/types.h>
#include "dynamic_string/dynamic_string.h"

//Forward declare the lexitem struct
typedef struct lexitem_t lexitem_t;

/**
 * All valid tokens in Ollie
 */
typedef enum {
	BLANK = 0,
	START, /* start token */
	LET,
	DECLARE,
	BOOL,
	ALIAS,
	WHEN,
	IDLE,
	MUT,
	DEFER,
	ASM,
	ASM_STATEMENT,
	IF,
	MACRO,
	ENDMACRO,
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
	SHORT_CONST,
	SHORT_CONST_FORCE_U,
	BYTE_CONST,
	BYTE_CONST_FORCE_U,
	LONG_CONST,
	FLOAT_CONST,
	DOUBLE_CONST,
	STR_CONST,
	REL_ADDRESS_CONST, // For when we store things like .LC pointers(think global char*)
	CHAR_CONST,
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
	COMMA,
	SEMICOLON,
	DOLLAR, /* $ */
	ARROW, /* -> */
	FAT_ARROW, /* => */
	ERROR,
	DONE,
	VOID,
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
	TRUE_CONST,
	FALSE_CONST,
	INLINE,
} ollie_token_t;


/**
 * The lexitem_t struct holds everything that we could possibly
 * need for one lexitem in the language
 */
struct lexitem_t {
	//The string(lexeme) that got us this token
	dynamic_string_t lexeme;
	//This union will hold all of the constant values
	//that a lexitem could possibly have
	union {
		double double_value;
		float float_value;
		u_int64_t unsigned_long_value;
		int64_t signed_long_value;
		u_int32_t unsigned_int_value;
		int32_t signed_int_value;
		u_int16_t unsigned_short_value;
		int16_t signed_short_value;
		u_int8_t unsigned_byte_value;
		int8_t signed_byte_value;
		char char_value;
	} constant_values;
	//The line number of the source that we found it on
	u_int32_t line_num;
	//The token associated with this item
	ollie_token_t tok;
	//Should this lexitem be ignored? This is mainly used
	//by the preprocessor during it's traversal of the token
	//streams
	u_int8_t ignore;
};

#endif /* OLLIE_TOKEN_H */
