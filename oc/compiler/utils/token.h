/**
 * Author: Jack Robbins
 *
 * This structure defines all of the tokens used in the language
*/

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

