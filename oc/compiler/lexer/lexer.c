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
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include "../stack/lexstack.h"

//We will use this to keep track of what the current lexer state is
typedef enum {
	IN_START,
	IN_IDENT,
	IN_INT,
	IN_FLOAT,
	IN_STRING,
	IN_MULTI_COMMENT,
	IN_SINGLE_COMMENT
} Lex_state;


/* ============================================= GLOBAL VARIABLES  ============================================ */
//Current line num
u_int16_t line_num = 0;

//The number of characters in the current token
int16_t token_char_count = 0;

//Our lexer stack
lex_stack_t* pushed_back_tokens = NULL;

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
	//Wipe this clean
	memset(lex_item.lexeme, 0, MAX_TOKEN_LENGTH*sizeof(char));

	//Assign our line number;
	lex_item.line_num = line_number;

	//Token array, we will index using their enum values
	const Token tok_arr[] = {IF, THEN, ELSE, DO, WHILE, FOR, FN, RET, JUMP, REQUIRE, REPLACE, 
						STATIC, EXTERNAL, U_INT8, S_INT8, U_INT16, S_INT16,
						U_INT32, S_INT32, U_INT64, S_INT64, FLOAT32, FLOAT64, CHAR, DEFINE, ENUM, ON,
						REGISTER, CONSTANT, VOID, TYPESIZE, LET, DECLARE, WHEN, CASE, DEFAULT, SWITCH, BREAK, CONTINUE, 
						CONSTRUCT, AS, ALIAS, SIZEOF, DEFER, MUT, DEPENDENCIES, ASM, WITH, LIB, IDLE};

	//Direct one to one mapping
	char* keyword_arr[] = {"if", "then", "else", "do", "while", "for", "fn", "ret", "jump",
								 "require", "#replace", "static", "external", "u8", "i8", "u16",
								 "i16", "u32", "i32", "u64", "i64", "f32", "f64", 
								  "char", "define", "enum", "on", "register", "constant",
								  "void", "typesize", "let", "declare", "when", "case", "default", "switch",
								  "break", "continue", "construct", "as", "alias", "sizeof", "defer", "mut", "#dependencies", "asm",
								  "with", "lib", "idle"};

	//Let's see if we have a keyword here
	for(u_int8_t i = 0; i < 50; i++){
		if(strcmp(keyword_arr[i], lexeme) == 0){
			//We can get out of here
			lex_item.tok = tok_arr[i];
			strcpy(lex_item.lexeme, keyword_arr[i]);
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
	
	//Fail out if too long
	if(strlen(lex_item.lexeme) >= MAX_IDENT_LENGTH){
		printf("[LINE %d | LEXER ERROR]: Identifiers may be at most 100 characters long\n", line_number);
		lex_item.tok = ERROR;
		return lex_item;
	}

	//If we get here, we know that it's an ident
	strcpy(lex_item.lexeme, lexeme);
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
 * A special case here where we get the next assembly inline statement. Assembly
 * inline statements are officially terminated by a backslash "\", so we 
 * will simply run through what we have here until we get to that backslash. We'll
 * then pack what we had into a lexer item and send it back to the caller
 */
Lexer_item get_next_assembly_statement(FILE* fl, u_int16_t* parser_line_num){
	//We'll be giving this back
	Lexer_item asm_statement;
	asm_statement.tok = ASM_STATEMENT;
	//Wipe this while we're at it
	memset(asm_statement.lexeme, 0, MAX_TOKEN_LENGTH*sizeof(char));

	//Searching char
	char ch;

	//For lexeme concatenation
	char* lexeme_cursor = asm_statement.lexeme;
	u_int16_t lexeme_index = 0;

	//First pop off all of the tokens if there are any on the stack
	while(lex_stack_is_empty(pushed_back_tokens) == 0){
		//Pop whatever we have off
		Lexer_item token = pop_token(pushed_back_tokens);
		//Add it in here
		strcat(asm_statement.lexeme, token.lexeme);
		//Update the char count
		lexeme_cursor += token.char_count;
	}

	//So long as we don't see a backslash, we keep going
	ch = get_next_char(fl);

	//So long as we don't see this move along, adding ch into 
	//our lexeme
	while(ch != '\\' && lexeme_index < MAX_TOKEN_LENGTH){
		//Whitespace tracking
		is_ws(ch, &line_num, parser_line_num);

		//Add it in
		*lexeme_cursor = ch;
		lexeme_cursor++;
		//Add one more to this too
		lexeme_index++;

		//Refresh the char
		ch = get_next_char(fl);
	}

	//If for some reason we overran the length, we give back an error
	if(lexeme_index >= MAX_TOKEN_LENGTH){
		asm_statement.tok = ERROR;
		return asm_statement;
	}

	//Grab the char count
	asm_statement.char_count = token_char_count;

	//Otherwise we're done
	return asm_statement;
}


/**
 * Constantly iterate through the file and grab the next token that we have
*/
Lexer_item get_next_token(FILE* fl, u_int16_t* parser_line_num, const_search_t const_search){
	//If this is NULL, we need to make it
	if(pushed_back_tokens == NULL){
		pushed_back_tokens = create_lex_stack();
	}

	//IF we have pushed back tokens, we need to return them first
	if(lex_stack_is_empty(pushed_back_tokens) == 0){
		//Just pop this and leave
		return pop_token(pushed_back_tokens);
	}


	//We'll eventually return this
	Lexer_item lex_item;
	//By default it's an error
	lex_item.tok = ERROR;

	//Have we seen hexadecimal?
	u_int8_t seen_hex = 0;
	//Are we forcing to unsigned
	u_int8_t forced_to_unsigned = 0;

	//If we're at the start -- added to avoid overcounts
	if(ftell(fl) == 0){
		line_num = 1;
		*parser_line_num = 1;
	}

	//We begin in the start state
	Lex_state current_state = IN_START;

	//Whenever we're in start we're automatically at 0
	token_char_count = 0;
	//Current char we have
	char ch;
	char ch2;
	//Store the lexeme
	char lexeme[1000];

	//The next index for the lexeme
	char* lexeme_cursor = lexeme;

	//We'll run through character by character until we hit EOF
	while((ch = get_next_char(fl)) != EOF){
		//Check to make sure we aren't overrunning our bounds
		if(current_state != IN_MULTI_COMMENT && token_char_count > MAX_TOKEN_LENGTH-1){
			Lexer_item l;
			l.tok = ERROR;
			return l;
		}

		//Switch on the current state
		switch(current_state){
			case IN_START:
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
							current_state = IN_MULTI_COMMENT;
							break;
						} else if(ch2 == '/'){
							current_state = IN_SINGLE_COMMENT;
							break;
						} else {
							current_state = IN_START;
							//"Put back" the char
							put_back_char(fl);

							//Prepare the token and return it
							lex_item.tok = F_SLASH;
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						}

					case '+':
						ch2 = get_next_char(fl);
						
						//We could see++
						if(ch2 == '+'){
							current_state = IN_START;
							//Prepare and return
							lex_item.tok = PLUSPLUS;
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else {
							current_state = IN_START;
							//"Put back" the char
							put_back_char(fl);	

							lex_item.tok = PLUS;
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						}

					case '-':
						ch2 = get_next_char(fl);

						if(ch2 == '-'){
							current_state = IN_START;
							//Prepare and return
							lex_item.tok = MINUSMINUS;
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						//We can also have an arrow
						} else if(ch2 == '>'){
							current_state = IN_START;
							//Prepare and return
							lex_item.tok = ARROW;
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						//If we're looking for a constant, there are more options
						//here. This could be a negative sign.
						} else if(const_search == SEARCHING_FOR_CONSTANT){
							//Wipe this out now that we're here
							memset(lexeme, 0, MAX_TOKEN_LENGTH);
							//Set this too -- for iteration
							lexeme_cursor = lexeme;

							//We're in an int
							if(ch2 >= '0' && ch2 <= '9'){
								*lexeme_cursor = ch;
								*(lexeme_cursor + 1) = ch2;
								lexeme_cursor += 2;
								current_state = IN_INT;
								break;
							}

							//We're in a float
							if(ch2 == '.'){
								*lexeme_cursor = ch;
								lexeme_cursor += 1;
								current_state = IN_FLOAT;
								break;
							}
							
							//Break otherwise
							break;

						//Otherwise we didn't find anything here
						} else {
							current_state = IN_START;
							//"Put back" the char
							put_back_char(fl);

							lex_item.tok = MINUS;
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						}

					case '*':
						current_state = IN_START;
						lex_item.tok = STAR;
						lex_item.line_num = line_num;
						lex_item.char_count = token_char_count;
						return lex_item;

					case '=':
						ch2 = get_next_char(fl);
						
						//If we get this then it's +=
						if(ch2 == '='){
							current_state = IN_START;
							//Prepare and return
							lex_item.tok = D_EQUALS;
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else if(ch2 == '>'){
							current_state = IN_START;
							//Prepare and return
							lex_item.tok = ARROW_EQ;
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else {
							current_state = IN_START;
							//"Put back" the char
							put_back_char(fl);

							lex_item.tok = EQUALS;
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						}

					case '&':
						ch2 = get_next_char(fl);

						if(ch2 == '&'){
							current_state = IN_START;
							//Prepare and return
							lex_item.tok = DOUBLE_AND;
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else {
							put_back_char(fl);
							current_state = IN_START;
							lex_item.tok = AND;
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						}

					case '|':
						ch2 = get_next_char(fl);
						//If we get this then it's +=
						if(ch2 == '|'){
							current_state = IN_START;
							//Prepare and return
							lex_item.tok = DOUBLE_OR;
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else {
							current_state = IN_START;
							//"Put back" the char
							put_back_char(fl);
							lex_item.tok = OR;
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						}

					case ';':
						lex_item.tok = SEMICOLON;
						strcpy(lex_item.lexeme, ";");
						lex_item.line_num = line_num;
						lex_item.char_count = token_char_count;
						return lex_item;

					case '%':
						current_state = IN_START;
						lex_item.tok = MOD;
						lex_item.line_num = line_num;
						lex_item.char_count = token_char_count;
						return lex_item;

					case ':':
						ch2 = get_next_char(fl);
						if(ch2 == ':'){
							current_state = IN_START;
							//Prepare and return
							lex_item.tok = DOUBLE_COLON;
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						//We have a ":="
						} else if(ch2 == '='){
							current_state = IN_START;
							//Prepare and return
							lex_item.tok = COLONEQ;
							strcpy(lex_item.lexeme, ":=");
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else {
							current_state = IN_START;
							//Put it back
							put_back_char(fl);
							lex_item.tok = COLON;
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						}

					case '(':
						lex_item.tok = L_PAREN;
						lex_item.line_num = line_num;
						lex_item.char_count = token_char_count;
						return lex_item;

					case ')':
						lex_item.tok = R_PAREN;
						lex_item.line_num = line_num;
						lex_item.char_count = token_char_count;
						return lex_item;

					case '^':
						lex_item.tok = CARROT;
						lex_item.line_num = line_num;
						lex_item.char_count = token_char_count;
						return lex_item;

					case '{':
						lex_item.tok = L_CURLY;
						strcpy(lex_item.lexeme, "{");
						lex_item.line_num = line_num;
						lex_item.char_count = token_char_count;
						return lex_item;

					case '}':
						lex_item.tok = R_CURLY;
						lex_item.line_num = line_num;
						lex_item.char_count = token_char_count;
						return lex_item;

					case '[':
						lex_item.tok = L_BRACKET;
						lex_item.line_num = line_num;
						lex_item.char_count = token_char_count;
						return lex_item;

					case ']':
						lex_item.tok = R_BRACKET;
						lex_item.line_num = line_num;
						lex_item.char_count = token_char_count;
						return lex_item;

					case '@':
						lex_item.tok = AT;
						lex_item.line_num = line_num;
						lex_item.char_count = token_char_count;
						return lex_item;

					case '.':
						//Let's see what we have here
						ch2 = get_next_char(fl);
						if(ch2 >= '0' && ch2 <= '9'){
							//Erase this now
							memset(lexeme, 0, 1000);
							//Reset the cursor
							lexeme_cursor = lexeme;
							//We are not in an int
							current_state = IN_FLOAT;
							//Add this in
							*lexeme_cursor = ch;
							lexeme_cursor++;
							*lexeme_cursor = ch2;
							lexeme_cursor++;
						} else if(ch2 == '.'){
							//Let's see if ch3 is '.'
							char ch3 = get_next_char(fl);
							//We have a DOTDOTDOT
							if(ch3 == '.'){
								lex_item.tok = DOTDOTDOT;
								lex_item.line_num = line_num;
								lex_item.char_count = token_char_count;
								//Give it back
								return lex_item;
							}

							//Otherwise, we'll put them both back
							fseek(fl, -2, SEEK_CUR);

							//And return a DOT
							lex_item.tok = DOT;	
							lex_item.line_num = line_num;
							lex_item.char_count = 1;
							return lex_item;

						} else {
							//Put back ch2
							put_back_char(fl);
							lex_item.tok = DOT;
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						}

						break;
					
					case ',':
						lex_item.tok = COMMA;
						strcpy(lex_item.lexeme, ",");
						lex_item.line_num = line_num;
						lex_item.char_count = token_char_count;
						return lex_item;

					case '~':
						lex_item.tok = B_NOT;
						lex_item.line_num = line_num;
						lex_item.char_count = token_char_count;
						return lex_item;

					case '!':
						ch2 = get_next_char(fl);
						if(ch2 == '='){
							current_state = IN_START;
							//Prepare and return
							lex_item.tok = NOT_EQUALS;
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else {
							current_state = IN_START;
							//Put it back
							put_back_char(fl);
							lex_item.tok = L_NOT;
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						}

					//Beginning of a string literal
					case '"':
						//Say that we're in a string
						current_state = IN_STRING;
						//0 this out
						memset(lexeme, 0, 1000);
						//String literal pointer
						lexeme_cursor = lexeme;
						break;

					//Beginning of a char const
					case '\'':
						//0 this out
						memset(lexeme, 0, 1000);
						//String literal pointer
						lexeme_cursor = lexeme;

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
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						}

						//Package and return
						lex_item.tok = CHAR_CONST;
						strcpy(lex_item.lexeme, lexeme);
						lex_item.line_num = line_num;
						lex_item.char_count = 3;
						return lex_item;

					case '<':
						//Grab the next char
						ch2 = get_next_char(fl);
						if(ch2 == '<'){
							lex_item.tok = L_SHIFT;
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else if(ch2 == '=') {
							lex_item.tok = L_THAN_OR_EQ;
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else {
							put_back_char(fl);
							lex_item.tok = L_THAN;
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
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else if(ch2 == '=') {
							lex_item.tok = G_THAN_OR_EQ;
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						} else {
							put_back_char(fl);
							lex_item.tok = G_THAN;
							lex_item.line_num = line_num;
							lex_item.char_count = token_char_count;
							return lex_item;
						}
						break;

					default:
						if((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '$' || ch == '#'){
							//Erase this now
							memset(lexeme, 0, MAX_TOKEN_LENGTH);
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
							memset(lexeme, 0, MAX_TOKEN_LENGTH);
							//Reset the cursor
							lexeme_cursor = lexeme;
							//We are not in an int
							current_state = IN_INT;
							//Add this in
							*lexeme_cursor = ch;
							lexeme_cursor++;
						} else {
							lex_item.tok = ERROR;
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
					current_state = IN_START;
					//Return if we have ident or keyword
					return identifier_or_keyword(lexeme, line_num);
				}

				break;

			case IN_INT:
				//Add it in and move along
				if(ch >= '0' && ch <= '9'){
					*lexeme_cursor = ch;
					lexeme_cursor++;
				//If we see hex and we're in hex, it's also fine
				} else if(((ch >= 'a' && ch <= 'f') && seen_hex == 1) 
						|| ((ch >= 'A' && ch <= 'F') && seen_hex == 1)){
						*lexeme_cursor = ch;
						lexeme_cursor++;
				} else if(ch == 'x' || ch == 'X'){
					//Have we seen the hex code?
					//Fail case here
					if(seen_hex == 1){
						Lexer_item err;
						err.tok = ERROR;
						return err;
					}

					if(*(lexeme_cursor-1) != '0'){
						Lexer_item err;
						err.tok = ERROR;
						return err;

					}

					//Otherwise set this and add it in
					seen_hex = 1;

					*lexeme_cursor = ch;
					lexeme_cursor++;
				
				} else if (ch == '.'){
					//We're actually in a float const
					current_state = IN_FLOAT;
					*lexeme_cursor = ch;
					lexeme_cursor++;

				} else if (ch == 'l'){
					//We have seen a long constant
					current_state = IN_START;
					strcpy(lex_item.lexeme, lexeme);
					lex_item.line_num = line_num;
					lex_item.char_count = token_char_count;
					lex_item.tok = LONG_CONST;
					return lex_item;

				} else if (ch == 'u' || ch == 'U'){
					//We are forcing this to be unsigned
					//We can still see "l", so let's check
					ch2 = get_next_char(fl);

					//If this is an l, it's a long
					if(ch2 == 'l'){
						lex_item.tok = LONG_CONST_FORCE_U;
					} else {
						//Put it back
						put_back_char(fl);
						lex_item.tok = INT_CONST_FORCE_U;
					}

					//Pack everything up and return
					current_state = IN_START;
					strcpy(lex_item.lexeme, lexeme);
					lex_item.line_num = line_num;
					lex_item.char_count = token_char_count;

					return lex_item;

				} else {
					//Otherwise we're out
					//"Put back" the char
					put_back_char(fl);
					//Reset the state
					current_state = IN_START;

					//Populate and return
					if(seen_hex == 1){
						lex_item.tok = HEX_CONST;
					} else {
						lex_item.tok = INT_CONST;
					}

					strcpy(lex_item.lexeme, lexeme);
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
					current_state = IN_START;
					
					//We'll give this back now
					lex_item.tok = FLOAT_CONST;
					strcpy(lex_item.lexeme, lexeme);
					lex_item.line_num = line_num;
					lex_item.char_count = token_char_count;
					return lex_item;
				}

				break;

			case IN_STRING:
				//If we see the end of the string
				if(ch == '"'){ 
					//Reset the search
					current_state = IN_START;
					//Set the token
					lex_item.tok = STR_CONST;
					//Set the lexeme & line num
					strcpy(lex_item.lexeme, lexeme);
					lex_item.line_num = line_num;
					lex_item.char_count = token_char_count;
					return lex_item;
				//Escape char
				} else if (ch == '\\'){ //TODO LOOK AT ME
					//Consume the next character, whatever it is
					is_ws(fgetc(fl), &line_num, parser_line_num);
				} else {
					//Otherwise we'll just keep adding here
					//Just for line counting
					is_ws(ch, &line_num, parser_line_num);
					*lexeme_cursor = ch;
					lexeme_cursor++;
				}

				break;

			//If we're in a comment, we can escape if we see "*/"
			case IN_MULTI_COMMENT:
				//Are we at the start of an escape sequence?
				if(ch == '*'){
					ch2 = get_next_char(fl);
					if(ch2 == '/'){
						//We are now out of the comment
						current_state = IN_START;
						//Reset the char count
						token_char_count = 0;
						break;
					} else {
						put_back_char(fl);
						break;
					}
				}
				//Otherwise just check for whitespace
				is_ws(ch, &line_num, parser_line_num);
				break;

			//If we're in a single line comment
			case IN_SINGLE_COMMENT:
				//Are we at the start of the escape sequence
				//Newline means we get out
				if(ch == '\n'){
					line_num++;
					(*parser_line_num)++;
					current_state = IN_START;
					//Reset the char count
					token_char_count = 0;
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
		lex_item.char_count = 0;
		lex_item.line_num = *parser_line_num;
		//Destroy the stack
		destroy_lex_stack(pushed_back_tokens);
	}

	return lex_item;
}


/**
 * Push a token back by moving the seek head back appropriately
 */
void push_back_token(Lexer_item l){
	//All that we need to do here is push the token onto the stack
	push_token(pushed_back_tokens, l);
}


/**
 * Print out a token and it's associated line number
*/
void print_token(Lexer_item* l){
	//Print out with nice formatting
	printf("TOKEN: %3d, Lexeme: %10s, Line: %4d, Characters: %4d\n", l->tok, l->lexeme, l->line_num, l->char_count);
}


