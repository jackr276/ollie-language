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
//Our stack for storing variables, etc
stack_t* variable_stack;
stack_t* grouping_stack;
//The number of errors
u_int16_t num_errors = 0;
//The current parser line number
u_int16_t parser_line_num = 0;



/**
 * Simply prints a parse message in a nice formatted way
*/
void print_parse_message(parse_message_t* parse_message){
	//Mapped by index to the enum values
	char* type[] = {"WARNING", "ERROR", "INFO"};

	//Print this out on a single line
	printf("[LINE %d: PARSER %s]: %s\n", parse_message->line_num, type[parse_message->message], parse_message->info);
}


/**
 * Do we have an identifier or not?
 */
static u_int8_t identifier(FILE* fl){
	//Grab the next token
	Lexer_item l = get_next_token(fl, &parser_line_num);
	parse_message_t message;
	char info[500];
	
	//If we can't find it that's bad
	if(l.tok != IDENT){
		message.message = PARSE_ERROR;
		sprintf(info, "String %s is not a valid identifier", l.lexeme);
		message.line_num = l.line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}

	//We'll push this ident onto the stack and let whoever called(function/variable etc.) deal with it
	//We have no need to search the symtable in this function because we are unable to context-sensitive
	//analysis here
	push(variable_stack, l);
	return 1;
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


u_int8_t direct_declarator(FILE* fl){
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


u_int8_t enumerator(FILE* fl){

}



/**
 * Helper to maintain RL(1) properties. Remember, by the time we've gotten here, we've already seen a COMMA
 *
 * BNF Rule: <enumerator-list-prime> ::= ,<enumerator><enumerator-list-prime>
 */
u_int8_t enumeration_list_prime(FILE* fl){
	Lexer_item l;
	parse_message_t message;
	u_int8_t status = 0;

	//We now need to see a valid enumerator
	status = enumerator(fl);

	//Get out if bad
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid enumerator in enumeration list";
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}

	//Now if we see a comma, we know that we have an enumerator-list-prime
	l = get_next_token(fl, &parser_line_num);

	//If we see a comma, we'll use the helper
	if(l.tok == COMMA){
		return enumeration_list_prime(fl);
	} else {
		//Put it back and get out if no
		push_back_token(fl, l);
		return 1;
	}
}


/**
 * An enumeration list guarantees that we have at least one enumerator
 *
 * BNF Rule: <enumerator-list> ::= <enumerator><enumerator-list-prime>
 */
u_int8_t enumeration_list(FILE* fl){
	Lexer_item l;
	parse_message_t message;
	u_int8_t status = 0;

	//We need to see a valid enumerator
	status = enumerator(fl);

	//Get out if bad
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid enumerator in enumeration list";
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}

	//Now if we see a comma, we know that we have an enumerator-list-prime
	l = get_next_token(fl, &parser_line_num);

	//If we see a comma, we'll use the helper
	if(l.tok == COMMA){
		return enumeration_list_prime(fl);
	} else {
		//Put it back and get out if no
		push_back_token(fl, l);
		return 1;
	}
}


/**
 * An enumeration specifier will always start with enumerated
 * REMEMBER: Due to RL(1), by the time we get here ENUMERATED has already been seen
 *
 * BNF Rule: <enumator-specifier> ::= enumerated <identifier> { <enumerator-list> } 
 * 						  			| enumerated <identifier>
 *
 * TODO SYMTAB
 * 						  			
 */
