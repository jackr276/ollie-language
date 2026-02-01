/**
 * Lexical analyzer and tokenizer for Ollie-language
 * GOAL: A lexical analyzer(also called a tokenizer, lexer, etc) runs through a source code file and "chunks" it into 
 * tokens. These tokens represent valid "lexemes" in the language. It will also determine if there are any invalid characters and pass
 * that information along to the parser.
 *
 * There is only one function that is exposed to external files, which is the "get_next_token()" function. This function simply returns
 * the next token from the file. When EOF is reached, a special DONE token is passed along so that the parser knows when to stop
 *
 * OVERALL STRUCTURE: The lexer/semantic analyzer is the very first thing that touches the source code. Once source code leaves the semantic
 * analyzer, it really is no longer source code and is instead a token stream
 *
 * NEXT IN LINE: parser
*/

#include "lexer.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include "../utils/constants.h"

//Total number of keywords
#define KEYWORD_COUNT 53

//We will use this to keep track of what the current lexer state is
typedef enum {
	IN_START,
	IN_IDENT,
	IN_INT,
	IN_FLOAT,
	IN_STRING,
	IN_MULTI_COMMENT,
	IN_SINGLE_COMMENT
} lex_state_t;


/* ============================================= GLOBAL VARIABLES  ============================================ */
//For the file name
static char* file_name;

//For any/all error printing
static char info[2000];

//Token array, we will index using their enum values
static const ollie_token_t tok_array[] = {IF, ELSE, DO, WHILE, FOR, FN, RETURN, JUMP, REQUIRE, REPLACE, 
					U8, I8, U16, I16, U32, I32, U64, I64, F32, F64, CHAR, DEFINE, ENUM,
					REGISTER, CONSTANT, VOID, TYPESIZE, LET, DECLARE, WHEN, CASE, DEFAULT, SWITCH, BREAK, CONTINUE, 
					STRUCT, AS, ALIAS, SIZEOF, DEFER, MUT, DEPENDENCIES, ASM, WITH, LIB, IDLE, PUB, UNION, BOOL,
				    EXTERNAL, TRUE_CONST, FALSE_CONST, INLINE};

//Direct one to one mapping
static const char* keyword_array[] = {"if", "else", "do", "while", "for", "fn", "ret", "jump",
						 "require", "replace", "u8", "i8", "u16",
						 "i16", "u32", "i32", "u64", "i64", "f32", "f64", 
						  "char", "define", "enum", "register", "constant",
						  "void", "typesize", "let", "declare", "when", "case", "default", "switch",
						  "break", "continue", "struct", "as", "alias", "sizeof", "defer", "mut", "dependencies", "asm",
						  "with", "lib", "idle", "pub", "union", "bool", "external", "true", "false", "inline"};

/* ============================================= GLOBAL VARIABLES  ============================================ */

//=============================== Private Utility Macros ================================
/**
 * The minimum token amount is 512. This is considered
 * a sane starting amount
 */
#define DEFAULT_TOKEN_COUNT 512

/**
 * Grab the next char in the stream
 */
#define GET_NEXT_CHAR(fl) fgetc(fl)

/**
 * Put back the char and update the token char num appropriately
 */
#define PUT_BACK_CHAR(fl) fseek(fl, -1, SEEK_CUR)
//=============================== Private Utility Macros ================================


/**
 * Print a lexer message in a nicely formatted way
 */
static inline void print_lexer_error(char* info, u_int32_t line_number){
	//Print this out on a single line
	fprintf(stdout, "\n[FILE: %s] --> [LINE %d | TOKENIZER ERROR]: %s\n", file_name, line_number, info);
}


/**
 * A utility function for error printing that converts any given token
 * into a string
 */
