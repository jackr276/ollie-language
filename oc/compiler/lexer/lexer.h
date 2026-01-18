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

//The lexitem_t struct
typedef struct lexitem_t lexitem_t;
//The overall token stream value
typedef struct ollie_token_stream_t ollie_token_stream_t;

struct lexitem_t {
	//The string(lexeme) that got us this token
	dynamic_string_t lexeme;
	//The line number of the source that we found it on
	u_int16_t line_num;
	//The token associated with this item
	ollie_token_t tok;
};

struct ollie_token_stream_t {
	//Array of tokens
	lexitem_t* token_stream;
	//Current token index
	u_int32_t current_token_index;
	//Max index, needed to know when we resize
	u_int32_t max_token_index;
} ;


//======================== Public utility macros ========================
/**
 * Reset the file pointer to go back to the start
 */
#define RESET_FILE(fl) fseek(fl, 0, SEEK_SET)

/**
 * Get the current file pointer position
 */
#define GET_CURRENT_FILE_POSITION(fl) ftell(fl)

//======================== Public utility macros ========================

/**
 * Convert a token into a string for error printing purposes
 */
char* token_to_string(ollie_token_t token);

/**
 * Reconsume the tokens starting from a given seek
 */
void reconsume_tokens(FILE* fl, int64_t reconsume_start);

/**
 * Special case -- hunting for assembly statements
 */
lexitem_t get_next_assembly_statement(FILE* fl);

/**
 * Generic token grabbing function
 */
lexitem_t get_next_token(FILE* fl, u_int32_t* parser_line_num);

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
ollie_token_stream_t intialize_token_stream();

/**
 * Deallocate the entire token string
 */
void destroy_token_stream(ollie_token_stream_t* stream);


#endif /* LEXER_H */
