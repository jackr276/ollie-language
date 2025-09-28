/**
 * Expose all needed lexer APIs to anywhere else that needs it
*/

//Include guards
#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>
#include <sys/types.h>
#include "../utils/dynamic_string/dynamic_string.h"
#include "../utils/token.h"

//The maximum token length is 500 
#define MAX_TOKEN_LENGTH 500
#define MAX_IDENT_LENGTH 200

//Are we hunting for a constant?
typedef enum {
	SEARCHING_FOR_CONSTANT,
	NOT_SEARCHING_FOR_CONSTANT
} const_search_t;


//The lexitem_t struct
typedef struct lexitem_t lexitem_t;


struct lexitem_t{
	//The string(lexeme) that got us this token
	dynamic_string_t lexeme;
	//The line number of the source that we found it on
	u_int16_t line_num;
	//The token associated with this item
	ollie_token_t tok;
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
 * Initialize the lexer by dynamically allocating the lexstack
 * and any other needed data structures
 */
void initialize_lexer();

/**
 * Deinitialize the entire lexer
 */
void deinitialize_lexer();

/**
 * A utility function for error printing that converts an operator to a string
 */
char* operator_to_string(ollie_token_t op);

#endif /* LEXER_H */
