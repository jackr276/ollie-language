/**
 * Author: Jack Robbins
 *
 * The implementation file for the ollie preprocessor
*/

#include "preprocessor.h"
#include <sys/types.h>

/**
 * A generic printer for any preprocessor errors that we may encounter
 */
static inline void print_preprocessor_error(char* info, u_int32_t line_number){

}



/**
 * Entry point to the entire preprocessor is here. The preprocessor
 * will traverse the token stream and make replacements as it sees
 * fit with defined macros
 */
preprocessor_results_t preprocess(ollie_token_stream_t* stream){
	//Store the preprocessor results
	preprocessor_results_t results;

	//The stream is always the original stream
	results.stream = stream;


	//Give the results back
	return results;
}
