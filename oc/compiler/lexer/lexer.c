/**
 * Lexical analyzer and tokenizer for Ollie-language
 * GOAL: A lexical analyzer(also called a tokenizer, lexer, etc) runs through a source code file and "chunks" it into 
 * tokens. These tokens represent valid "lexemes" in the language. It will also determine if there are any invalid characters and pass
 * that information along to the parser.
 *
 * There is only one function that is exposed to external files, which is the "get_next_token()" function. This function simply returns
 * the next token from the file. When EOF is reached, a special DONE token is passed along so that the parser knows when to stop
*/

#include "lexer.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>

//We will use this to keep track of what the current lexer state is
typedef enum {
	START,
	IN_IDENT,
	IN_INT,
	IN_FLOAT,
	IN_STRING,
	IN_COMMENT
} Lex_state;


/* ============================================= GLOBAL VARIABLES  ============================================ */
//Current line num
static u_int16_t line_num;

//The number of characters in the current token
int16_t token_char_count;

/* ============================================= GLOBAL VARIABLES  ============================================ */


/**
 * Helper that will determine if we have whitespace(ws) 
 */
static u_int8_t is_ws(char ch, u_int16_t* line_num, u_int16_t* parser_line_num){
	u_int8_t is_ws = ch == ' ' || ch == '\n' || ch == '\t';
	
	//Count if we have a higher line number
	if(ch == '\n'){
		(*line_num)++;
		(*parser_line_num)++;
	}

	return is_ws;
}


/**
 * Determines if an identifier is a keyword or some user-written identifier
 */
static Lexer_item identifier_or_keyword(char* lexeme, u_int16_t line_number){
	Lexer_item lex_item;
	//Assign our line number;
	lex_item.line_num = line_number;

	//Token array, we will index using their enum values
	const Token tok_arr[] = {IF, THEN, ELSE, DO, WHILE, FOR, TRUE, FALSE, FUNC, RET, JUMP, LINK,
						STATIC, COMPTIME, EXTERNAL, U_INT8, S_INT8, U_INT16, S_INT16,
						U_INT32, S_INT32, U_INT64, S_INT64, FLOAT32, FLOAT64, CHAR, STR, SIZE, DEFINE, ENUMERATED, ON,
						REGISTER, CONSTANT, VOID, TYPESIZE, LET, DECLARE, WHEN, CASE, DEFAULT, SWITCH, BREAK, CONTINUE, 
						ASN, STRUCTURE, AS};

	//Direct one to one mapping
	char* keyword_arr[] = {"if", "then", "else", "do", "while", "for", "True", "False", "func", "ret", "jump",
								 "link", "static", "comptime", "external", "u_int8", "s_int8", "u_int16",
								 "s_int16", "u_int32", "s_int32", "u_int64", "s_int64", "float32", "float64", 
								  "char", "str", "size", "define", "enumerated", "on", "register", "constant",
								  "void", "typesize", "let", "declare", "when", "case", "default", "switch",
								  "break", "continue", "asn", "structure", "as"};

	//Let's see if we have a keyword here
	for(u_int8_t i = 0; i < 45; i++){
		if(strcmp(keyword_arr[i], lexeme) == 0){
			//We can get out of here
			lex_item.tok = tok_arr[i];
			lex_item.lexeme = keyword_arr[i];
			lex_item.char_count = token_char_count;
			return lex_item;
		}
	}

	//Otherwise if we get here, it could be a regular ident or a label ident
	if(*lexeme == '$'){
		lex_item.tok = LABEL_IDENT;
	} else {
		lex_item.tok = IDENT;
	}
	
	//If we get here, we know that it's an ident
	lex_item.lexeme = lexeme;
	lex_item.char_count = token_char_count;

	return lex_item;
}

static char get_next_char(FILE* fl){
	char ch = fgetc(fl);
	token_char_count++;
	return ch;
}


/**
 * Put back the char and update the token char num appropriately
 */