char* lexitem_to_string(lexitem_t* lexitem){
	switch(lexitem->tok){
		case BLANK:
			return "blank";
		case START:
			return "start";
		case ASM_STATEMENT:
			return "Assembly Statement";
		case AT:
			return "@";
		case COLONEQ:
			return ":=";
		case DOT:
			return ".";
		case POUND:
			return "#";
		case L_PAREN:
			return "(";
		case R_PAREN:
			return ")";
		case L_BRACKET:
			return "[";
		case R_BRACKET:
			return "]";
		case L_CURLY:
			return "{";
		case R_CURLY:
			return "}";
		case QUESTION:
			return "?";
		case COMMA:
			return ",";
		case SEMICOLON:
			return ";";
		case DOLLAR:
			return "$";
		case ARROW:
			return "->";
		case FAT_ARROW:
			return "=>";
		case ERROR:
			return "ERROR";
		case DONE:
			return "DONE";
		case IDENT:
		case FUNC_CONST:
		case STR_CONST:
			return lexitem->lexeme.string;
		case INT_CONST:
			sprintf(info, "%d", lexitem->constant_values.signed_int_value);
			return info;
		case INT_CONST_FORCE_U:
			sprintf(info, "%ud", lexitem->constant_values.unsigned_int_value);
			return info;
		case LONG_CONST_FORCE_U:
			sprintf(info, "%ld", lexitem->constant_values.unsigned_long_value);
			return info;
		case SHORT_CONST_FORCE_U:
			sprintf(info, "%u", lexitem->constant_values.unsigned_short_value);
			return info;
		case SHORT_CONST:
			sprintf(info, "%d", lexitem->constant_values.signed_short_value);
			return info;
		case BYTE_CONST:
			sprintf(info, "%d", lexitem->constant_values.signed_byte_value);
			return info;
		case BYTE_CONST_FORCE_U:
			sprintf(info, "%u", lexitem->constant_values.unsigned_byte_value);
			return info;
		case LONG_CONST:
			sprintf(info, "%ld", lexitem->constant_values.unsigned_long_value);
			return info;
		case DOUBLE_CONST:
			sprintf(info, "%lf", lexitem->constant_values.double_value);
			return info;
		case FLOAT_CONST:
			sprintf(info, "%f", lexitem->constant_values.float_value);
			return info;
		case CHAR_CONST:
			sprintf(info, "%c", lexitem->constant_values.char_value);
			return info;
		case IF:
			return "if";
		case ELSE:
			return "else";
		case DO:
			return "do";
		case WHILE:
			return "while";
		case FOR:
			return "for";
		case FN:
			return "fn";
		case RETURN:
			return "ret";
		case JUMP:
			return "jump";
		case REQUIRE:
			return "require";
		case REPLACE:
			return "replace";
		case U8:
			return "u8";
		case I8:
			return "i8";
		case U16:
			return "u16";
		case I16:
			return "i16";
		case U32:
			return "u32";
		case I32:
			return "i32";
		case U64:
			return "u64";
		case I64:
			return "i64";
		case F32:
			return "f32";
		case F64:
			return "f64";
		case CHAR:
			return "char";
		case DEFINE:
			return "define";
		case ENUM:
			return "enum";
		case REGISTER:
			return "register";
		case CONSTANT:
			return "constant";
		case VOID:
			return "void";
		case TYPESIZE:
			return "typesize";
		case LET:
			return "let";
		case DECLARE:
			return "declare";
		case WHEN:
			return "when";
		case CASE:
			return "case";
		case DEFAULT:
			return "default";
		case SWITCH:
			return "switch";
		case BREAK:
			return "break";
		case CONTINUE:
			return "continue";
		case STRUCT:
			return "struct";
		case AS:
			return "AS";
		case ALIAS:
			return "alias";
		case SIZEOF:
			return "sizeof";
		case DEFER:
			return "defer";
		case MUT:
			return "mut";
		case DEPENDENCIES:
			return "dependencies";
		case ASM:
			return "asm";
		case WITH:
			return "with";
		case LIB:
			return "lib";
		case IDLE:
			return "idle";
		case PUB:
			return "pub";
		case UNION:
			return "union";
		case BOOL:
			return "bool";
		case EXTERNAL:
			return "external";
		case TRUE_CONST:
			return "true";
		case FALSE_CONST:
			return "const";
		case PLUSPLUS:
			return "++";
		case MINUSMINUS:
			return "--";
		case PLUS:
			return "+";
		case MINUS:
			return "-";
		case STAR:
			return "*";
		case F_SLASH:
			return "/";
		case MOD:
			return "%";
		case EQUALS:
			return "=";
		case PLUSEQ:
			return "+=";
		case MINUSEQ:
			return "-=";
		case STAREQ:
			return "*=";
		case SLASHEQ:
			return "/=";
		case SINGLE_AND:
			return "&";
		case ANDEQ:
			return "&=";
		case SINGLE_OR:
			return "|";
		case OREQ:
			return "|=";
		case MODEQ:
			return "%=";
		case COLON:
			return ":";
		case CARROT:
			return "^";
		case XOREQ:
			return "^=";
		case DOUBLE_OR:
			return "||";
		case DOUBLE_AND:
			return "&&";
		case L_SHIFT:
			return "<<";
		case LSHIFTEQ:
			return "<<=";
		case R_SHIFT:
			return ">>";
		case RSHIFTEQ:
			return ">>=";
		case G_THAN:
			return ">";
		case L_THAN:
			return "<";
		case G_THAN_OR_EQ:
			return ">=";
		case L_THAN_OR_EQ:
			return "<=";
		case DOUBLE_EQUALS:
			return "==";
		case NOT_EQUALS:
			return "!=";
		case B_NOT:
			return "~";
		case L_NOT:
			return "!";
		default:
			return "UNKNOWN";
	}
}


/**
 * Convert specifically an operator token to a string for printing
 */
