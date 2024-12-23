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

	char ch;

	while((ch = fgetc(fl)) != EOF){
		if(is_ws(ch) == 1){
			continue;
		} else {
			printf("%c\n", ch);
		}
	}

	return lex_item;
}


/**
 * May or may not use this
*/
void print_lexer_item(){

}
