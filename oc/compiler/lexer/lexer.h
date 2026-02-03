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

//=================================== Public utility macros ==============================
/**
 * Get the index of the current stream seek head
 */
#define GET_CURRENT_TOKEN_INDEX(stream) stream->token_pointer

//=================================== Public utility macros ==============================

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
	//This union will hold all of the constant values
	//that a lexitem could possibly have
	union {
		double double_value;
		float float_value;
		u_int64_t unsigned_long_value;
		int64_t signed_long_value;
		u_int32_t unsigned_int_value;
		int32_t signed_int_value;
		u_int16_t unsigned_short_value;
		int16_t signed_short_value;
		u_int8_t unsigned_byte_value;
		int8_t signed_byte_value;
		char char_value;
	} constant_values;
	//The line number of the source that we found it on
	u_int32_t line_num;
	//The token associated with this item
	ollie_token_t tok;
	//Should this lexitem be ignored? This is mainly used
	//by the preprocessor during it's traversal of the token
	//streams
	u_int8_t ignore;
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
ollie_token_stream_t tokenize(char* current_file_name);

/**
 * Deallocate the entire token stream
 */
void destroy_token_stream(ollie_token_stream_t* stream);

/**
 * Generic token grabbing function
 */
lexitem_t get_next_token(ollie_token_stream_t* stream, u_int32_t* parser_line_number);

/**
 * Push a token back to the stream
 */
void push_back_token(ollie_token_stream_t* stream, u_int32_t* parser_line_number);

/**
 * Reset the stream to reconsume tokens from a given start point
 */
void reset_stream_to_given_index(ollie_token_stream_t* stream, u_int32_t reconsume_start);

/**
 * Convert a token into a string for error printing purposes
 */
char* lexitem_to_string(lexitem_t* lexitem);

/**
 * Convert specifically an operator token to a string for printing
 */
char* operator_token_to_string(ollie_token_t token);

#endif /* LEXER_H */