char* operator_token_to_string(ollie_token_t token){
	switch(token){
		case PLUSPLUS:
			return "++";
		case MINUSMINUS:
			return "--";
		case PLUS:
			return "+";
		case MINUS:
			return "-";
		case STAR:
			return "*";
		case F_SLASH:
			return "/";
		case MOD:
			return "%";
		case EQUALS:
			return "=";
		case PLUSEQ:
			return "+=";
		case MINUSEQ:
			return "-=";
		case STAREQ:
			return "*=";
		case SLASHEQ:
			return "/=";
		case SINGLE_AND:
			return "&";
		case ANDEQ:
			return "&=";
		case SINGLE_OR:
			return "|";
		case OREQ:
			return "|=";
		case MODEQ:
			return "%=";
		case COLON:
			return ":";
		case CARROT:
			return "^";
		case XOREQ:
			return "^=";
		case DOUBLE_OR:
			return "||";
		case DOUBLE_AND:
			return "&&";
		case L_SHIFT:
			return "<<";
		case LSHIFTEQ:
			return "<<=";
		case R_SHIFT:
			return ">>";
		case RSHIFTEQ:
			return ">>=";
		case G_THAN:
			return ">";
		case L_THAN:
			return "<";
		case G_THAN_OR_EQ:
			return ">=";
		case L_THAN_OR_EQ:
			return "<=";
		case DOUBLE_EQUALS:
			return "==";
		case NOT_EQUALS:
			return "!=";
		case B_NOT:
			return "~";
		case L_NOT:
			return "!";
		default:
			return "UNKNOWN OPERATOR";
	}
}


/**
 * Helper that will determine if we have whitespace(ws) 
 */
static inline u_int8_t is_whitespace(char ch, u_int32_t* line_number){
	switch(ch){
		//Unique case - we'll bump our line number counts
		case '\n':
			(*line_number)++;
			return TRUE;

		case ' ':
		case '\t':
			return TRUE;

		//Anything else just fall right out
		default:
			return FALSE;
	}
}


/**
 * Determines if an identifier is a keyword or some user-written identifier
 */
static lexitem_t identifier_or_keyword(dynamic_string_t lexeme, u_int32_t line_number){
	lexitem_t lex_item;

	//Assign our line number;
	lex_item.line_num = line_number;
	//Wipe this out too
	lex_item.constant_values.signed_long_value = 0;

	//Let's see if we have a keyword here
	for(u_int8_t i = 0; i < KEYWORD_COUNT; i++){
		if(strcmp(keyword_array[i], lexeme.string) == 0){
			//For true/false, we can convert them into the kind of constant we want off the bat
			switch(tok_array[i]){
				case TRUE_CONST:
					lex_item.tok = BYTE_CONST_FORCE_U;
					//Set the byte value
					lex_item.constant_values.unsigned_byte_value = 1;

					return lex_item;
				
				case FALSE_CONST:
					lex_item.tok = BYTE_CONST_FORCE_U;
					//Set the byte value
					lex_item.constant_values.unsigned_byte_value = 0;

					return lex_item;

				default:
					//We can get out of here
					lex_item.tok = tok_array[i];
					//Store the lexeme in here
					lex_item.lexeme = lexeme;
					return lex_item;
			}
		}
	}
	
	//Set the type here
	lex_item.tok = IDENT;

	//Fail out if too long
	if(lexeme.current_length >= MAX_IDENT_LENGTH){
		printf("[LINE %d | LEXER ERROR]: Identifiers may be at most %d characters long\n", line_number, MAX_IDENT_LENGTH);
		lex_item.tok = ERROR;
		return lex_item;
	}

	//Store the lexeme in here
	lex_item.lexeme = lexeme;

	//Give back the lexer item
	return lex_item;
}


/**
 * Reset the stream to a given token index
 */
void reset_stream_to_given_index(ollie_token_stream_t* stream, u_int32_t reconsume_start){
	stream->token_pointer = reconsume_start;
}


/**
 * Generic function that grabs the next token out of the token stream
 */
lexitem_t get_next_token(ollie_token_stream_t* stream, u_int32_t* parser_line_number){
	//Grab the current token index
	u_int32_t token_index = stream->token_pointer;

	//Safe to read here
	if(token_index < stream->current_token_index){
		//Push up the pointer for the next call
		(stream->token_pointer)++;

		//Update the parser line number in here as well
		*parser_line_number = stream->token_stream[token_index].line_num;

		//Give back the token at this given index
		return stream->token_stream[token_index];

	//This should never happen in normal operation
	} else {
		fprintf(stdout, "Fatal internal compiler error. Attempt to read past the token stream endpoint\n");
		exit(1);
	}
}


/**
 * Push a token back. In reality, all that this does is
 * derement the token pointer
 */
void push_back_token(ollie_token_stream_t* stream, u_int32_t* parser_line_number){
	(stream->token_pointer)--;

	//Revert the line number to be whatever this current token's line number is
	*parser_line_number = stream->token_stream[stream->token_pointer].line_num;
}


/**
 * Add a token into the stream. This also handles dynamic resizing if
 * it's needed
 */
static inline void add_lexitem_to_stream(ollie_token_stream_t* stream, lexitem_t lexitem){
	//Dynamic resize for the token stream
	if(stream->current_token_index == stream->max_token_index){
		//Double it
		stream->max_token_index *= 2;

		//Reallocate the entire array
		stream->token_stream = realloc(stream->token_stream, sizeof(lexitem_t) * stream->max_token_index);
	}

	//Add it into the stream
	stream->token_stream[stream->current_token_index] = lexitem;

	//Update the index for next time
	(stream->current_token_index)++;
}


/**
 * Generate all of the ollie tokens and store them in the stream. When
 * this function returns, we will either have a good stream with everything
 * needed in it or a stream in an error state
 */