u_int8_t enumeration_specifier(FILE* fl){
	Lexer_item l;
	parse_message_t message;


	//We now have to see a valid identifier, since we've already seen the ENUMERATED keyword
	u_int8_t status = identifier(fl);

	//If it's bad then we're done here
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid identifier in enumeration specifier";
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}
	
	//Following this, if we see a right curly brace, we know that we have a list
	l = get_next_token(fl, &parser_line_num);

	if(l.tok == L_CURLY){
		//Push onto the grouping stack for matching
		push(grouping_stack, l);

		//We now must see a valid enumeration list
		status = enumeration_list(fl);

		//If it's bad then we're done here
		if(status == 0){
			message.message = PARSE_ERROR;
			message.info = "Invalid enumeration list in enumeration specifier";
			message.line_num = parser_line_num;
			print_parse_message(&message);
			num_errors++;
			return 0;
		}

		l = get_next_token(fl, &parser_line_num);

		//All of our fail cases here
		if(l.tok != R_CURLY){
			message.message = PARSE_ERROR;
			message.info = "Right curly brace expected at end of enumeration list";
			message.line_num = parser_line_num;
			print_parse_message(&message);
			num_errors++;
			return 0;
		}

		//Unmatched left curly
		if(pop(grouping_stack).tok != L_CURLY){
			message.message = PARSE_ERROR;
			message.info = "Unmatched right parenthesis";
			message.line_num = parser_line_num;
			print_parse_message(&message);
			num_errors++;
			return 0;
		}

		//Otherwise we should be fine when we get here, so we can return
		return 0;
		
	} else {
		//Otherwise, push it back and let someone else handle it
		push_back_token(fl, l);
		return 1;
	}
}


/**
 * Type specifiers can be the set of primitives or user defined types
 * TODO SYMTAB stuff
 *
 * BNF Rule:  <type-specifier> ::= void
 * 								 | u_int8
 * 								 | s_int8
 * 								 | u_int16
 * 								 | s_int16
 * 								 | u_int32
 * 								 | s_int32
 * 								 | u_int64
 * 								 | s_int64
 * 								 | float32
 * 								 | float64
 * 								 | char
 * 								 | str
 * 								 | <enumeration-specifier>
 * 								 | <structure-specifier>
 * 								 | <user-defined-type>
 */
u_int8_t type_specifier(FILE* fl){
	parse_message_t message;
	//Grab the next token
	Lexer_item l = get_next_token(fl, &parser_line_num);
	u_int8_t status = 0;

	//In the case that we have one of the primitive types
	if(l.tok == VOID || l.tok == U_INT8 || l.tok == S_INT8 || l.tok == U_INT16 || l.tok == S_INT16
	  || l.tok == U_INT32 || l.tok == S_INT32 || l.tok == U_INT64 || l.tok == S_INT64 || l.tok == FLOAT32
	  || l.tok == FLOAT64 || l.tok == CHAR || l.tok == STR){
		//TODO put in symtable
		return 1;
	}

	//Otherwise, we still have some options here
	//If we see enumerated, we know it's an enumerated type
	if(l.tok == ENUMERATED){
		status = enumeration_specifier(fl);

		//If it's bad then we're done here
		if(status == 0){
			message.message = PARSE_ERROR;
			message.info = "Invalid enumeration specifier in type specifier";
			message.line_num = parser_line_num;
			print_parse_message(&message);
			num_errors++;
			return 0;
		}	

		return 1;
	}


	return 0;
}


/**
 * We can see several different storage class specifiers
 * 
 * BNF Rule: <storage-class-specifier> ::= static 
 * 								 		| external
 * 								 		| register
 */
u_int8_t storage_class_specifier(FILE* fl){
	Lexer_item l = get_next_token(fl, &parser_line_num);

	//If we see one here
	if(l.tok == STATIC || l.tok == EXTERNAL | l.tok == REGISTER){
		//TODO store in symtab
		return 1;
	} else {
		//Otherwise, put the token back and get out
		push_back_token(fl, l);
		return 0;
	}
}


/**
 * For a parameter declaration, we can see this items in order
 *
 * BNF Rule: <parameter-declaration> ::= (<storage-class-specifier>)? (constant)? <type-specifier> <direct-declarator>
 */
