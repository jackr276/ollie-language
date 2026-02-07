/**
 * Parser API file. Exposes all needed external functions used by the compiler
*/

//Include guards
#ifndef PARSER_H
#define PARSER_H

#include "../utils/stack/heapstack.h"
#include "../utils/stack/lexstack.h"
#include "../symtab/symtab.h"
#include "../lexer/lexer.h"
#include "../type_system/type_system.h"
#include "../ast/ast.h"
#include "../utils/utility_structs.h"
#include "../utils/error_management.h"
#include <sys/types.h>

typedef struct front_end_results_package_t front_end_results_package_t;

/**
 * A struct that specifically returns the results of the compiler front-end
 */
struct front_end_results_package_t{
	//The root of the AST
	generic_ast_node_t* root;
	//The function, variable and type symtabs
	function_symtab_t* function_symtab;
	variable_symtab_t* variable_symtab;
	type_symtab_t* type_symtab;
	//Grouping stack
	lex_stack_t grouping_stack;
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
void print_parse_message(error_message_type_t message_type, char* info, u_int32_t line_num);

/**
 * Parse the entirety of the file. Returns 0 if successful
 *
 * NOTE: the parser will destroy the given token stream once done
 */
front_end_results_package_t* parse(compiler_options_t* options);

#endif /* PARSER_H */
