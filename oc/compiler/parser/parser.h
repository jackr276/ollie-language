/**
 * Parser API file. Exposes all needed external functions used by the compiler
*/

//Include guards
#ifndef PARSER_H
#define PARSER_H

#include "../stack/heapstack.h"
#include "../stack/lexstack.h"
#include "../symtab/symtab.h"
#include "../lexer/lexer.h"
#include "../type_system/type_system.h"
#include "../ast/ast.h"
#include "../call_graph/call_graph.h"
#include <sys/types.h>

typedef struct parse_message_t parse_message_t;
typedef struct front_end_results_package_t front_end_results_package_t;

/**
 * What type of message do we have
 */
typedef enum {
	WARNING=0,
	PARSE_ERROR=1,
	INFO=2,
} parse_message_type_t;

/**
 * What is the return status of the parser
 */
typedef enum{
	PARSER_RESULT_SUCCESS,
	PARSER_RESULT_FAILURE
} parser_result_type_t;


/**
 * A specific type of error that we can give back if needed
 */
struct parse_message_t{
	//What type is it	
	parse_message_type_t message;
	//Info message given
	char* info;
	//Is this a fatal error
	u_int8_t fatal;
	//The line number
	u_int16_t line_num;
};

/**
 * A struct that specifically returns the results of the compiler front-end
 */
struct front_end_results_package_t{
	//Success = 1, failure = 0
	parser_result_type_t result_type;
	//The root of the AST
	generic_ast_node_t* root;
	//The function, variable and type symtabs
	function_symtab_t* function_symtab;
	variable_symtab_t* variable_symtab;
	type_symtab_t* type_symtab;
	constants_symtab_t* constant_symtab;
	//Grouping stack
	lex_stack_t* grouping_stack;
	//Global call graph entry point
	call_graph_node_t* os;
	//Number of errors
	u_int16_t num_errors;
	//Number of warnings
	u_int16_t num_warnings;
	//The number of lines processed
	u_int32_t lines_processed;
	//Did we find a main function
	u_int8_t found_main_function;
};


/**
 * For printing formatted parser errors
 */
void print_parse_message(parse_message_type_t message_type, char* info, u_int16_t line_num);

/**
 * Parse the entirety of the file. Returns 0 if successful
 */
front_end_results_package_t parse(FILE* fl, char* file_name);

#endif /* PARSER_H */
