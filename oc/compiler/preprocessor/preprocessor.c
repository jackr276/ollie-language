/**
 * Author: Jack Robbins
 *
 * The implementation file for the ollie preprocessor
 *
 * The ollie preproccessor will take two passes over the entire token stream.
 * The first pass will be a consumption pass, where we will read in all of the macros
 * that have been defined. The second pass will be our substitution pass, where all of these macros will be replaced
 * in the file. It should be noted that this will be a destructive process, meaning that we will flag the tokens that
 * were consumed as part of the macro to be ignored by the parser. This avoids any confusion that we may have
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
	
	//What line number was this macro defined on
	u_int32_t line_number;
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
 * Put simply, the consumption pass will run through the entire token
 * stream looking for macros. When it finds a macro, it will flag that section
 * of the token stream to be ignored by future passes(in reality this means
 * it will be cut out completely) and will store the macro token snippet
 * inside of a struct for later use. The consumption pass does not have anything
 * to do with macro replacement. This will come after in the replacement
 * pass
 */
static void macro_consumption_pass(ollie_token_stream_t* stream){

}


/**
 * The macro replacement pass will produce an entirely new token stream in which all of our replacements have been
 * made. This is done to avoid the inefficiencies of inserting tokens into the original dynamic array over
 * and over again which causes a need to shift everything to the right by one each time
 */
static ollie_token_stream_t* macro_replacement_pass(ollie_token_stream_t* stream){

	//TODO TOTAL DUMMY - do not mistake
	return stream;
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
