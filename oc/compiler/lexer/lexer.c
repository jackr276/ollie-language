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
#include "../utils/stack/lexstack.h"
#include "../utils/constants.h"

//Total number of keywords
#define KEYWORD_COUNT 52

//We will use this to keep track of what the current lexer state is
typedef enum {
	IN_START,
	IN_IDENT,
	IN_INT,
	IN_FLOAT,
	IN_STRING,
	IN_MULTI_COMMENT,
	IN_SINGLE_COMMENT
} lex_state;


/* ============================================= GLOBAL VARIABLES  ============================================ */
//Current line num
u_int32_t line_num = 0;

//Token array, we will index using their enum values
static const ollie_token_t tok_array[] = {IF, ELSE, DO, WHILE, FOR, FN, RET, JUMP, REQUIRE, REPLACE, 
					U8, I8, U16, I16, U32, I32, U64, I64, F32, F64, CHAR, DEFINE, ENUM,
					REGISTER, CONSTANT, VOID, TYPESIZE, LET, DECLARE, WHEN, CASE, DEFAULT, SWITCH, BREAK, CONTINUE, 
					STRUCT, AS, ALIAS, SIZEOF, DEFER, MUT, DEPENDENCIES, ASM, WITH, LIB, IDLE, PUB, UNION, BOOL,
				    EXTERNAL, TRUE_CONST, FALSE_CONST};

//Direct one to one mapping
static const char* keyword_array[] = {"if", "else", "do", "while", "for", "fn", "ret", "jump",
						 "require", "replace", "u8", "i8", "u16",
						 "i16", "u32", "i32", "u64", "i64", "f32", "f64", 
						  "char", "define", "enum", "register", "constant",
						  "void", "typesize", "let", "declare", "when", "case", "default", "switch",
						  "break", "continue", "struct", "as", "alias", "sizeof", "defer", "mut", "dependencies", "asm",
						  "with", "lib", "idle", "pub", "union", "bool", "external", "true", "false"};

/* ============================================= GLOBAL VARIABLES  ============================================ */

//=============================== Private Utility Macros ================================
/**
 * The minimum token amount is 256
 */
#define DEFAULT_TOKEN_COUNT 256

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
		case HEX_CONST:
		case INT_CONST:
		case INT_CONST_FORCE_U:
		case LONG_CONST_FORCE_U:
		case SHORT_CONST_FORCE_U:
		case SHORT_CONST:
		case BYTE_CONST:
		case BYTE_CONST_FORCE_U:
		case LONG_CONST:
		case DOUBLE_CONST:
		case FLOAT_CONST:
		case STR_CONST:
		case CHAR_CONST:
			return lexitem->lexeme.string;
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
		case RET:
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
	}
}


/**
 * Helper that will determine if we have whitespace(ws) 
 */