u_int8_t parameter_declaration(FILE* fl){
	Lexer_item lookahead;
	parse_message_t message;
	u_int8_t is_const = 0;
	u_int8_t status = 0;

	//We can optionally see a storage_class_specifier here
	storage_class_specifier(fl);

	//Now we can optionally see const here
	lookahead = get_next_token(fl, &parser_line_num);
	
	//We'll flag this for now TODO must go in symtab
	if(lookahead.tok == CONSTANT){
		is_const = 1;
	} else {
		//Put it back and move on
		push_back_token(fl, lookahead);
	}
	
	//Now we must see a valid type specifier
	status = type_specifier(fl);
	
	//If it's bad then we're done here
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid type specifier found in parameter declaration";
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}

	//Finally, we must see a direct declarator that is valid
	status = direct_declarator(fl);


	return 0;
}


/**
 * Optional repetition allowed with our parameter list
 *
 * BNF Rule: <parameter-list-prime> ::= , <parameter-declaration><parameter-list-prime>
 * 										| epsilon
 */
u_int8_t parameter_list_prime(FILE* fl){
	Lexer_item lookahead;
	u_int8_t status;
	parse_message_t message;

	//Grab the next token
	lookahead = get_next_token(fl, &parser_line_num);
	
	//If we see a comma, we know that this is the recursive step
	if(lookahead.tok == COMMA){
		//We need to now see a valid parameter declaration
		status = parameter_declaration(fl);
		
		//If it went wrong
		if(status == 0){
			message.message = PARSE_ERROR;
			message.info = "Invalid parameter declaration in parameter list";
			message.line_num = parser_line_num;
			print_parse_message(&message);
			num_errors++;
			return 0;
		}
		
		//Now we will take our recursive step in seeing a parameter list prime
		return parameter_list_prime(fl);
	}

	//This means that we had an epsilon here, so we'll put the token back and leave
	push_back_token(fl, lookahead);
	return 1;
}


/**
 * BNF Rule: <parameter-list> ::= <parameter-declaration>(<parameter-list-prime>)?
 */
u_int8_t parameter_list(FILE* fl){
	u_int8_t status;
	parse_message_t message;

	//First, we must see a valid parameter declaration
	status = parameter_declaration(fl);
	
	//If we didn't see a valid one
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "Invalid parameter declaration in parameter list";
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}

	//Now we can see our parameter list
	return parameter_list_prime(fl);
}


/**
 * A function specifier can be either a STATIC or an EXTERNAL definition
 *
 * BNF rule: <function-specifier> ::= static 
 *			        			  | external
 */
u_int8_t function_specifier(FILE* fl){
	//Grab the next token
	Lexer_item l = get_next_token(fl, &parser_line_num);
	parse_message_t message;
	
	//If we find one of these, push it to the stack and return 1
	if(l.tok == STATIC || l.tok == EXTERNAL){
		//Push to the stack and let the caller decide how to handle
		push(variable_stack, l);
		return 1;
	}
	
	//Otherwise we have something bad here
	message.message = PARSE_ERROR;
	message.info = "Invalid function specifier";
	num_errors++;

	return 0;
}


/**
 * Handle the case where we declare a function
 *
 * BNF Rule: <function-definition> ::= func (<function-specifier>)? <identifier> (<parameter-list>?) -> <type-specifier> <compound-statement>
 */
