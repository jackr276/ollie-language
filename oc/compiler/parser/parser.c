/**
 * The parser for Ollie-Lang
 *
 * GOAL: The goal of the parser is to determine if the input program is a syntatically valid sentence in the language.
 * This is done via recursive-descent in our case. As the 
*/

#include "parser.h"
#include <stdio.h>
#include <sys/types.h>




u_int8_t structure_declarator_list(FILE* fl, symtab_t* symtab, stack_t* stack){

}


u_int8_t specifier_qualifier(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
}


u_int8_t structure_declaration(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
}

u_int8_t structure_specifier(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
}

u_int8_t type(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
}

u_int8_t pointer(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
}

u_int8_t type_specifier(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
}

u_int8_t storage_specifier(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;

}

u_int8_t function_defintion(FILE* fl, symtab_t* symtab, stack_t* stack){

	return 0;
}

u_int8_t declaration_partition(FILE* fl, symtab_t* symtab, stack_t* stack){

	return 0;
}

u_int8_t program(FILE* fl, symtab_t* symtab, stack_t* stack){
	Lexer_item l;
	return 0;
}

/**
 * Entry point for our parser. Everything beyond this point will be called in a recursive-descent fashion through
 * static methods
*/
u_int8_t parse(FILE* fl){
	//Initialize our global symtab here
	symtab_t* symtab = initialize_symtab();
	//Also create a stack for our matching uses(curlies, parens, etc.)
	stack_t* stack = create_stack();


	program(fl, symtab, stack);
	
	
	//Clean these both up for memory safety
	destroy_stack(stack, STATES_ONLY);
	destroy_symtab(symtab);
	
	return 0;
}


/**
 * Simply prints a parse message in a nice formatted way
*/
void print_parse_message(parse_message_t* parse_message){
	//Mapped by index to the enum values
	char* type[] = {"WARNING", "ERROR", "INFO"};

	//Print this out on a single line
	printf("[PARSER %s]: %s\n", type[parse_message->message], parse_message->info);
}