static inline u_int8_t is_whitespace(char ch, u_int32_t* line_num){
	switch(ch){
		//Unique case - we'll bump our line number counts
		case '\n':
			(*line_num)++;
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

	//Let's see if we have a keyword here
	for(u_int8_t i = 0; i < KEYWORD_COUNT; i++){
		if(strcmp(keyword_array[i], lexeme.string) == 0){
			//For true/false, we can convert them into the kind of constant we want off the bat
			switch(tok_array[i]){
				case TRUE_CONST:
					lex_item.tok = BYTE_CONST_FORCE_U;
					lex_item.lexeme = dynamic_string_alloc();
					dynamic_string_set(&(lex_item.lexeme), "1");

					return lex_item;
				
				case FALSE_CONST:
					lex_item.tok = BYTE_CONST_FORCE_U;
					lex_item.lexeme = dynamic_string_alloc();
					dynamic_string_set(&(lex_item.lexeme), "0");

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
 * Reconsume the tokens starting from a given seek
 */
void reconsume_tokens(FILE* fl, int64_t reconsume_start){
	//Seek back to where the user wanted to reconsume from
	fseek(fl, reconsume_start, SEEK_SET);
}


/**
 * A special case here where we get the next assembly inline statement. Assembly
 * inline statements are officially terminated by a backslash "\", so we 
 * will simply run through what we have here until we get to that backslash. We'll
 * then pack what we had into a lexer item and send it back to the caller
 */
lexitem_t get_next_assembly_statement(FILE* fl){
	//We'll be giving this back
	lexitem_t asm_statement;
	asm_statement.tok = ASM_STATEMENT;

	//The dynamic string for our assembly statement
	dynamic_string_t asm_string = dynamic_string_alloc();

	//Searching char
	char ch;

	//First pop off all of the tokens if there are any on the stack
	while(lex_stack_is_empty(&pushed_back_tokens) == FALSE){
		//Pop whatever we have off
		lexitem_t token = pop_token(&pushed_back_tokens);

		//Concatenate the string here
		dynamic_string_concatenate(&asm_string, token.lexeme.string);
	}

	//So long as we don't see a backslash, we keep going
	ch = GET_NEXT_CHAR(fl);

	//So long as we don't see this move along, adding ch into 
	//our lexeme
	while(ch != ';'){
		//In this case we'll add the char to the back
		dynamic_string_add_char_to_back(&asm_string, ch);

		//Refresh the char
		ch = GET_NEXT_CHAR(fl);
	}
	
	//Store the asm string as the lexeme
	asm_statement.lexeme = asm_string;

	//Otherwise we're done
	return asm_statement;
}


/**
 * Constantly iterate through the file and grab the next token that we have
*/
lexitem_t get_next_token(FILE* fl, u_int32_t* parser_line_num){
	//
	//
	//
	//
	//TODO
	//
	//
	//
}


/**
 * Push a token back by moving the seek head back appropriately
 */
void push_back_token(lexitem_t l){
}


/**
 * Add a token into the stream. This also handles dynamic resizing if
 * it's needed
 */
static inline void add_lexitem_to_stream(ollie_token_stream_t* stream, lexitem_t token){
	//Dynamic resize for the token stream
	if(stream->current_token_index == stream->max_token_index){
		//Double it
		stream->max_token_index *= 2;

		//Reallocate the entire array
		stream->token_stream = realloc(stream->token_stream, sizeof(lexitem_t) * stream->max_token_index);
	}

	//Add it into the stream
	stream->token_stream[stream->current_token_index] = token;

	//Update the index for next time
	(stream->current_token_index)++;
}


/**
 * Generate all of the ollie tokens and store them in the stream. When
 * this function returns, we will either have a good stream with everything
 * needed in it or a stream in an error state
 */
static void generate_all_tokens(FILE* fl, ollie_token_stream_t* stream){
	//Local variables for consuming
	char ch;
	char ch2;
	char ch3;

	//We'll run through character by character until we hit EOF
	while((ch = GET_NEXT_CHAR(fl)) != EOF){
		//Initialize here. We have the ERROR token as our sane default
		lexitem_t lex_item = {{NULL, 0, 0}, 0, ERROR};

		//Have we seen a hex?
		u_int8_t seen_hex;

		//Current state always begins in START
		lex_state current_state = IN_START;

		//Switch on the current state
		switch(current_state){
			case IN_START:
				//If we see whitespace we just get out
				if(is_whitespace(ch, &line_num) == TRUE){
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
								lex_item.line_num = line_num;
								add_lexitem_to_stream(stream, lex_item);
								break;

							default:
								//"Put back" the char
								PUT_BACK_CHAR(fl);

								//Prepare the token and return it
								lex_item.tok = F_SLASH;
								lex_item.line_num = line_num;
								add_lexitem_to_stream(stream, lex_item);
								break;
						}

					case '+':
						ch2 = GET_NEXT_CHAR(fl);

						switch(ch2){
							case '+':
								lex_item.tok = PLUSPLUS;
								lex_item.line_num = line_num;
								add_lexitem_to_stream(stream, lex_item);
								break;

							case '=':
								lex_item.tok = PLUSEQ;
								lex_item.line_num = line_num;
								add_lexitem_to_stream(stream, lex_item);
								break;

							default:
								PUT_BACK_CHAR(fl);	
								lex_item.tok = PLUS;
								lex_item.line_num = line_num;
								add_lexitem_to_stream(stream, lex_item);
								break;
						}

					//Pound for label identifiers
					case '#':
						lex_item.tok = POUND;
						lex_item.line_num = line_num;
						add_lexitem_to_stream(stream, lex_item);
						break;

					//Question mark for ternary operations
					case '?':
						lex_item.tok = QUESTION;
						lex_item.line_num = line_num;
						add_lexitem_to_stream(stream, lex_item);
						break;

					case '-':
						ch2 = GET_NEXT_CHAR(fl);

						switch(ch2){
							case '-':
								lex_item.tok = MINUSMINUS;
								lex_item.line_num = line_num;
								add_lexitem_to_stream(stream, lex_item);
								break;

							case '=':
								lex_item.tok = MINUSEQ;
								lex_item.line_num = line_num;
								add_lexitem_to_stream(stream, lex_item);
								break;
								
							case '>':
								lex_item.tok = ARROW;
								lex_item.line_num = line_num;
								add_lexitem_to_stream(stream, lex_item);
								break;

							default:
								PUT_BACK_CHAR(fl);
								lex_item.tok = MINUS;
								lex_item.line_num = line_num;
								add_lexitem_to_stream(stream, lex_item);
								break;
						}

					case '*':
						ch2 = GET_NEXT_CHAR(fl);

						switch(ch2){
							case '=':
								lex_item.tok = STAREQ;
								lex_item.line_num = line_num;
								add_lexitem_to_stream(stream, lex_item);
								break;

							default:
								PUT_BACK_CHAR(fl);
								lex_item.tok = STAR;
								lex_item.line_num = line_num;
								add_lexitem_to_stream(stream, lex_item);
								break;
						}

					case '=':
						ch2 = GET_NEXT_CHAR(fl);

						switch(ch2){
							case '=':
								lex_item.tok = DOUBLE_EQUALS;
								lex_item.line_num = line_num;
								add_lexitem_to_stream(stream, lex_item);
								break;

							case '>':
								lex_item.tok = FAT_ARROW;
								lex_item.line_num = line_num;
								add_lexitem_to_stream(stream, lex_item);
								break;

							default:
								PUT_BACK_CHAR(fl);
								lex_item.tok = EQUALS;
								lex_item.line_num = line_num;
								add_lexitem_to_stream(stream, lex_item);
								break;
						}

					case '&':
						ch2 = GET_NEXT_CHAR(fl);

						if(ch2 == '&'){
							//Prepare and return
							lex_item.tok = DOUBLE_AND;
							lex_item.line_num = line_num;
							return lex_item;

						//We could see &=
						} else if (ch2 == '=') {
							//Prepare and return
							lex_item.tok = ANDEQ;
							lex_item.line_num = line_num;
							return lex_item;

						} else {
							PUT_BACK_CHAR(fl);
							lex_item.tok = SINGLE_AND;
							lex_item.line_num = line_num;
							return lex_item;
						}

					case '|':
						ch2 = GET_NEXT_CHAR(fl);
						//If we get this then it's +=
						if(ch2 == '|'){
							//Prepare and return
							lex_item.tok = DOUBLE_OR;
							lex_item.line_num = line_num;
							return lex_item;
						//We could also see |=
						} else if(ch2 == '='){
							//Prepare and return
							lex_item.tok = OREQ;
							lex_item.line_num = line_num;
							return lex_item;
						} else {
							//"Put back" the char
							PUT_BACK_CHAR(fl);
							lex_item.tok = SINGLE_OR;
							lex_item.line_num = line_num;
							return lex_item;
						}

					case ';':
						lex_item.tok = SEMICOLON;
						lex_item.line_num = line_num;
						return lex_item;

					case '%':
						ch2 = GET_NEXT_CHAR(fl);

						//We could see %=
						if(ch2 == '='){
							lex_item.tok = MODEQ;
							lex_item.line_num = line_num;
							return lex_item;
						} else {
							lex_item.tok = MOD;
							lex_item.line_num = line_num;
							return lex_item;
						}

					case ':':
						ch2 = GET_NEXT_CHAR(fl);

						//We have a ":="
						if(ch2 == '='){
							//Prepare and return
							lex_item.tok = COLONEQ;
							lex_item.line_num = line_num;
							return lex_item;
						} else {
							//Put it back
							PUT_BACK_CHAR(fl);
							lex_item.tok = COLON;
							lex_item.line_num = line_num;
							return lex_item;
						}

					case '(':
						lex_item.tok = L_PAREN;
						lex_item.line_num = line_num;
						return lex_item;

					case ')':
						lex_item.tok = R_PAREN;
						lex_item.line_num = line_num;
						return lex_item;

					case '^':
						ch2 = GET_NEXT_CHAR(fl);

						if(ch2 == '='){
							lex_item.tok = XOREQ;
							lex_item.line_num = line_num;
							return lex_item;

						} else {
							PUT_BACK_CHAR(fl);
							lex_item.tok = CARROT;
							lex_item.line_num = line_num;
							return lex_item;
						}

					case '{':
						lex_item.tok = L_CURLY;
						lex_item.line_num = line_num;
						return lex_item;

					case '}':
						lex_item.tok = R_CURLY;
						lex_item.line_num = line_num;
						return lex_item;

					case '[':
						lex_item.tok = L_BRACKET;
						lex_item.line_num = line_num;
						return lex_item;

					case ']':
						lex_item.tok = R_BRACKET;
						lex_item.line_num = line_num;
						return lex_item;

					case '@':
						lex_item.tok = AT;
						lex_item.line_num = line_num;
						return lex_item;

					case '.':
						//Let's see what we have here
						ch2 = GET_NEXT_CHAR(fl);
						if(ch2 >= '0' && ch2 <= '9'){
							//Allocate the string
							lexeme = dynamic_string_alloc();

							//Add both of these in
							dynamic_string_add_char_to_back(&lexeme, ch);
							dynamic_string_add_char_to_back(&lexeme, ch2);

							//We are not in an int
							current_state = IN_FLOAT;

						} else {
							//Put back ch2
							PUT_BACK_CHAR(fl);
							lex_item.tok = DOT;
							lex_item.line_num = line_num;
							return lex_item;
						}

						break;
					
					case ',':
						lex_item.tok = COMMA;
						lex_item.line_num = line_num;
						return lex_item;

					case '~':
						lex_item.tok = B_NOT;
						lex_item.line_num = line_num;
						return lex_item;

					case '!':
						ch2 = GET_NEXT_CHAR(fl);
						if(ch2 == '='){
							//Prepare and return
							lex_item.tok = NOT_EQUALS;
							lex_item.line_num = line_num;
							return lex_item;
						} else {
							//Put it back
							PUT_BACK_CHAR(fl);
							lex_item.tok = L_NOT;
							lex_item.line_num = line_num;
							return lex_item;
						}

					//Beginning of a string literal
					case '"':
						//Say that we're in a string
						current_state = IN_STRING;
						//Allocate the lexeme
						lexeme = dynamic_string_alloc();
						break;

					//Beginning of a char const
					case '\'':
						//Grab the next char
						ch2 = GET_NEXT_CHAR(fl);

						//Allocate the lexeme here
						lexeme = dynamic_string_alloc();

						//Add our char const ch2 in
						dynamic_string_add_char_to_back(&lexeme, ch2);

						//Now we must see another single quote
						ch2 = GET_NEXT_CHAR(fl);

						//If this is the case, then we've messed up
						if(ch2 != '\''){
							lex_item.tok = ERROR;
							lex_item.line_num = line_num;
							return lex_item;
						}

						//Package and return
						lex_item.tok = CHAR_CONST;
						//Add the dynamic string in
						lex_item.lexeme = lexeme;
						lex_item.line_num = line_num;
						return lex_item;

					case '<':
						//Grab the next char
						ch2 = GET_NEXT_CHAR(fl);
						if(ch2 == '<'){
							ch3 = GET_NEXT_CHAR(fl);

							if(ch3 == '='){
								lex_item.tok = LSHIFTEQ;
								lex_item.line_num = line_num;
								return lex_item;

							} else {
								PUT_BACK_CHAR(fl);
								lex_item.tok = L_SHIFT;
								lex_item.line_num = line_num;
								return lex_item;
							}

						} else if(ch2 == '=') {
							lex_item.tok = L_THAN_OR_EQ;
							lex_item.line_num = line_num;
							return lex_item;
						} else {
							PUT_BACK_CHAR(fl);
							lex_item.tok = L_THAN;
							lex_item.line_num = line_num;
							return lex_item;
						}
						break;

					case '>':
						//Grab the next char
						ch2 = GET_NEXT_CHAR(fl);
						if(ch2 == '>'){
							ch3 = GET_NEXT_CHAR(fl);
							if(ch3 == '='){
								lex_item.tok = RSHIFTEQ;
								lex_item.line_num = line_num;
								return lex_item;

							} else {
								PUT_BACK_CHAR(fl);
								lex_item.tok = R_SHIFT;
								lex_item.line_num = line_num;
								return lex_item;
							}

						} else if(ch2 == '=') {
							lex_item.tok = G_THAN_OR_EQ;
							lex_item.line_num = line_num;
							return lex_item;
						} else {
							PUT_BACK_CHAR(fl);
							lex_item.tok = G_THAN;
							lex_item.line_num = line_num;
							return lex_item;
						}
						break;

					default:
						if((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '$' || ch == '#' || ch == '_'){
							//Allocate the lexeme
							lexeme = dynamic_string_alloc();
							//Add the char in
							dynamic_string_add_char_to_back(&lexeme, ch);
							//We are now in an identifier
							current_state = IN_IDENT;
						//If we get here we have the start of either an int or a real
						} else if(ch >= '0' && ch <= '9'){
							//Allocate the lexeme
							lexeme = dynamic_string_alloc();
							//Add the character to the lexeme
							dynamic_string_add_char_to_back(&lexeme, ch);
							//We are not in an int
							current_state = IN_INT;
						} else {
							lex_item.tok = ERROR;
							lex_item.line_num = line_num;
							return lex_item;
						}
				}

				break;

			case IN_IDENT:
				//Is it a number, letter, or _ or $?. If so, we can have it in our ident
				if(ch == '_' || ch == '$' || (ch >= 'a' && ch <= 'z') 
				   || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')){
					//Add the character to the lexeme
					dynamic_string_add_char_to_back(&lexeme, ch);
				} else {
					//If we get here, we need to get out of the thing
					//We'll put this back as we went too far
					PUT_BACK_CHAR(fl);
					//Return if we have ident or keyword
					return identifier_or_keyword(lexeme, line_num);
				}

				break;

			case IN_INT:
				//Add it in and move along
				if(ch >= '0' && ch <= '9'){
					dynamic_string_add_char_to_back(&lexeme, ch);
				//If we see hex and we're in hex, it's also fine
				} else if(((ch >= 'a' && ch <= 'f') && seen_hex == TRUE) 
						|| ((ch >= 'A' && ch <= 'F') && seen_hex == TRUE)){
					dynamic_string_add_char_to_back(&lexeme, ch);
				} else {
					switch(ch){
						case 'x':
						case 'X':
							//Have we seen the hex code?
							//Fail case here
							if(seen_hex == TRUE){
								lexitem_t err;
								err.tok = ERROR;
								return err;
							}

							//If we haven't seen the 0 here it's bad
							if(*(lexeme.string) != '0'){
								lexitem_t err;
								err.tok = ERROR;
								return err;
							}

							//Otherwise set this and add it in
							seen_hex = TRUE;

							//Add the character dynamically
							dynamic_string_add_char_to_back(&lexeme, ch);

							break;

						case '.':
							//We're actually in a float const
							current_state = IN_FLOAT;
							//Add the character dynamically
							dynamic_string_add_char_to_back(&lexeme, ch);

							break;

						//The 'l' or 'L' tells us that we're forcing to long
						case 'l':
						case 'L':
							lex_item.line_num = line_num;
							lex_item.lexeme = lexeme;
							lex_item.tok = LONG_CONST;
							return lex_item;

						//Forcing to short
						case 's':
						case 'S':
							lex_item.line_num = line_num;
							lex_item.lexeme = lexeme;
							lex_item.tok = LONG_CONST;
							return lex_item;

						//Forcing to Byte
						case 'b':
						case 'B':
							lex_item.line_num = line_num;
							lex_item.lexeme = lexeme;
							lex_item.tok = BYTE_CONST;
							return lex_item;

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
									break;

								case 's':
								case 'S':
									lex_item.tok = SHORT_CONST_FORCE_U;
									break;

								case 'b':
								case 'B':
									lex_item.tok = BYTE_CONST_FORCE_U;
									break;

								default:
									//Put it back
									PUT_BACK_CHAR(fl);
									lex_item.tok = INT_CONST_FORCE_U;

									break;
							}


							//Pack everything up and return
							lex_item.lexeme = lexeme;
							lex_item.line_num = line_num;

							return lex_item;

						default:
							//Otherwise we're out
							//"Put back" the char
							PUT_BACK_CHAR(fl);

							//Populate and return
							if(seen_hex == TRUE){
								lex_item.tok = HEX_CONST;
							} else {
								lex_item.tok = INT_CONST;
							}

							lex_item.lexeme = lexeme;
							lex_item.line_num = line_num;
							return lex_item;
					}
				}

				break;

			case IN_FLOAT:
				//We're just in a regular float here
				if(ch >= '0' && ch <= '9'){
					//Add the character in
					dynamic_string_add_char_to_back(&lexeme, ch);

				//We can now see a D or d here that tells
				//us to force this to double precision
				} else if(ch == 'd' || ch == 'D'){
					//Give this back as a double CONST
					lex_item.tok = DOUBLE_CONST;
					lex_item.lexeme = lexeme;
					lex_item.line_num = line_num;
					return lex_item;

				} else {
					//Put back the char
					PUT_BACK_CHAR(fl);
					
					//We'll give this back now
					lex_item.tok = FLOAT_CONST;
					lex_item.lexeme = lexeme;
					lex_item.line_num = line_num;
					return lex_item;
				}

				break;

			case IN_STRING:
				//If we see the end of the string
				if(ch == '"'){ 
					//Set the token
					lex_item.tok = STR_CONST;
					//Set the lexeme & line num
					lex_item.lexeme = lexeme;
					lex_item.line_num = line_num;
					return lex_item;
				//Escape char
				} else if (ch == '\\'){
					//Consume the next character, whatever it is
					is_whitespace(fgetc(fl), &line_num, parser_line_num);
				} else {
					//Otherwise we'll just keep adding here
					//Just for line counting
					is_whitespace(ch, &line_num, parser_line_num);
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
				is_whitespace(ch, &line_num, parser_line_num);
				break;

			//If we're in a single line comment
			case IN_SINGLE_COMMENT:
				//Are we at the start of the escape sequence
				//Newline means we get out
				if(ch == '\n'){
					line_num++;
					(*parser_line_num)++;
					current_state = IN_START;
				} 
				//Otherwise just go forward
				break;

			//Some very weird error here
			default:
				fprintf(stderr, "[LEXER ERROR]: Found a stateless token\n");
				return lex_item;
		}
	}

	//Return this token
	if(ch == EOF){
		lex_item.tok = DONE;
		lex_item.line_num = *parser_line_num;
	}

	return lex_item;

}

/**
 * Initialize the lexer by dynamically allocating the lexstack
 * and any other needed data structures
 */
ollie_token_stream_t tokenize(FILE* fl){
	//Stack allocate
	ollie_token_stream_t token_stream;

	//Initialize the internal storage for our token
	token_stream.token_stream = calloc(DEFAULT_TOKEN_COUNT, sizeof(ollie_token_t));
	//Initialize to our default
	token_stream.max_token_index = DEFAULT_TOKEN_COUNT;
	//Initialize to 0
	token_stream.current_token_index = 0;
	//From the parsing perspective
	token_stream.token_pointer = 0;

	//Consume all of the tokens here using the helper
	generate_all_tokens(fl, &token_stream);

	//Give it back
	return token_stream;
}


/**
 * Print out a token and it's associated line number
*/
void print_token(lexitem_t* l){
	if(l->lexeme.string == NULL){
		//Print out with nice formatting
		printf("TOKEN: %3d, Lexeme %15s, Line: %4d\n", l->tok, "NONE", l->line_num);
	} else {
		//Print out with nice formatting
		printf("TOKEN: %3d, Lexeme: %15s, Line: %4d\n", l->tok, l->lexeme.string, l->line_num);
	}
}


/**
 * Deinitialize the entire lexer
 */
void deinitialize_lexer(){

}
