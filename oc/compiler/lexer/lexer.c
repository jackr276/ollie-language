/**
 * Lexical analyzer and tokenizer for Ollie-language
*/

#include "lexer.h"
#include <string.h>
#include <stdio.h>
#include <sys/types.h>


/**
 * Helper that will determine if we have whitespace(ws) 
 */
static u_int8_t is_ws(char ch){
	return ch == ' ' || ch == '\n' || ch == '\t';
}



static Lexer_item identifier_or_keyword(const char* lexeme, u_int16_t line_number){
	Lexer_item lex_item;
	//Assign our line number;
	lex_item.line_num = line_number;

	//Token array, we will index using their enum values
	const Token tok_arr[] = {IF, THEN, ELSE, DO, WHILE, FOR, TRUE, FALSE, FUNC, RET,
						STATIC, EXTERNAL, REF, DEREF, MEMADDR};

	const char* keyword_arr[] = {"if", "then", "else", "do", "while", "for", "true", "false",
								 "static", "external", "ref", "deref", "memaddr"};

	//Let's see if we have a keyword here
	for(u_int8_t i = 0; i < 15; i++){
		if(strcmp(keyword_arr[i], lexeme) == 0){
			//We can get out of here
			lex_item.tok = tok_arr[i];
			lex_item.lexeme = keyword_arr[i];
			return lex_item;
		}
	}

	//If we get here, we know that it's an ident
	lex_item.tok = IDENT;
	lex_item.lexeme = lexeme;

	return lex_item;
}


/**
 * Constantly iterate through the file and grab the next token that we have
*/
Lexer_item get_next_token(FILE* fl){
	//We'll eventually return this
	Lexer_item lex_item;

	//We begin in the start state
	Lex_state current_state = START;

	//Current char we have
	char ch;
	char ch2;

	//We'll run through character by character until we hit EOF
	while((ch = fgetc(fl)) != EOF){
		//Skip all whitespace--Ollie is whitespace agnostic
		if(is_ws(ch) == 1){
			continue;
		}

		//Switch on the current state
		switch(current_state){
			case START:
				//Let's see what we have here
				switch(ch){
					//We could be seeing a comment here
					case '/':
						//Grab the next char, if we see a '*' then we're in a comment
						ch2 = fgetc(fl);
						
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
							return lex_item;

						//Otherwise we just have a divide char
						} else {
							current_state = START;
							//"Put back" the char
							fseek(fl, -1, SEEK_CUR);
							//Prepare the token and return it
							lex_item.tok = F_SLASH;
							lex_item.lexeme = "/";
							return lex_item;
						}

					case '+':
						ch2 = fgetc(fl);
						
						//If we get this then it's +=
						if(ch2 == '='){
							current_state = START;
							//Prepare and return
							lex_item.tok = PLUS_EQUALS;
							lex_item.lexeme = "+=";
							return lex_item;
						} else {
							current_state = START;
							//"Put back" the char
							fseek(fl, -1, SEEK_CUR);
							lex_item.tok = PLUS;
							lex_item.lexeme = "+";
							return lex_item;
						}

					case '-':
						ch2 = fgetc(fl);
						
						//If we get this then it's +=
						if(ch2 == '='){
							current_state = START;
							//Prepare and return
							lex_item.tok = MINUS_EQUALS;
							lex_item.lexeme = "-=";
							return lex_item;
						//We can also have an arrow
						} else if(ch2 == '>'){
							current_state = START;
							//Prepare and return
							lex_item.tok = ARROW;
							lex_item.lexeme = "->";
							return lex_item;
						} else {
							current_state = START;
							//"Put back" the char
							fseek(fl, -1, SEEK_CUR);
							lex_item.tok = MINUS;
							lex_item.lexeme = "-";
							return lex_item;
						}

					case '*':
						ch2 = fgetc(fl);
						
						//If we get this then it's +=
						if(ch2 == '='){
							current_state = START;
							//Prepare and return
							lex_item.tok = TIMES_EQUALS;
							lex_item.lexeme = "*=";
							return lex_item;
						} else {
							current_state = START;
							//"Put back" the char
							fseek(fl, -1, SEEK_CUR);
							lex_item.tok = STAR;
							lex_item.lexeme = "*";
							return lex_item;
						}

					case '=':
						ch2 = fgetc(fl);
						
						//If we get this then it's +=
						if(ch2 == '='){
							current_state = START;
							//Prepare and return
							lex_item.tok = D_EQUALS;
							lex_item.lexeme = "==";
							return lex_item;
						} else {
							current_state = START;
							//"Put back" the char
							fseek(fl, -1, SEEK_CUR);
							lex_item.tok = EQUALS;
							lex_item.lexeme = "=";
							return lex_item;
						}

					case '&':
						ch2 = fgetc(fl);
						
						//If we get this then it's +=
						if(ch2 == '&'){
							current_state = START;
							//Prepare and return
							lex_item.tok = D_AND;
							lex_item.lexeme = "&&";
							return lex_item;
						} else {
							current_state = START;
							//"Put back" the char
							fseek(fl, -1, SEEK_CUR);
							lex_item.tok = S_AND;
							lex_item.lexeme = "&";
							return lex_item;
						}

					case '|':
						ch2 = fgetc(fl);
						
						//If we get this then it's +=
						if(ch2 == '|'){
							current_state = START;
							//Prepare and return
							lex_item.tok = D_OR;
							lex_item.lexeme = "||";
							return lex_item;
						} else {
							current_state = START;
							//"Put back" the char
							fseek(fl, -1, SEEK_CUR);
							lex_item.tok = S_OR;
							lex_item.lexeme = "|";
							return lex_item;
						}

					case ';':
						lex_item.tok = SEMICOLON;
						lex_item.lexeme = ";";
						return lex_item;

					case ':':
						lex_item.tok = SEMICOLON;
						lex_item.lexeme = ";";
						return lex_item;

					case '(':
						lex_item.tok = L_PAREN;
						lex_item.lexeme = "(";
						return lex_item;

					case ')':
						lex_item.tok = R_PAREN;
						lex_item.lexeme = ")";
						return lex_item;

					case '{':
						lex_item.tok = L_CURLY;
						lex_item.lexeme = "{";
						return lex_item;

					case '}':
						lex_item.tok = R_CURLY;
						lex_item.lexeme = "}";
						return lex_item;


					


				}

				break;

			case IN_IDENT:
				break;

			case IN_INT:
				break;

			case IN_FLOAT:
				break;

			case IN_STRING:
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

				break;
		}
	}

	//Return this token
	if(ch == EOF){
		lex_item.tok = DONE;
		lex_item.lexeme = "EOF";
	}

	return lex_item;
}

