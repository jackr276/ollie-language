/**
 * Author: Jack Robbins
 *
 * The ollie language preprocessor handles imports(TODO) and anything related to
 * Ollie macro statements. It is guaranteed to run *before* the parser, and will manipulate
 * the token stream itself
*/

//Include guards
#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

//Really all that is needed here is the lexer
#include "../lexer/lexer.h"
#include <sys/types.h>

//The generic result struct for the preprocessor
typedef struct preprocessor_results_t preprocessor_results_t;

/**
 * Hold onto some info from the preprocessor like the status,
 * token stream, and macros processed
 */
struct preprocessor_results_t {
	//The token stream
	ollie_token_stream_t* stream;
	//The number of errors
	u_int32_t error_count;
	//The number of warnings
	u_int32_t warning_count;
	//The number of macros processed(more of novelty info but it's fine to have)
	u_int32_t macros_processed;
	//Did this work or not?
	u_int8_t success;
};


/**
 * Entry point to the entire preprocessor is here. The preprocessor
 * will traverse the token stream and make replacements as it sees
 * fit with defined macros
 */
preprocessor_results_t preprocess(char* file_name, ollie_token_stream_t* stream);

#endif /* PREPROCESSOR_H */