static u_int8_t generate_all_tokens(FILE* fl, ollie_token_stream_t* stream){
	//Start the line number off at 1
	u_int32_t line_number = 1;

	//Local variables for consuming
	char ch;
	char ch2;
	char ch3;

	//Current state always begins in START
	lex_state_t current_state = IN_START;

	//Initialize the lexitem to be nothing at first
	lexitem_t lex_item;
	lex_item.constant_values.signed_long_value = 0;
	lex_item.tok = ERROR;
	lex_item.line_num = 0;
	INITIALIZE_NULL_DYNAMIC_STRING(lex_item.lexeme);

	//We will need this numeric lexeme for any number we encounter.
	//We will be reusing it, so it's declared up here. It is important
	//to note that this dynamic string *will never* leave this function. It
	//will never be passed along as a pointer to anything else
	dynamic_string_t numeric_lexeme = dynamic_string_alloc();

	//For eventual use down the road. We will not allocate here because this
	//is not always needed
	dynamic_string_t lexeme;

	//Have we seen a hex? Assume no by default
	u_int8_t seen_hex = FALSE;

	//We'll run through character by character until we hit EOF
	while((ch = GET_NEXT_CHAR(fl)) != EOF){
		switch(current_state){
			case IN_START:
				//Reset the seen_hex flag since we're now in the start state
				seen_hex = FALSE;

				//Wipe out the stack lexitem again
				lex_item.constant_values.signed_long_value = 0;
				lex_item.tok = ERROR;
				lex_item.line_num = 0;
				INITIALIZE_NULL_DYNAMIC_STRING(lex_item.lexeme);

				//If we see whitespace we just get out
				if(is_whitespace(ch, &line_number) == TRUE){
					continue;
				}

				//Let's see what we have here
				switch(ch){
					//We could be seeing a comment here
					case '/':
						//Grab the next char, if we see a '*' then we're in a comment
						ch2 = GET_NEXT_CHAR(fl);

						//Based on the second char we take action
						switch(ch2){
							case '*':
								current_state = IN_MULTI_COMMENT;
								break;

							case '/':
								current_state = IN_SINGLE_COMMENT;
								break;
								
							case '=':
								//Prepare the token and put it in the stream
								lex_item.tok = SLASHEQ;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;

							default:
								//"Put back" the char
								PUT_BACK_CHAR(fl);

								//Prepare the token and return it
								lex_item.tok = F_SLASH;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;
						}

						break;

					case '+':
						ch2 = GET_NEXT_CHAR(fl);

						switch(ch2){
							case '+':
								lex_item.tok = PLUSPLUS;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;

							case '=':
								lex_item.tok = PLUSEQ;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;

							default:
								PUT_BACK_CHAR(fl);	
								lex_item.tok = PLUS;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;
						}

						break;

					//Pound for label identifiers
					case '#':
						lex_item.tok = POUND;
						lex_item.line_num = line_number;
						add_lexitem_to_stream(stream, lex_item);
						break;

					//Question mark for ternary operations
					case '?':
						lex_item.tok = QUESTION;
						lex_item.line_num = line_number;
						add_lexitem_to_stream(stream, lex_item);
						break;

					case '-':
						ch2 = GET_NEXT_CHAR(fl);

						switch(ch2){
							case '-':
								lex_item.tok = MINUSMINUS;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;

							case '=':
								lex_item.tok = MINUSEQ;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;
								
							case '>':
								lex_item.tok = ARROW;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;

							default:
								PUT_BACK_CHAR(fl);
								lex_item.tok = MINUS;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;
						}

						break;

					case '*':
						ch2 = GET_NEXT_CHAR(fl);

						switch(ch2){
							case '=':
								lex_item.tok = STAREQ;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;

							default:
								PUT_BACK_CHAR(fl);
								lex_item.tok = STAR;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;
						}

						break;

					case '=':
						ch2 = GET_NEXT_CHAR(fl);

						switch(ch2){
							case '=':
								lex_item.tok = DOUBLE_EQUALS;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;

							case '>':
								lex_item.tok = FAT_ARROW;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;

							default:
								PUT_BACK_CHAR(fl);
								lex_item.tok = EQUALS;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;
						}

						break;

					case '&':
						ch2 = GET_NEXT_CHAR(fl);

						switch(ch2){
							case '&':
								lex_item.tok = DOUBLE_AND;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;

							case '=':
								lex_item.tok = ANDEQ;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;

							default:
								PUT_BACK_CHAR(fl);
								lex_item.tok = SINGLE_AND;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;
						}

						break;

					case '|':
						ch2 = GET_NEXT_CHAR(fl);

						switch(ch2){
							case '|':
								lex_item.tok = DOUBLE_OR;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;

							case '=':
								lex_item.tok = OREQ;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;

							default:
								PUT_BACK_CHAR(fl);
								lex_item.tok = SINGLE_OR;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;
						}

						break;

					case ';':
						lex_item.tok = SEMICOLON;
						lex_item.line_num = line_number;
						add_lexitem_to_stream(stream, lex_item);
						break;

					case '%':
						ch2 = GET_NEXT_CHAR(fl);

						switch(ch2) {
							case '=':
								lex_item.tok = MODEQ;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;

							default:
								PUT_BACK_CHAR(fl);
								lex_item.tok = MOD;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;
						}

						break;

					case ':':
						ch2 = GET_NEXT_CHAR(fl);

						switch(ch2) {
							case '=':
								lex_item.tok = COLONEQ;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;

							default:
								PUT_BACK_CHAR(fl);
								lex_item.tok = COLON;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;
						}

						break;

					case '(':
						lex_item.tok = L_PAREN;
						lex_item.line_num = line_number;
						add_lexitem_to_stream(stream, lex_item);
						break;

					case ')':
						lex_item.tok = R_PAREN;
						lex_item.line_num = line_number;
						add_lexitem_to_stream(stream, lex_item);
						break;

					case '^':
						ch2 = GET_NEXT_CHAR(fl);

						switch(ch2) {
							case '=':
								lex_item.tok = XOREQ;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;

							default:
								PUT_BACK_CHAR(fl);
								lex_item.tok = CARROT;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;
						}

						break;

					case '{':
						lex_item.tok = L_CURLY;
						lex_item.line_num = line_number;
						add_lexitem_to_stream(stream, lex_item);
						break;

					case '}':
						lex_item.tok = R_CURLY;
						lex_item.line_num = line_number;
						add_lexitem_to_stream(stream, lex_item);
						break;

					case '[':
						lex_item.tok = L_BRACKET;
						lex_item.line_num = line_number;
						add_lexitem_to_stream(stream, lex_item);
						break;

					case ']':
						lex_item.tok = R_BRACKET;
						lex_item.line_num = line_number;
						add_lexitem_to_stream(stream, lex_item);
						break;

					case '@':
						lex_item.tok = AT;
						lex_item.line_num = line_number;
						add_lexitem_to_stream(stream, lex_item);
						break;

					case '.':
						//Let's see what we have here
						ch2 = GET_NEXT_CHAR(fl);
						
						switch(ch2){
							case '0':
							case '1':
							case '2':
							case '3':
							case '4':
							case '5':
							case '6':
							case '7':
							case '8':
							case '9':
								//VERY IMPORTANT - we need to add this to the *numeric lexeme*. That is what will be processed
								//by the converter
								dynamic_string_add_char_to_back(&numeric_lexeme, ch);
								dynamic_string_add_char_to_back(&numeric_lexeme, ch2);

								//We are not in an int
								current_state = IN_FLOAT;

								break;

							default:
								PUT_BACK_CHAR(fl);
								lex_item.tok = DOT;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;
						}

						break;
					
					case ',':
						lex_item.tok = COMMA;
						lex_item.line_num = line_number;
						add_lexitem_to_stream(stream, lex_item);
						break;

					case '~':
						lex_item.tok = B_NOT;
						lex_item.line_num = line_number;
						add_lexitem_to_stream(stream, lex_item);
						break;

					case '!':
						ch2 = GET_NEXT_CHAR(fl);

						switch(ch2) {
							case '=':
								lex_item.tok = NOT_EQUALS;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;

							default:
								PUT_BACK_CHAR(fl);
								lex_item.tok = L_NOT;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;
						}

						break;

					//Beginning of a string literal
					case '"':
						current_state = IN_STRING;
						lexeme = dynamic_string_alloc();
						break;

					//Beginning of a char const
					case '\'':
						//Grab the next char
						ch2 = GET_NEXT_CHAR(fl);

						//We've seen no escape character, so
						//we can do normal processing
						if(ch2 != '\\'){
							//We need to hit the closing single quote
							ch3 = GET_NEXT_CHAR(fl);

							//Remember - we need to see the closing quote here
							if(ch3 != '\''){
								print_lexer_error("Char must be a single character or escape sequence character, followed by '''", line_number);
								return FAILURE;
							}

							//Store the char value
							lex_item.constant_values.char_value = ch2;

						//If this is the escape character, then
						//we need to consume the next token
						} else {
							//Get the next token
							ch2 = GET_NEXT_CHAR(fl);

							//We can't just see the escape backslash
							if(ch2 == '\''){
								print_lexer_error("Escape sequence requires a character after \\.", line_number);
								return FAILURE;
							}

							//We need to hit the closing single quote
							ch3 = GET_NEXT_CHAR(fl);

							//Remember - we need to see the closing quote here
							if(ch3 != '\''){
								print_lexer_error("Char must be a single character or escape sequence character, followed by '''", line_number);
								return FAILURE;
							}

							//There are only a few kinds of escape characters
							//allowed. If a user attempt an invalid escape character,
							//that is a hard failure
							switch(ch2) {
								case '0':
									//This is the actual value 0
									lex_item.constant_values.char_value = 0;
									break;

								//Double escape char
								case '\\':
									lex_item.constant_values.char_value = 92;
									break;

								//BEL char
								case 'a':
									lex_item.constant_values.char_value = 7;
									break;

								//Backspace
								case 'b':
									lex_item.constant_values.char_value = 8;
									break;
								
								//Horizontal tab
								case 't':
									lex_item.constant_values.char_value = 9;
									break;

								//Newline
								case 'n':
									lex_item.constant_values.char_value = 10;
									break;

								//Vertical tab
								case 'v':
									lex_item.constant_values.char_value = 11;
									break;

								//Form feed
								case 'f':
									lex_item.constant_values.char_value = 12;
									break;
									
								//Carriage return
								case 'r':
									lex_item.constant_values.char_value = 13;
									break;

								//Trying to escape into a char
								case '\'':
									lex_item.constant_values.char_value = '\'';
									break;
									
								//Trying to escape into a quote
								case '\"':
									lex_item.constant_values.char_value = '\"';
									break;

								//Hard fail in this case
								default:
									print_lexer_error("Invalid escape sequence character found. Please consult the ASCII manual(man ascii) for the list of escape characters", line_number);
									lex_item.tok = ERROR;
									lex_item.line_num = line_number;
									return FAILURE;
							}
						}

						//If we get to down here then it all worked out, so we'll
						//add it into the stream
						lex_item.tok = CHAR_CONST;
						lex_item.line_num = line_number;
						add_lexitem_to_stream(stream, lex_item);

						break;

					case '<':
						ch2 = GET_NEXT_CHAR(fl);

						switch(ch2){
							case '<':
								ch3 = GET_NEXT_CHAR(fl);

								switch(ch3){
									case '=':
										lex_item.tok = LSHIFTEQ;
										lex_item.line_num = line_number;
										add_lexitem_to_stream(stream, lex_item);
										break;
									
									default:
										PUT_BACK_CHAR(fl);
										lex_item.tok = L_SHIFT;
										lex_item.line_num = line_number;
										add_lexitem_to_stream(stream, lex_item);
										break;
								}

								break;

							case '=':
								lex_item.tok = L_THAN_OR_EQ;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;

							default:
								PUT_BACK_CHAR(fl);
								lex_item.tok = L_THAN;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;
						}
						
						break;

					case '>':
						ch2 = GET_NEXT_CHAR(fl);

						switch(ch2){
							case '>':
								ch3 = GET_NEXT_CHAR(fl);

								switch(ch3){
									case '=':
										lex_item.tok = RSHIFTEQ;
										lex_item.line_num = line_number;
										add_lexitem_to_stream(stream, lex_item);
										break;

									default:
										PUT_BACK_CHAR(fl);
										lex_item.tok = R_SHIFT;
										lex_item.line_num = line_number;
										add_lexitem_to_stream(stream, lex_item);
										break;
								}

								break;

							case '=':
								lex_item.tok = G_THAN_OR_EQ;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;

							default:
								PUT_BACK_CHAR(fl);
								lex_item.tok = G_THAN;
								lex_item.line_num = line_number;
								add_lexitem_to_stream(stream, lex_item);
								break;
						}

						break;

					default:
						if((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '$' || ch == '#' || ch == '_'){
							lexeme = dynamic_string_alloc();
							dynamic_string_add_char_to_back(&lexeme, ch);
							current_state = IN_IDENT;

						//If we get here we have the start of either an int or a real
						} else if(ch >= '0' && ch <= '9'){
							//VERY IMPORTANT - we need to add this to the *numeric lexeme*. That is what will be processed
							//by the converter
							dynamic_string_add_char_to_back(&numeric_lexeme, ch);
							current_state = IN_INT;

						} else {
							print_lexer_error("Invalid character found", line_number);
							return FAILURE;
						}

						break;
				}

				break;

			case IN_IDENT:
				//Is it a number, letter, or _ or $?. If so, we can have it in our ident
				if(ch == '_' || ch == '$' || (ch >= 'a' && ch <= 'z') 
				   || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')){
					dynamic_string_add_char_to_back(&lexeme, ch);

				} else {
					PUT_BACK_CHAR(fl);
					//Invoke the helper, then add it in
					lex_item = identifier_or_keyword(lexeme, line_number);
					add_lexitem_to_stream(stream, lex_item);

					//IMPORTANT - reset the state here
					current_state = IN_START;
				}

				break;

			//For any/all INT constants, we will be using the numeric
			//lexeme to hold values temporarily until we're done
			case IN_INT:
				//Add it in and move along
				if(ch >= '0' && ch <= '9'){
					dynamic_string_add_char_to_back(&numeric_lexeme, ch);

				//If we see hex and we're in hex, it's also fine
				} else if(((ch >= 'a' && ch <= 'f') && seen_hex == TRUE) 
						|| ((ch >= 'A' && ch <= 'F') && seen_hex == TRUE)){
					dynamic_string_add_char_to_back(&numeric_lexeme, ch);

				} else {
					switch(ch){
						case 'x':
						case 'X':
							//If we've already seen the hex code this is bad
							if(seen_hex == TRUE){
								print_lexer_error("Hexadecimal numbers cannot have 'x' or 'X' in them twice", line_number);
								return FAILURE;
							}

							//If we haven't seen the 0 here it's bad
							if(*(numeric_lexeme.string) != '0'){
								print_lexer_error("Hexadecimal 'x' or 'X' must be preceeded by a '0'", line_number);
								return FAILURE;
							}

							//Otherwise set this and add it in
							seen_hex = TRUE;

							//Add the character dynamically
							dynamic_string_add_char_to_back(&numeric_lexeme, ch);

							break;

						case '.':
							//If we've already seen the hex code this is bad
							if(seen_hex == TRUE){
								print_lexer_error("The '.' character is not valid in hexadecimal constants", line_number);
								return FAILURE;
							}

							//We're actually in a float const
							current_state = IN_FLOAT;
							//Add the character dynamically
							dynamic_string_add_char_to_back(&numeric_lexeme, ch);

							break;

						//The 'l' or 'L' tells us that we're forcing to long
						case 'l':
						case 'L':
							lex_item.line_num = line_number;
							lex_item.lexeme = lexeme;
							lex_item.tok = LONG_CONST;

							//Convert accordingly
							if(seen_hex == FALSE){
								lex_item.constant_values.signed_long_value = atol(numeric_lexeme.string);
							} else {
								lex_item.constant_values.signed_long_value = strtol(numeric_lexeme.string, NULL, 0);
							}

							//Add this into the stream
							add_lexitem_to_stream(stream, lex_item);

							//IMPORTANT - reset the state here
							current_state = IN_START;

							//We need to also reset the numeric lexeme
							clear_dynamic_string(&numeric_lexeme);

							break;

						//Forcing to short
						case 's':
						case 'S':
							lex_item.line_num = line_number;
							lex_item.lexeme = lexeme;
							lex_item.tok = SHORT_CONST;

							//Convert accordingly
							if(seen_hex == FALSE){
								lex_item.constant_values.signed_short_value = atol(numeric_lexeme.string);
							} else {
								lex_item.constant_values.signed_short_value = strtol(numeric_lexeme.string, NULL, 0);
							}

							//Add it to the stream
							add_lexitem_to_stream(stream, lex_item);

							//IMPORTANT - reset the state here
							current_state = IN_START;

							//We need to also reset the numeric lexeme
							clear_dynamic_string(&numeric_lexeme);

							break;

						//Forcing to Byte
						case 'y':
						case 'Y':
							lex_item.line_num = line_number;
							lex_item.lexeme = lexeme;
							lex_item.tok = BYTE_CONST;

							//Convert accordingly
							if(seen_hex == FALSE){
								lex_item.constant_values.signed_byte_value = atol(numeric_lexeme.string);
							} else {
								lex_item.constant_values.signed_byte_value = strtol(numeric_lexeme.string, NULL, 0);
							}

							//Add it to the stream
							add_lexitem_to_stream(stream, lex_item);

							//IMPORTANT - reset the state here
							current_state = IN_START;

							//We need to also reset the numeric lexeme
							clear_dynamic_string(&numeric_lexeme);

							break;

						//If we see this it means we're forcing to unsigned
						case 'u':
						case 'U':
							//We are forcing this to be unsigned
							//We can still see "l", so let's check
							ch2 = GET_NEXT_CHAR(fl);

							//We can still see more qualifiers
							switch(ch2){
								case 'l':
								case 'L':
									lex_item.tok = LONG_CONST_FORCE_U;

									//Convert accordingly
									if(seen_hex == FALSE){
										lex_item.constant_values.unsigned_long_value = atol(numeric_lexeme.string);
									} else {
										lex_item.constant_values.unsigned_long_value = strtol(numeric_lexeme.string, NULL, 0);
									}

									break;

								case 's':
								case 'S':
									lex_item.tok = SHORT_CONST_FORCE_U;

									//Convert accordingly
									if(seen_hex == FALSE){
										lex_item.constant_values.unsigned_short_value = atol(numeric_lexeme.string);
									} else {
										lex_item.constant_values.unsigned_short_value = strtol(numeric_lexeme.string, NULL, 0);
									}

									break;

								case 'y':
								case 'Y':
									lex_item.tok = BYTE_CONST_FORCE_U;

									//Convert accordingly
									if(seen_hex == FALSE){
										lex_item.constant_values.unsigned_byte_value = atol(numeric_lexeme.string);
									} else {
										lex_item.constant_values.unsigned_byte_value = strtol(numeric_lexeme.string, NULL, 0);
									}

									break;

								default:
									//Put it back
									PUT_BACK_CHAR(fl);
									lex_item.tok = INT_CONST_FORCE_U;

									//Convert accordingly
									if(seen_hex == FALSE){
										lex_item.constant_values.unsigned_int_value = atol(numeric_lexeme.string);
									} else {
										lex_item.constant_values.unsigned_int_value = strtol(numeric_lexeme.string, NULL, 0);
									}

									break;
							}

							//Add the line number and get it into the stream
							lex_item.line_num = line_number;
							add_lexitem_to_stream(stream, lex_item);

							//IMPORTANT - reset the state here
							current_state = IN_START;

							//We need to also reset the numeric lexeme
							clear_dynamic_string(&numeric_lexeme);

							break;

						default:
							//Otherwise we're out
							//"Put back" the char
							PUT_BACK_CHAR(fl);

							//This is an int const
							lex_item.tok = INT_CONST;

							//Convert accordingly
							if(seen_hex == FALSE){
								lex_item.constant_values.signed_int_value = atol(numeric_lexeme.string);
							} else {
								lex_item.constant_values.signed_int_value = strtol(numeric_lexeme.string, NULL, 0);
							}

							//Pack it up and add it in
							lex_item.line_num = line_number;
							add_lexitem_to_stream(stream, lex_item);

							//IMPORTANT - reset the state here
							current_state = IN_START;

							//We need to also reset the numeric lexeme
							clear_dynamic_string(&numeric_lexeme);

							break;
					}
				}

				break;

			case IN_FLOAT:
				//We're just in a regular float here
				if(ch >= '0' && ch <= '9'){
					//Add the character in
					dynamic_string_add_char_to_back(&numeric_lexeme, ch);

				//We can now see a D or d here that tells
				//us to force this to double precision
				} else if(ch == 'd' || ch == 'D'){
					//Give this back as a double CONST
					lex_item.tok = DOUBLE_CONST;
					lex_item.line_num = line_number;

					//Convert here
					lex_item.constant_values.double_value = atof(numeric_lexeme.string);

					//Get it into the stream
					add_lexitem_to_stream(stream, lex_item);

					//IMPORTANT - reset the state here
					current_state = IN_START;

					//We need to also reset the numeric lexeme
					clear_dynamic_string(&numeric_lexeme);

				} else {
					//Put back the char
					PUT_BACK_CHAR(fl);
					
					//We'll give this back now
					lex_item.tok = FLOAT_CONST;
					lex_item.line_num = line_number;

					//Convert here
					lex_item.constant_values.float_value = atof(numeric_lexeme.string);

					//Get it into the stream
					add_lexitem_to_stream(stream, lex_item);

					//IMPORTANT - reset the state here
					current_state = IN_START;

					//We need to also reset the numeric lexeme
					clear_dynamic_string(&numeric_lexeme);
				}

				break;

			case IN_STRING:
				//If we see the end of the string
				if(ch == '"'){ 
					lex_item.tok = STR_CONST;
					lex_item.lexeme = lexeme;
					lex_item.line_num = line_number;
					add_lexitem_to_stream(stream, lex_item);

					//IMPORTANT - reset the state here
					current_state = IN_START;

				//Escape char
				} else if (ch == '\\'){
					//Consume the next character, whatever it is
					is_whitespace(fgetc(fl), &line_number);

				} else {
					//Otherwise we'll just keep adding here
					//Just for line counting
					is_whitespace(ch, &line_number);
					dynamic_string_add_char_to_back(&lexeme, ch);
				}

				break;

			//If we're in a comment, we can escape if we see "*/"
			case IN_MULTI_COMMENT:
				//Are we at the start of an escape sequence?
				if(ch == '*'){
					ch2 = GET_NEXT_CHAR(fl);
					if(ch2 == '/'){
						//We are now out of the comment
						current_state = IN_START;
						//Reset the char count
						break;
					} else {
						PUT_BACK_CHAR(fl);
						break;
					}
				}
				//Otherwise just check for whitespace
				is_whitespace(ch, &line_number);
				break;

			//If we're in a single line comment
			case IN_SINGLE_COMMENT:
				//Are we at the start of the escape sequence
				//Newline means we get out
				if(ch == '\n'){
					line_number++;
					current_state = IN_START;
				} 

				//Otherwise just go forward
				break;

			//Some very weird error here
			default:
				print_lexer_error("Found a stateless token", line_number);
				return FAILURE;
		}
	}

	//Once we get down here, it is safe for us to free the numeric lexeme because we do not
	//need it anymore
	dynamic_string_dealloc(&numeric_lexeme);

	//Return this token
	if(ch == EOF){
		lex_item.tok = DONE;
		lex_item.line_num = line_number;
		add_lexitem_to_stream(stream, lex_item);
		return SUCCESS;
	}

	return SUCCESS;
}


/**
 * Initialize the lexer by dynamically allocating the lexstack
 * and any other needed data structures
 *
 * The tokenizer also handles all file input
 */
ollie_token_stream_t tokenize(char* current_file_name){
	//Store the file name for any error printing
	file_name = current_file_name;

	//Stack allocate
	ollie_token_stream_t token_stream;
	//Initialize the internal storage for our token
	token_stream.token_stream = calloc(DEFAULT_TOKEN_COUNT, sizeof(lexitem_t));
	//Initialize to our default
	token_stream.max_token_index = DEFAULT_TOKEN_COUNT;
	//Initialize to 0
	token_stream.current_token_index = 0;
	//From the parsing perspective
	token_stream.token_pointer = 0;

	//Attempt to open the file
	FILE* fl = fopen(current_file_name, "r");

	//If we can't open, it's an autofailure
	if(fl == NULL){
		sprintf(info, "Failed to open file %s", file_name);
		print_lexer_error(info, 0);

		//Print the failure out and leave
		token_stream.status = STREAM_STATUS_FAILURE;
		return token_stream;
	}

	//Consume all of the tokens here using the helper
	u_int8_t result = generate_all_tokens(fl, &token_stream);

	//Once we're done, we close the file
	fclose(fl);

	//Update the status accordingly
	token_stream.status = result == SUCCESS ? STREAM_STATUS_SUCCESS : STREAM_STATUS_FAILURE;

	//Give it back
	return token_stream;
}


/**
 * Deallocate the entire token stream. In reality this
 * just means freeing the array
 */
void destroy_token_stream(ollie_token_stream_t* stream){
	free(stream->token_stream);
}
