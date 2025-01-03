/**
 * The parser for Ollie-Lang
 *
 * GOAL: The goal of the parser is to determine if the input program is a syntatically valid sentence in the language.
 * This is done via recursive-descent in our case. As the 
*/

#include "parser.h"
#include <stdio.h>
#include <sys/types.h>

//Our global symbol table
symtab_t* symtab;
//Our global stack
stack_t* stack;
//The number of errors
u_int8_t num_errors = 0;


/**
 * Simply prints a parse message in a nice formatted way
*/
void print_parse_message(parse_message_t* parse_message){
	//Mapped by index to the enum values
	char* type[] = {"WARNING", "ERROR", "INFO"};

	//Print this out on a single line
	printf("[LINE %d: PARSER %s]: %s\n", parse_message->line_num, type[parse_message->message], parse_message->info);
}


static u_int8_t identifier(FILE* fl){
	return 0;
}

static u_int8_t declaration(FILE* fl){
	return 0;
}

static u_int8_t shift_expression(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
}

static u_int8_t relational_expression(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
}


static u_int8_t equality_expression(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
}

static u_int8_t and_expression(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
}

static u_int8_t exclusive_or_expression(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
}


static u_int8_t inclusive_or_expression(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
}


u_int8_t logical_and_expression(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
}


u_int8_t logical_or_expression(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
}


u_int8_t conditional_expression(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
}


u_int8_t constant_expression(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
}


u_int8_t direct_declarator(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
}

u_int8_t declarator(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
}


u_int8_t structure_declarator(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
}

u_int8_t structure_declarator_list(FILE* fl, symtab_t* symtab, stack_t* stack){
	return 0;
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


/** 
 * What storage specifier do we have?
 *
 * BNF Rule: <storage-specifier> ::= static
 * 						 | external 
 * 						 | register 
 * 						 | defined
 */
u_int8_t storage_specifier(FILE* fl){
	//Grab the next token
	Lexer_item l = get_next_token(fl);
	
	//If we find one of these, push it to the stack and return 1
	if(l.tok == STATIC || l.tok == EXTERNAL || l.tok == REGISTER || l.tok == DEFINED){
		push(stack, l);
		return 1;
	}

	//Otherwise, we didn't find one
	//Whatever we found, put it back
	push_back_token(fl, l);
	return 0;
}


/**
 * A function specifier can be either a STATIC or an EXTERNAL definition
 *
 * BNF rule: <function-specifier> ::= static 
 *			        			  | external
 */
u_int8_t function_specifier(FILE* fl){
	//Grab the next token
	Lexer_item l = get_next_token(fl);
	parse_message_t message;
	
	//If we find one of these, push it to the stack and return 1
	if(l.tok == STATIC || l.tok == EXTERNAL){
		push(stack, l);
		return 1;
	}
	
	//Otherwise we have something bad here
	message.message = PARSE_ERROR;
	message.info = "Invalid function specifier";
	

	return 0;
}


/**
 * Handle the case where we declare a function
 *
 * BNF Rule: <function-definition> ::= func (<function-specifier>)? <identifier> <parameter-list> -> <type-specifier> <compound-statement>
 */
u_int8_t function_declaration(FILE* fl){
	Lexer_item lookahead;
	parse_message_t message;
	//We may need this in later iterations
	Token function_storage_type = BLANK;
	u_int8_t status;
	
	//REMEMBER: by the time we get here, we've already seen and consumed "FUNC"
	lookahead = get_next_token(fl);
	
	//We've seen the option function specifier
	if(lookahead.tok == COLON){
		status = function_specifier(fl);

		//If this happened then we have an issue
		if(status == 0){
			message.message = PARSE_ERROR;
			message.info = "Invalid function specifier";
			message.line_num = lookahead.line_num;
			print_parse_message(&message);
			return 0;
		}
	} else {
		//Otherwise put the token back in the stream
		push_back_token(fl, lookahead);
	}
	
	//Now we must see an identifer
	status = identifier(fl);
	
	//At this point we'll need to store in symtable

	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "No valid identifier found";
		message.line_num = lookahead.line_num;
		print_parse_message(&message);
		return 0;
	}

	
	

	return 0;
}


/**
 * Here we can either have a function definition or a declaration
 *
 * <declaration-partition>::= <function-definition>
 *                        	| <declaration>
 */
u_int8_t declaration_partition(FILE* fl){
	Lexer_item lookahead;
	u_int8_t status;
	parse_message_t message;

	lookahead = get_next_token(fl);

	//We know that we have a function here
	if(lookahead.tok == FUNC){
		status = function_declaration(fl);
	} else {
		//Push it back
		push_back_token(fl, lookahead);
		//Otherwise, the only other option is a declaration
		status = declaration(fl);
	}
	
	//Something failed
	if(status == 0){
		//Otherwise we've failed completely
		message.message = PARSE_ERROR;
		message.info = "Declaration Partition could not find a valid function or declaration";
		message.line_num = lookahead.line_num;
		print_parse_message(&message);

		num_errors++;
		return 0;
	}
	
	//If we get here it worked
	return 1;
}


/**
 * Here is our entry point
 *
 * BNF Rule: <program>::= {<declaration-partition>}*
 */
u_int8_t program(FILE* fl){
	Lexer_item l;
	u_int8_t status = 0;
	parse_message_t message;

	//As long as we aren't done
	while((l = get_next_token(fl)).tok != DONE){
		//Put the token back
		push_back_token(fl, l);

		//Pass along and let the rest handle
		status = declaration_partition(fl);
		
		//If we have an error then we'll print it out
		if(status == 0){
			message.message = PARSE_ERROR;
			message.info = "Program rule encountered an error from declaration partition";
			message.line_num = l.line_num;
			print_parse_message(&message);
			num_errors++;
			//If we have but one failure, the whole thing is toast
			break;
		}
	}

	return status;
}


/**
 * Entry point for our parser. Everything beyond this point will be called in a recursive-descent fashion through
 * static methods
*/
u_int8_t parse(FILE* fl){
	u_int8_t status = 0;
	num_errors = 0;
	parse_message_t message;

	//Initialize our global symtab here
	symtab = initialize_symtab();
	//Also create a stack for our matching uses(curlies, parens, etc.)
	stack = create_stack();

	status = program(fl);

	//If we failed
	if(status == 0){
		message.message = PARSE_ERROR;
		char info[500];
		sprintf(info, "Parsing failed with %d errors", num_errors);
		message.info = info;
		print_parse_message(&message);
	}
	
	//Clean these both up for memory safety
	destroy_stack(stack);
	destroy_symtab(symtab);
	
	return status;
}


