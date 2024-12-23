/**
 * Lexical analyzer and tokenizer for Ollie-language
*/

#include "lexer.h"
#include <stdio.h>
#include <sys/types.h>


/**
 * Helper that will determine if we have whitespace(ws) 
 */
static u_int8_t is_ws(char ch){
	return ch == ' ' || ch == '\n' || ch == '\t';
}



Lexer_item identifier_or_keyword(const char* lexeme, int line_number){
	Lexer_item lex_item;

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
					char ch2 = fgetc(fl);	
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


/**
 * May or may not use this
*/
void print_lexer_item(){

}
