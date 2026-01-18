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

/**
 * Tokenization status
 */
typedef enum {
	STREAM_STATUS_FAILURE,
	STREAM_STATUS_SUCCESS
} token_stream_status_t;


struct lexitem_t {
	//The string(lexeme) that got us this token
	dynamic_string_t lexeme;
	//The line number of the source that we found it on
	u_int32_t line_num;
	//The token associated with this item
	ollie_token_t tok;
};


struct ollie_token_stream_t {
	//Array of tokens
	lexitem_t* token_stream;
	//Current token index
	u_int32_t current_token_index;
	//This is the value that we're looking
	//at from the parser perspective
	u_int32_t token_pointer;
	//Max index, needed to know when we resize
	u_int32_t max_token_index;
	//Let the caller know if this worked or not
	token_stream_status_t status;
};


/**
 * Tokenzie an entire file and return a token
 * stream item that the parser can use as it wishes
 *
 * The tokenizer assumes that the fl file pointer
 * is 100% valid
 */
ollie_token_stream_t tokenize(FILE* fl, char* current_file_name);

/**
 * Deallocate the entire token stream
 */
void destroy_token_stream(ollie_token_stream_t* stream);

/**
 * Convert a token into a string for error printing purposes
 */
char* lexitem_to_string(lexitem_t* lexitem);

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

#endif /* LEXER_H */