static void put_back_char(FILE* fl){
	fseek(fl, -1, SEEK_CUR);
	token_char_count--;
}


/**
 * Constantly iterate through the file and grab the next token that we have
*/
Lexer_item get_next_token(FILE* fl, u_int16_t* parser_line_num){
	//We'll eventually return this
	Lexer_item lex_item;

	//If we're at the start -- added to avoid overcounts
	if(ftell(fl) == 0){
		line_num = 0;
		*parser_line_num = 0;
	}

	//We begin in the start state
	Lex_state current_state = START;

	//Whenever we're in start we're automatically at 0
	token_char_count = 0;
	//Current char we have
	char ch;
	char ch2;
	//Store the lexeme
	char lexeme[10000];

	//The next index for the lexeme
	char* lexeme_cursor = lexeme;

	//We'll run through character by character until we hit EOF
	while((ch = get_next_char(fl)) != EOF){
		//Switch on the current state
		switch(current_state){
			case START:
				//We've seen 1 token to be here
				token_char_count = 1;

				//If we see whitespace we just get out
				if(is_ws(ch, &line_num, parser_line_num)){
					continue;
				}

				//Let's see what we have here
				switch(ch){
					//We could be seeing a comment here
					case '/':
						//Grab the next char, if we see a '*' then we're in a comment
						ch2 = get_next_char(fl);
							
						//If we're here we have a comment
						if(ch2 == '*'){
							current_state = IN_COMMENT;
							break;

						//Otherwise we could have '=/'
						} else if(ch2 == '='){
							current_state = START;
							//Prepare the token and return it
							lex_item.tok = DIV_EQUALS;
							lex_item.lexeme = "/=";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;

						//Otherwise we just have a divide char
						} else {
							current_state = START;
							//"Put back" the char
							put_back_char(fl);

							//Prepare the token and return it
							lex_item.tok = F_SLASH;
							lex_item.lexeme = "/";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						}

					case '+':
						ch2 = get_next_char(fl);
						
						//If we get this then it's +=
						if(ch2 == '='){
							current_state = START;
							//Prepare and return
							lex_item.tok = PLUS_EQUALS;
							lex_item.lexeme = "+=";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else if(ch2 == '+'){
							current_state = START;
							//Prepare and return
							lex_item.tok = PLUSPLUS;
							lex_item.lexeme = "++";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else {
							current_state = START;
							//"Put back" the char
							put_back_char(fl);	

							lex_item.tok = PLUS;
							lex_item.lexeme = "+";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						}

					case '-':
						ch2 = get_next_char(fl);

						//If we get this then it's +=
						if(ch2 == '='){
							current_state = START;
							//Prepare and return
							lex_item.tok = MINUS_EQUALS;
							lex_item.lexeme = "-=";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else if(ch2 == '-'){
							current_state = START;
							//Prepare and return
							lex_item.tok = MINUSMINUS;
							lex_item.lexeme = "--";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						//We can also have an arrow
						} else if(ch2 == '>'){
							current_state = START;
							//Prepare and return
							lex_item.tok = ARROW;
							lex_item.lexeme = "->";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else {
							current_state = START;
							//"Put back" the char
							put_back_char(fl);

							lex_item.tok = MINUS;
							lex_item.lexeme = "-";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						}

					case '*':
						ch2 = get_next_char(fl);
						
						//If we get this then it's +=
						if(ch2 == '='){
							current_state = START;
							//Prepare and return
							lex_item.tok = TIMES_EQUALS;
							lex_item.lexeme = "*=";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else {
							current_state = START;
							//"Put back" the char
							put_back_char(fl);

							lex_item.tok = STAR;
							lex_item.lexeme = "*";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						}

					case '=':
						ch2 = get_next_char(fl);
						
						//If we get this then it's +=
						if(ch2 == '='){
							current_state = START;
							//Prepare and return
							lex_item.tok = D_EQUALS;
							lex_item.lexeme = "==";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else if (ch2 == '?') {
							//Prepare and return
							lex_item.tok = CONDITIONAL_DEREF;
							lex_item.lexeme = "=?";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else {
							current_state = START;
							//"Put back" the char
							put_back_char(fl);

							lex_item.tok = EQUALS;
							lex_item.lexeme = "=";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						}

					case '&':
						ch2 = get_next_char(fl);					

						//If we get this then it's +=
						if(ch2 == '&'){
							current_state = START;
							//Prepare and return
							lex_item.tok = DOUBLE_AND;
							lex_item.lexeme = "&&";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else if(ch2 == '='){
							current_state = START;
							//Prepare and return
							lex_item.tok = AND_EQUALS;
							lex_item.lexeme = "&=";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else {
							current_state = START;
							//"Put back" the char
							put_back_char(fl);

							lex_item.tok = AND;
							lex_item.lexeme = "&";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						}

					case '|':
						ch2 = get_next_char(fl);
						
						//If we get this then it's +=
						if(ch2 == '|'){
							current_state = START;
							//Prepare and return
							lex_item.tok = DOUBLE_OR;
							lex_item.lexeme = "||";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else if(ch2 == '='){
							current_state = START;
							//Prepare and return
							lex_item.tok = OR_EQUALS;
							lex_item.lexeme = "|=";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else {
							current_state = START;
							//"Put back" the char
							put_back_char(fl);
							lex_item.tok = OR;
							lex_item.lexeme = "|";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						}

					case ';':
						lex_item.tok = SEMICOLON;
						lex_item.lexeme = ";";
						lex_item.line_num = line_num;
						lex_item.char_count = token_char_count;
						return lex_item;

					case '%':
						ch2 = get_next_char(fl);

						if(ch2 == '='){
							current_state = START;
							//Prepare and return
							lex_item.tok = MOD_EQUALS;
							lex_item.lexeme = "%=";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else {
							current_state = START;
							//Put it back
							put_back_char(fl);
							lex_item.tok = COLON;
							lex_item.lexeme = ":";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						}

					case ':':
						ch2 = get_next_char(fl);

						if(ch2 == ':'){
							current_state = START;
							//Prepare and return
							lex_item.tok = DOUBLE_COLON;
							lex_item.lexeme = "::";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						//We have a ":="
						} else if(ch2 == '='){
							current_state = START;
							//Prepare and return
							lex_item.tok = COLONEQ;
							lex_item.lexeme = ":=";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else {
							current_state = START;
							//Put it back
							put_back_char(fl);
							lex_item.tok = COLON;
							lex_item.lexeme = ":";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						}

					case '(':
						lex_item.tok = L_PAREN;
						lex_item.lexeme = "(";
						lex_item.line_num = line_num;
						lex_item.char_count = token_char_count;
						return lex_item;

					case ')':
						lex_item.tok = R_PAREN;
						lex_item.lexeme = ")";
						lex_item.line_num = line_num;
						lex_item.char_count = token_char_count;
						return lex_item;

					case '^':
						lex_item.tok = CARROT;
						lex_item.lexeme = "^";
						lex_item.line_num = line_num;
						lex_item.char_count = token_char_count;
						return lex_item;

					case '{':
						lex_item.tok = L_CURLY;
						lex_item.lexeme = "{";
						lex_item.line_num = line_num;
						lex_item.char_count = token_char_count;
						return lex_item;

					case '}':
						lex_item.tok = R_CURLY;
						lex_item.lexeme = "}";
						lex_item.line_num = line_num;
						lex_item.char_count = token_char_count;
						return lex_item;

					case '[':
						lex_item.tok = L_BRACKET;
						lex_item.lexeme = "[";
						lex_item.line_num = line_num;
						lex_item.char_count = token_char_count;
						return lex_item;

					case ']':
						lex_item.tok = R_BRACKET;
						lex_item.lexeme = "]";
						lex_item.line_num = line_num;
						lex_item.char_count = token_char_count;
						return lex_item;

					case '#':
						lex_item.tok = POUND;
						lex_item.lexeme = "#";
						lex_item.line_num = line_num;
						lex_item.char_count = token_char_count;
						return lex_item;

					case '`':
						lex_item.tok = CONDITIONAL_DEREF;
						lex_item.lexeme = "`";
						lex_item.line_num = line_num;
						lex_item.char_count = token_char_count;
						return lex_item;

					case '.':
						//Let's see what we have here
						ch2 = get_next_char(fl);
						if(ch2 >= '0' && ch2 <= '9'){
							//Erase this now
							memset(lexeme, 0, 10000);
							//Reset the cursor
							lexeme_cursor = lexeme;
							//We are not in an int
							current_state = IN_FLOAT;
							//Add this in
							*lexeme_cursor = ch;
							lexeme_cursor++;
							*lexeme_cursor = ch2;
							lexeme_cursor++;
						} else {
							//Put back ch2
							put_back_char(fl);
							lex_item.tok = DOT;
							lex_item.lexeme = ".";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						}

						break;
					
					case ',':
						lex_item.tok = COMMA;
						lex_item.lexeme = ",";
						lex_item.line_num = line_num;
						lex_item.char_count = token_char_count;
						return lex_item;

					//Beginning of a string literal
					case '"':
						//Say that we're in a string
						current_state = IN_STRING;
						//0 this out
						memset(lexeme, 0, 10000);
						//String literal pointer
						lexeme_cursor = lexeme;
						break;

					//Beginning of a char const
					case '\'':
						//0 this out
						memset(lexeme, 0, 10000);
						//String literal pointer
						lexeme_cursor = lexeme;

						*lexeme_cursor = '\'';
						lexeme_cursor++;

						//Grab the next char
						ch2 = get_next_char(fl);

						//Put this in
						*lexeme_cursor = ch2;
						lexeme_cursor++;

						//Now we must see another single quote
						ch2 = get_next_char(fl);

						//If this is the case, then we've messed up
						if(ch2 != '\''){
							lex_item.tok = ERROR;
							lex_item.lexeme = "Error: Char constant may be one character in length";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						}

						//Othwerise, add it in and get out
						*lexeme_cursor = '\'';
						lexeme_cursor++;

						//Package and return
						lex_item.tok = CHAR_CONST;
						lex_item.lexeme = lexeme;
						lex_item.line_num = line_num;
						lex_item.char_count = 3;
						return lex_item;

					case '<':
						//Grab the next char
						ch2 = get_next_char(fl);
						if(ch2 == '<'){
							lex_item.tok = L_SHIFT;
							lex_item.lexeme = "<<";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else if(ch2 == '=') {
							lex_item.tok = L_THAN_OR_EQ;
							lex_item.lexeme = "<=";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else {
							put_back_char(fl);
							lex_item.tok = L_THAN;
							lex_item.lexeme = "<";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						}
						break;

					case '>':
						//Grab the next char
						ch2 = get_next_char(fl);
						if(ch2 == '>'){
							lex_item.tok = R_SHIFT;
							lex_item.lexeme = ">>";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else if(ch2 == '=') {
							lex_item.tok = G_THAN_OR_EQ;
							lex_item.lexeme = ">=";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else {
							put_back_char(fl);
							lex_item.tok = G_THAN;
							lex_item.lexeme = ">";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						}
						break;

					default:
						if((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '$'){
							//Erase this now
							memset(lexeme, 0, 10000);
							//Reset the cursor
							lexeme_cursor = lexeme;
							//We are now in an identifier
							current_state = IN_IDENT;
							//Add this char into the lexeme
							*lexeme_cursor = ch;
							lexeme_cursor++;
						//If we get here we have the start of either an int or a real
						} else if(ch >= '0' && ch <= '9'){
							//Erase this now
							memset(lexeme, 0, 10000);
							//Reset the cursor
							lexeme_cursor = lexeme;
							//We are not in an int
							current_state = IN_INT;
							//Add this in
							*lexeme_cursor = ch;
							lexeme_cursor++;
						} else {
							lex_item.tok = ERROR;
							lex_item.lexeme = "Error: Invalid character provided for identifier";
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						}
				}

				break;

			case IN_IDENT:
				//Is it a number, letter, or _ or $?. If so, we can have it in our ident
				if(ch == '_' || ch == '$' || (ch >= 'a' && ch <= 'z') 
				   || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')){
					//Add it in
					*lexeme_cursor = ch;
					//Advance
					lexeme_cursor++;
				} else {
					//If we get here, we need to get out of the thing
					//We'll put this back as we went too far
					put_back_char(fl);
					//Restart the state
					current_state = START;
					//Return if we have ident or keyword
					return identifier_or_keyword(lexeme, line_num);
				}

				break;

			case IN_INT:
				//Add it in and move along
				if(ch >= '0' && ch <= '9'){
					*lexeme_cursor = ch;
					lexeme_cursor++;
				} else if (ch == '.'){
					//We're actually in a float const
					current_state = IN_FLOAT;
					*lexeme_cursor = ch;
					lexeme_cursor++;
				} else {
					//Otherwise we're out
					//"Put back" the char
					put_back_char(fl);
					//Reset the state
					current_state = START;

					//Populate and return
					lex_item.tok = INT_CONST;
					lex_item.lexeme = lexeme;
					lex_item.line_num = line_num;
					lex_item.char_count = token_char_count;
					return lex_item;
				}

				break;

			case IN_FLOAT:
				//We're just in a regular float here
				if(ch >= '0' && ch <= '9'){
					*lexeme_cursor = ch;
					lexeme_cursor++;
				} else {
					//Put back the char
					put_back_char(fl);
					//Reset the state
					current_state = START;
					
					//We'll give this back now
					lex_item.tok = FLOAT_CONST;
					lex_item.lexeme = lexeme;
					lex_item.line_num = line_num;
					lex_item.char_count = token_char_count;
					return lex_item;
				}

				break;

			case IN_STRING:
				//If we see the end of the string
				if(ch == '"'){ 
					//Reset the search
					current_state = START;
					//Set the token
					lex_item.tok = STR_CONST;
					//Set the lexeme & line num
					lex_item.lexeme = lexeme;
					lex_item.line_num = line_num;
					lex_item.char_count = token_char_count;
					return lex_item;
				} else {
					//Otherwise we'll just keep adding here
					*lexeme_cursor = ch;
					lexeme_cursor++;
				}

				break;

			//If we're in a comment, we can escape if we see "*/"
			case IN_COMMENT:
				//Are we at the start of an escape sequence?
				if(ch == '*'){
					ch2 = fgetc(fl);	
					if(ch2 == '/'){
						//We are now out of the comment
						current_state = START;
					} else {
						//"Put back" char2
						fseek(fl, -1, SEEK_CUR);
					}
				}

				//If we see whitespace we'll just increment the line number
				is_ws(ch, &line_num, parser_line_num);
				break;
		}
	}

	//Return this token
	if(ch == EOF){
		lex_item.tok = DONE;
		lex_item.lexeme = "DONE";
		lex_item.char_count = 1;
	}

	return lex_item;
}


/**
 * Push a token back by moving the seek head back appropriately
 */
void push_back_token(FILE* fl, Lexer_item l){
	int16_t back = -1 * l.char_count;
	//Push our stream back appropriately
	fseek(fl, back, SEEK_CUR);
}


/**
 * Print out a token and it's associated line number
*/
void print_token(Lexer_item* l){
	//Print out with nice formatting
	printf("TOKEN: %3d, Lexeme: %10s, Line: %4d, Characters: %4d\n", l->tok, l->lexeme, l->line_num, l->char_count);
}


