/**
 * Expose all needed lexer APIs to anywhere else that needs it
*/

//Include guards
#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>
#include <sys/types.h>
#include "../utils/dynamic_string/dynamic_string.h"
#include "../utils/ollie_token_array/ollie_token_array.h"
#include "../utils/token.h"

//The maximum token length is 500 
#define MAX_TOKEN_LENGTH 500
#define MAX_IDENT_LENGTH 200

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


struct ollie_token_stream_t {
	//The token array
	ollie_token_array_t token_stream;
	//This is the value that we're looking
	//at from the parser perspective
	u_int32_t token_pointer;
	//Let the caller know if this worked or not
	token_stream_status_t status;
};


/**
 * For efficient searching in our build system, we provide a utility that will only grab
 * the first 2 tokens. This is because all module declarations are required to be at the
 * very top of the file, and we know that each module declaration itself is:
 *
 * $module module_name;
 *
 * So if we're only looking for module names, we only really need to look at the first 2 if we're
 * doing a quick search
 */
u_int8_t get_first_2_tokens(ollie_token_stream_t* stream, char* current_file_name, u_int8_t silent_mode);

/**
 * Tokenzie an entire file and return a token
 * stream item that the parser can use as it wishes
 *
 * If we are running in silent mode, tokenizer errors will not appear
 * on stdout
 */
ollie_token_stream_t tokenize(char* current_file_name, u_int8_t silent_mode);

/**
 * Allocate a token stream struct on the stack and return by copy
 */
ollie_token_stream_t token_stream_alloc();

/**
 * Reset the token stream back to its defaults
 */
void reset_token_stream(ollie_token_stream_t* stream);

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
 * Print an entire token array using the lexitem_to_string helper
 */
void print_token_array(ollie_token_array_t* array);

/**
 * Convert a token into a string for error printing purposes
 */
char* lexitem_to_string(lexitem_t* lexitem);

/**
 * Convert specifically an operator token to a string for printing
 */
char* operator_token_to_string(ollie_token_t token);

/**
 * Is the given token a constant or not
 */
u_int8_t is_constant_token(ollie_token_t token);

#endif /* LEXER_H */
