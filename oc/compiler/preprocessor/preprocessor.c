/**
 * Author: Jack Robbins
 *
 * The implementation file for the ollie preprocessor
*/

#include "preprocessor.h"
#include "../utils/error_management.h"
#include <sys/types.h>

//What is the name of the file that we are preprocessing
static char* current_file_name;

//Define a basic struct for macro storage
typedef struct ollie_macro_t ollie_macro_t;

struct ollie_macro_t {
	//TODO

};


/**
 * A generic printer for any preprocessor errors that we may encounter
 */
static inline void print_preprocessor_message(error_message_type_t message, char* info, u_int32_t line_number){
	//Now print it
	const char* type[] = {"WARNING", "ERROR", "INFO", "DEBUG"};

	//Print this out on a single line
	fprintf(stdout, "\n[FILE: %s] --> [LINE %d | OLLIE PREPROCESSOR %s]: %s\n", current_file_name, line_number, type[message], info);
}



/**
 * Entry point to the entire preprocessor is here. The preprocessor
 * will traverse the token stream and make replacements as it sees
 * fit with defined macros
 */
preprocessor_results_t preprocess(char* file_name, ollie_token_stream_t* stream){
	//Store the preprocessor results
	preprocessor_results_t results;

	//The stream is always the original stream
	results.stream = stream;

	//Store the file name up top globally
	current_file_name = file_name;

	//Give the results back
	return results;
}