u_int8_t function_declaration(FILE* fl){
	Lexer_item lookahead;
	Lexer_item ident;
	parse_message_t message;
	//We may need this in later iterations
	Token function_storage_type = BLANK;
	u_int8_t status;
	//This will be used for error printing
	char info[500];
	
	//REMEMBER: by the time we get here, we've already seen and consumed "FUNC"
	lookahead = get_next_token(fl, &parser_line_num);
	
	//We've seen the option function specifier
	if(lookahead.tok == COLON){
		status = function_specifier(fl);

		//If this happened then we have an issue
		if(status == 0){
			message.message = PARSE_ERROR;
			message.info = "Invalid function specifier";
			message.line_num = parser_line_num;
			print_parse_message(&message);
			num_errors++;
			return 0;
		}

		//Otherwise we can grab out what we have here
		function_storage_type = pop(variable_stack).tok;

	} else {
		//Otherwise put the token back in the stream
		push_back_token(fl, lookahead);
	}
	
	//Now we must see an identifer
	status = identifier(fl);
	
	//We have no identifier, so we must quit
	if(status == 0){
		message.message = PARSE_ERROR;
		message.info = "No valid identifier found";
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}

	//At this point we'll need to store in symtable
	//Pop this off the stack
	ident = pop(variable_stack);
	//Let's see if this already exists
	symtab_record_t* record = lookup(symtab, ident.lexeme);

	//This means that we were trying to redefine a function
	if(record != NULL){
		message.message = PARSE_ERROR;
		memset(info, 0, 500*sizeof(char));
		sprintf(info, "Function %s has already been defined on line %d", record->name, record->line_number);
		message.info = info;
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}
	
	//Now we need to see a valid parentheis
	lookahead = get_next_token(fl, &parser_line_num);

	//If we didn't find it, no point in going further
	if(lookahead.tok != L_PAREN){
		message.message = PARSE_ERROR;
		message.info = "Left parenthesis expected";
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}

	//SPECIAL CASE -- we could have a blank parameter list, in which case we're done
	lookahead = get_next_token(fl, &parser_line_num);
	
	if(lookahead.tok == R_PAREN){
		//TODO insert blank parameter list
		goto arrow_ident;
	}

	//Otherwise we'll push this onto the grouping stack to check later
	push(grouping_stack, lookahead);
	
	//We'll deal with our storage of the function's name later, but at least now we know it's unique
	//We must now see a valid parameter list
	status = parameter_list(fl);

	//We have a bad parameter list
	if(status == 0){
		message.message = PARSE_ERROR;
		memset(info, 0, 500*sizeof(char));
		sprintf(info, "No valid paramter list found for function \"%s\"", ident.lexeme);
		message.info = info;
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}
	
	//Now we need to see a valid closing parenthesis
	lookahead = get_next_token(fl, &parser_line_num);

	//If we don't have an R_Paren that's an issue
	if(lookahead.tok != R_PAREN){
		message.message = PARSE_ERROR;
		message.info = "Right parenthesis expected";
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
		return 0;
	}
	
	//If this happens, then we have some unmatched parenthesis
	if(pop(grouping_stack).tok != L_PAREN){
		message.message = PARSE_ERROR;
		message.info = "Unmatched opening parenthesis found";
		message.line_num = parser_line_num;
		print_parse_message(&message);

		num_errors++;
		return 0;
	}


	//Past the point where we've seen the param_list
arrow_ident:
	//Grab the next one
	lookahead = get_next_token(fl, &parser_line_num);

	//We absolutely must see an arrow here
	if(lookahead.tok != ARROW){
		message.message = PARSE_ERROR;
		message.info = "Arrow expected after function declaration";
		message.line_num = parser_line_num;
		print_parse_message(&message);
		num_errors++;
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

	lookahead = get_next_token(fl, &parser_line_num);

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
		message.line_num = parser_line_num;
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
	while((l = get_next_token(fl, &parser_line_num)).tok != DONE){
		//Put the token back
		push_back_token(fl, l);

		//Pass along and let the rest handle
		status = declaration_partition(fl);
		
		//If we have an error then we'll print it out
		if(status == 0){
			message.message = PARSE_ERROR;
			message.info = "Program rule encountered an error from declaration partition";
			message.line_num = parser_line_num;
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
	variable_stack = create_stack();
	grouping_stack = create_stack();

	//Global entry/run point
	status = program(fl);

	//If we failed
	if(status == 0){
		char info[500];
		sprintf(info, "Parsing failed with %d errors", num_errors);
		printf("\n\n=======================================================================\n");
		printf("%s\n", info);
		printf("=======================================================================\n\n");
	}
	
	//Clean these both up for memory safety
	destroy_stack(variable_stack);
	destroy_stack(grouping_stack);
	destroy_symtab(symtab);
	
	return status;
}


