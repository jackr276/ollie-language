/**
 * The parser for Ollie-Lang
 *
 * GOAL: The goal of the parser is to determine if the input program is a syntatically valid sentence in the language.
 * This is done via recursive-descent in our case. As the 
 *
 * OVERALL STRUCTURE: The parser is the second thing that sees the source code. It only acts upon token streams that are given
 * to it from the lexer. The parser's goal is twofold. It will ensure that the structure of the program adheres to the rules of
 * the programming language, and it will translate the source code into an "Intermediate Representation(IR)" that can be given to 
 * the optimizer
 *
 * NEXT IN LINE: Control Flow Graph, OIR constructor, SSA form implementation
*/


#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "../stack/lexstack.h"
#include "../queue/heap_queue.h"

//For code clarity
#define SUCCESS 1
#define FAILURE 0

//All error sizes are 1000 unless specified
#define ERROR_SIZE 1500
#define LARGE_ERROR_SIZE 2000

//The function is reentrant
//Variable and function symbol tables
static function_symtab_t* function_symtab = NULL;
static variable_symtab_t* variable_symtab = NULL;
static type_symtab_t* type_symtab = NULL;
static constants_symtab_t* constant_symtab = NULL;

//The "operating system" function that is symbolically referenced here
static call_graph_node_t* os = NULL;
//The entire AST is rooted here
static generic_ast_node_t* prog = NULL;

//What is the current function that we are "in"
static symtab_function_record_t* current_function = NULL;
//What is the current variable that we are "in"
static symtab_variable_record_t* current_var = NULL;
//The queue that holds all of our jump statements for a given function
static heap_queue_t* current_function_jump_statements = NULL;
//What is the current switch statement type?
static generic_type_t* current_switch_statement_type = NULL;
//Have we found a default clause yet? -- reset for every switch statement
static u_int8_t found_default_clause = 0;

//Our stack for storing variables, etc
static lex_stack_t* grouping_stack = NULL;

//The number of errors
static u_int32_t num_errors;
//The number of warnings
static u_int32_t num_warnings;

//The current parser line number
static u_int16_t parser_line_num = 1;

//The overall node that holds all deferred statements for a function
generic_ast_node_t* deferred_stmts_node = NULL;

//Did we find a main function?
u_int8_t found_main_function = 0;

//The actual file token that we have, given on each parse
static char* current_file_token = NULL;

//Function prototypes are predeclared here as needed to avoid excessive restructuring of program
static generic_ast_node_t* cast_expression(FILE* fl);
static generic_ast_node_t* type_specifier(FILE* fl);
static generic_ast_node_t* assignment_expression(FILE* fl);
static generic_ast_node_t* unary_expression(FILE* fl);
static generic_ast_node_t* declaration(FILE* fl);
static generic_ast_node_t* compound_statement(FILE* fl);
static generic_ast_node_t* statement(FILE* fl);
static generic_ast_node_t* let_statement(FILE* fl);
static generic_ast_node_t* logical_or_expression(FILE* fl);
static generic_ast_node_t* case_statement(FILE* fl);
static generic_ast_node_t* default_statement(FILE* fl);
static generic_ast_node_t* declare_statement(FILE* fl);
//Definition is a special compiler-directive, it's executed here, and as such does not produce any nodes
static u_int8_t definition(FILE* fl);
static generic_ast_node_t* duplicate_subtree(const generic_ast_node_t* duplicatee);


/**
 * Simply prints a parse message in a nice formatted way
*/
void print_parse_message(parse_message_type_t message_type, char* info, u_int16_t line_num){
	//Build and populate the message
	parse_message_t parse_message;
	parse_message.message = message_type;
	parse_message.info = info;
	parse_message.line_num = line_num;

	//Fatal if error
	if(message_type == PARSE_ERROR){
		parse_message.fatal = 1;
	}

	//Now print it
	//Mapped by index to the enum values
	char* type[] = {"WARNING", "ERROR", "INFO"};

	//Print this out on a single line
	fprintf(stderr, "\n[FILE: %s] --> [LINE %d | COMPILER %s]: %s\n", current_file_token, parse_message.line_num, type[parse_message.message], parse_message.info);
}


/**
 * We will always return a pointer to the node holding the identifier. Due to the times when
 * this will be called, we can not do any symbol table validation here. 
 *
 * BNF "Rule": <identifier> ::= (<letter> | <digit> | _ | $){(<letter>) | <digit> | _ | $}*
 * Note all actual string parsing and validation is handled by the lexer
 */
static generic_ast_node_t* identifier(FILE* fl){
	//In case of error printing
	char info[ERROR_SIZE];

	//Grab the next token
	Lexer_item lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	
	//If we can't find it that's bad
	if(lookahead.tok != IDENT){
		sprintf(info, "String %s is not a valid identifier", lookahead.lexeme);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		//Create and return an error node that will be sent up the chain
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Create the identifier node
	generic_ast_node_t* ident_node = ast_node_alloc(AST_NODE_CLASS_IDENTIFIER); //Add the identifier into the node itself
	//Idents are assignable
	ident_node->is_assignable = ASSIGNABLE;
	//Copy the string we got into it
	strcpy(ident_node->identifier, lookahead.lexeme);
	//Default identifier type is s_int32
	ident_node->inferred_type = lookup_type(type_symtab, "i32")->type;
	//Add the line number
	ident_node->line_number = parser_line_num;

	//Return our reference to the node
	return ident_node;
}

/**
 * We will always return a pointer to the node holding the label identifier. Due to the times when
 * this will be called, we can not do any symbol table validation here. 
 *
 * BNF "Rule": <label-identifier> ::= ${(<letter>) | <digit> | _ | $}*
 * Note all actual string parsing and validation is handled by the lexer
 */
static generic_ast_node_t* label_identifier(FILE* fl){
	//In case of error printing
	char info[ERROR_SIZE];

	//Grab the next token
	Lexer_item lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	
	//If we can't find it that's bad
	if(lookahead.tok != LABEL_IDENT){
		sprintf(info, "String %s is not a valid label identifier", lookahead.lexeme);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		//Create and return an error node that will be sent up the chain
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Create the identifier node
	generic_ast_node_t* label_ident_node = ast_node_alloc(AST_NODE_CLASS_IDENTIFIER); //Add the identifier into the node itself
	//Copy the string we got into it
	strcpy(label_ident_node->identifier, lookahead.lexeme);
	//By default a label identifier is of type u_int64(memory address)
	label_ident_node->inferred_type = lookup_type(type_symtab, "u64")->type;
	//Add the line number
	label_ident_node->line_number = parser_line_num;

	//Return our reference to the node
	return label_ident_node;
}


/**
 * Handle a constant. There are 4 main types of constant, all handled by this function. A constant
 * is always the child of some parent node. We will always return the reference to the node
 * created here
 *
 * BNF Rule: <constant> ::= <integer-constant> 
 * 						  | <string-constant> 
 * 						  | <float-constant> 
 * 						  | <char-constant>
 */
static generic_ast_node_t* constant(FILE* fl, const_search_t const_search){
	//Lookahead token
	Lexer_item lookahead;

	//We should see one of the 4 constants here
	lookahead = get_next_token(fl, &parser_line_num, const_search);

	//Create our constant node
	generic_ast_node_t* constant_node = ast_node_alloc(AST_NODE_CLASS_CONSTANT);
	//Add the line number
	constant_node->line_number = parser_line_num;

	//We'll go based on what kind of constant that we have
	switch(lookahead.tok){
		case INT_CONST:
		case INT_CONST_FORCE_U:
			((constant_ast_node_t*)(constant_node->node))->constant_type = INT_CONST;
			//Store the int value we were given
			int32_t int_val = atoi(lookahead.lexeme);

			((constant_ast_node_t*)(constant_node->node))->int_val = int_val;

			//By default, int constants are of type s_int32
			if(lookahead.tok == INT_CONST_FORCE_U){
				constant_node->inferred_type = lookup_type(type_symtab, "i32")->type;
			} else {
				//Otherwise it's signed
				constant_node->inferred_type = lookup_type(type_symtab, "u32")->type;
			}
			break;

		case HEX_CONST:
			((constant_ast_node_t*)(constant_node->node))->constant_type = HEX_CONST;
			//Store the int value we were given
			int32_t hex_val = strtol(lookahead.lexeme, NULL, 16);

			((constant_ast_node_t*)(constant_node->node))->int_val = hex_val;

			//By default, int constants are of type s_int32
			constant_node->inferred_type = lookup_type(type_symtab, "i32")->type;
			break;

		case LONG_CONST:
		case LONG_CONST_FORCE_U:
			((constant_ast_node_t*)(constant_node->node))->constant_type = LONG_CONST;
			//Store the int value we were given
			int64_t long_val = atol(lookahead.lexeme);

			((constant_ast_node_t*)(constant_node->node))->long_val = long_val;

			//By default, int constants are of type s_int64 
			if(lookahead.tok == LONG_CONST_FORCE_U){
				constant_node->inferred_type = lookup_type(type_symtab, "u64")->type;
			} else {
				//Otherwise it's signed 
				constant_node->inferred_type = lookup_type(type_symtab, "i64")->type;
			}

			break;

		case FLOAT_CONST:
			((constant_ast_node_t*)(constant_node->node))->constant_type = FLOAT_CONST;
			//Grab the float val
			float float_val = atof(lookahead.lexeme);

			//Store the float value we were given
			((constant_ast_node_t*)(constant_node->node))->float_val = float_val;

			//By default, float constants are of type float32
			constant_node->inferred_type = lookup_type(type_symtab, "f32")->type;
			break;

		case CHAR_CONST:
			((constant_ast_node_t*)(constant_node->node))->constant_type = CHAR_CONST;
			//Grab the char val
			char char_val = *(lookahead.lexeme);

			//Store the char value that we were given
			((constant_ast_node_t*)(constant_node->node))->char_val = char_val;

			//Char consts are of type char(obviously)
			constant_node->inferred_type = lookup_type(type_symtab, "char")->type;
			break;

		case STR_CONST:
			((constant_ast_node_t*)(constant_node->node))->constant_type = STR_CONST;
			//String contants are of a char[] type. We will determine what the size of this char[] is here
			//Let's first find the string length
			u_int32_t length = strlen(lookahead.lexeme + 1);

			//If it's empty throw a warning
			if(length == 0){
				print_parse_message(WARNING, "0 length string given as constant", parser_line_num);
				num_warnings++;
				//This will fail out here
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}

			//Too long of a string
			if(length > 499){
				print_parse_message(PARSE_ERROR, "String literals may be at most 500 characters in length", parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}

			//Increment 1 to account for the null terminator
			length++;

			//So our type here is of char[length+1] because we have our null terminator
			char type_name[MAX_TYPE_NAME_LENGTH];
			//Compute what the name would be
			sprintf(type_name, "char[%d]", length);

			//Let's find the type if it's in the symtab
			symtab_type_record_t* found_type = lookup_type(type_symtab, type_name);

			//If we find it, great, and if not, we'll add it in
			if(found_type == NULL){
				//Grab the char type
				generic_type_t* char_type = lookup_type(type_symtab, "char")->type;

				//Create the char array
				generic_type_t* char_arr = create_array_type(char_type, parser_line_num, length);

				//The record for the string
				symtab_type_record_t* str_rec = create_type_record(char_arr);

				//Add this type into the symtab
				insert_type(type_symtab, str_rec);

				//Assign the type
				constant_node->inferred_type = char_arr;

			//Otherwise the type was defined by someone else, so we'll just reuse it
			} else {
				constant_node->inferred_type = found_type->type;
			}
			
			//By the time we make it down here, the type has been accounted for
			//We'll now copy the lexeme in
			strcpy(((constant_ast_node_t*)(constant_node->node))->string_val, lookahead.lexeme);
			break;

		default:
			print_parse_message(PARSE_ERROR, "Invalid constant given", parser_line_num);
			num_errors++;
			//Create and return an error node that will be propagated up
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//All went well so give the constant node back
	return constant_node;
}


/**
 * A function call looks for a very specific kind of identifer followed by
 * parenthesis and the appropriate number of parameters for the function, each of
 * the appropriate type
 * 
 * By the time we get here, we will have already consumed the "@" token
 *
 * BNF Rule: <function-call> ::= @<identifier>({<logical-or-expression>}?{, <logical-or-expression>}*)
 */
static generic_ast_node_t* function_call(FILE* fl){
	//For generic error printing
	char info[ERROR_SIZE];
	//The current line num
	u_int16_t current_line = parser_line_num;
	//The lookahead token
	Lexer_item lookahead;
	//A nicer reference that we'll keep to the function record
	symtab_function_record_t* function_record;
	//We'll also keep a nicer reference to the function name
	char* function_name;
	//The number of parameters that we've seen
	u_int8_t num_params = 0;
	//The number of parameters that the function actually takes
	u_int8_t function_num_params;
	
	//First grab the ident node
	generic_ast_node_t* ident = identifier(fl);

	//We have a general error-probably will be quite uncommon
	if(ident->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Non identifier provided as function call", parser_line_num);
		num_errors++;
		//We'll let the node propogate up
		return ident;
	}

	//Grab the function name out for convenience
	function_name = ident->identifier;

	//Let's now look up the function name in the function symtab
	function_record = lookup_function(function_symtab, function_name);

	//Important check here--if this function record does not exist, it means the user is trying to 
	//call a nonexistent function
	if(function_record == NULL){
		sprintf(info, "Function \"%s\" is being called before definition", function_name);
		print_parse_message(PARSE_ERROR, info, current_line);
		num_errors++;
		//Return the error node and get out
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now we can grab out some info for convenience
	function_num_params = function_record->number_of_params;

	//If we make it here, we know that our function actually exists. We can now create
	//the appropriate node that will hold all of our data about it
	//It is also now safe enough for us to allocate the function node
	generic_ast_node_t* function_call_node = ast_node_alloc(AST_NODE_CLASS_FUNCTION_CALL);

	//Store the function record in the node
	function_call_node->func_record = function_record;

	//We'll also add in that the current function has called this one
	call_function(current_function->call_graph_node, function_record->call_graph_node);
	//We'll now note that this was indeed called
	function_record->called = 1;

	//Add the inferred type in for convenience as well
	function_call_node->inferred_type = function_record->return_type;
	
	//We now need to see a left parenthesis for our param list
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//Fail out here
	if(lookahead.tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Left parenthesis expected on function call", parser_line_num);
		num_errors++;
		//Send this error node up the chain
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Push onto the grouping stack once we see this
	push_token(grouping_stack, lookahead);

	//If we only have one paramater for our function, we had better only see an R_PAREN here
	if(function_num_params == 0){
		//Grab the next token
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
		
		//If we don't see this it's bad
		if(lookahead.tok != R_PAREN){
			sprintf(info, "Function \"%s\" expects no parameters First declared here:", function_record->func_name);
			print_parse_message(PARSE_ERROR, info, current_line);
			//Print out the actual function record as well
			print_function_name(function_record);
			num_errors++;
			//Return the error node
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}
		
		//Be sure to clear the stack out
		pop_token(grouping_stack);

		//Otherwise this worked just fine, so we'll jump out here
		return function_call_node;
	}

	//Otherwise if we make it all the way here, we're going to need to do more complex checking

	//A node to hold our current parameter
	generic_ast_node_t* current_param;

	//A node to hold the current function parameter
	symtab_variable_record_t* current_function_param;

	//So long as we don't see the R_PAREN we aren't done
	while(1){
		//If we're exceeding the number of parameters, we'll fail out
		if(num_params > function_num_params){
			sprintf(info, "Function \"%s\" expects %d params, was given %d. First declared here:", function_name, function_num_params, num_params);
			print_parse_message(PARSE_ERROR, info, current_line);
			//Print out the actual function record as well
			print_function_name(function_record);
			num_errors++;
			//Return the error node
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Grab the current function param
		current_function_param = function_record->func_params[num_params].associate_var;

		//Parameters are in the form of a conditional expression
		current_param = logical_or_expression(fl);

		//We now have an error of some kind
		if(current_param->CLASS == AST_NODE_CLASS_ERR_NODE){
			print_parse_message(PARSE_ERROR, "Bad parameter passed to function call", current_line);
			num_errors++;
			//Return the error node -- it will propogate up the chain
			return current_param;
		}
	
		//Let's grab these to check for compatibility
		generic_type_t* param_type = current_function_param->type;
		generic_type_t* expr_type = current_param->inferred_type;

		//We now need to check the type equivalence here
		generic_type_t* param_type_checked = types_compatible(param_type, expr_type);

		//If this is null, it means that our check failed
		if(param_type_checked == NULL){
			sprintf(info, "Function \"%s\" expects an input of type \"%s\" as parameter %d, but was given an input of type \"%s\". First defined here:",
		   			function_name, param_type->type_name, num_params, expr_type->type_name);
			print_parse_message(PARSE_ERROR, info,  parser_line_num);
			//Print out the actual function record as well
			print_function_name(function_record);
			num_errors++;
			//Return the error node
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Otherwise it worked

		//We can now safely add this into the function call node as a child. In the function call node, 
		//the parameters will appear in order from left to right
		add_child_node(function_call_node, current_param);

		//Record that we saw one more parameter
		num_params++;

		//Refresh the token
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

		//Two options here, we can either see a COMMA or an R_PAREN
		//If it's an R_PAREN we're done
		if(lookahead.tok == R_PAREN){
			break;
		}

		//Otherwise it must be a comma. If it isn't we have a failure
		if(lookahead.tok != COMMA){
			print_parse_message(PARSE_ERROR, "Commas must be used to separate parameters in function call", parser_line_num);
			num_errors++;
			//Create and return an error node
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}
	}

	/**
	 * If we have a mismatch between what the function takes and what we want, throw an
	 * error
	 */
	if(num_params != function_num_params){
		sprintf(info, "Function %s expectects %d parameters, but was only given %d", function_record->func_name, function_num_params, num_params);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		print_function_name(function_record);
		num_errors++;
		//Error out
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Once we get here, we do need to finally verify that the closing R_PAREN matched the opening one
	if(pop_token(grouping_stack).tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected in function call", parser_line_num);
		num_errors++;
		//Return the error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Add the line number in
	function_call_node->line_number = current_line;

	//Destroy the ident node, we no longer need it

	//Otherwise, if we make it here, we're all good to return the function call node
	return function_call_node;
}


/**
 * A primary expression is, in a way, the termination of our expression chain. However, it can be used 
 * to chain back up to an expression in general using () as an enclosure. Just like all rules, a primary expression
 * itself has a parent and will produce children. The reference to the primary expression itself is always returned
 *
 * BNF Rule: <primary-expression> ::= <identifier>
 * 									| <constant> 
 * 									| (<logical-or-expression>)
 * 									| <function-call>
 */
static generic_ast_node_t* primary_expression(FILE* fl){
	//Freeze the current line number
	u_int16_t current_line = parser_line_num;
	//For error printing
	char info[ERROR_SIZE];
	//Lookahead token
	Lexer_item lookahead;

	//Grab the next token, we'll multiplex on this
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//We've seen an ident, so we'll put it back and let
	//that rule handle it. This identifier will always be 
	//a variable. It must also be a variable that has been initialized.
	//We will check that it was initialized here
	if(lookahead.tok == IDENT){
		//Put it back
		push_back_token(lookahead);

		//We will let the identifier rule actually grab the ident. In this case
		//the identifier will be a variable of some sort, that we'll need to check
		//against the symbol table
		generic_ast_node_t* ident = identifier(fl);

		//If there was a failure of some kind, we'll allow it to propogate up
		if(ident->CLASS == AST_NODE_CLASS_ERR_NODE){
			//Send the error up the chain
			return ident;
		}

		//Grab this out for convenience
		char* var_name = ident->identifier;

		//We have a few options here, we could find a constant that has been declared
		//like this. If so, we'll return a duplicate of the constant node that we have
		//inside of here
		symtab_constant_record_t* found_const = lookup_constant(constant_symtab, var_name);
		
		//If this is in fact a constant, we'll duplicate the whole thing and send it
		//out the door
		if(found_const != NULL){
			return duplicate_node(found_const->constant_node);
		}

		//Now we will look this up in the variable symbol table
		symtab_variable_record_t* found = lookup_variable(variable_symtab, var_name);

		//Record the current var for later use
		current_var = found;

		//We now must see a variable that was intialized. If it was not
		//initialized, then we have an issue
		if(found == NULL){
			sprintf(info, "Variable \"%s\" has not been declared", var_name);
			print_parse_message(PARSE_ERROR, info, current_line);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Store the inferred type
		ident->inferred_type = found->type;
		//Store the variable that's associated
		ident->variable = found;
		//Idents are assignable
		ident->is_assignable = ASSIGNABLE;

		//Give back the ident node
		return ident;

	//We can also see a constant
	} else if (lookahead.tok == INT_CONST || lookahead.tok == STR_CONST || lookahead.tok == FLOAT_CONST
			  || lookahead.tok == CHAR_CONST || lookahead.tok == LONG_CONST || lookahead.tok == HEX_CONST
			  || lookahead.tok == INT_CONST_FORCE_U || lookahead.tok == LONG_CONST_FORCE_U){
		//Again put the token back
		push_back_token(lookahead);

		//Call the constant rule to grab the constant node
		generic_ast_node_t* constant_node = constant(fl, SEARCHING_FOR_CONSTANT);

		//Fail out here if this happens
		if(constant_node->CLASS == AST_NODE_CLASS_ERR_NODE){
			return constant_node;
		}

		//Give back the constant node
		return constant_node;

	//This is the case where we are putting the expression
	//In parens
	} else if (lookahead.tok == L_PAREN){
		//We'll push it up to the stack for matching
		push_token(grouping_stack, lookahead);

		//We are now required to see a valid logical or expression expression
		generic_ast_node_t* expr = logical_or_expression(fl);

		//If it's an error, just give the node back
		if(expr->CLASS == AST_NODE_CLASS_ERR_NODE){
			return expr;
		}

		//Otherwise it worked, but we're still not done. We now must see the R_PAREN and
		//match it with the accompanying L_PAREN
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

		//Fail case here
		if(lookahead.tok != R_PAREN){
			print_parse_message(PARSE_ERROR, "Right parenthesis expected after expression", parser_line_num);
			num_errors++;
			//Create and return an error node
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Another fail case, if they're unmatched
		if(pop_token(grouping_stack).tok != L_PAREN){
			print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected", parser_line_num);
			num_errors++;
			//Create and return an error node
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Return the expression node
		return expr;
	
	//Otherwise, if we see an @ symbol, we know it's a function call
	} else if(lookahead.tok == AT){
		//We will let this rule handle the function call
		generic_ast_node_t* func_call = function_call(fl);

		//If we failed here
		if(func_call->CLASS == AST_NODE_CLASS_ERR_NODE){
			return func_call;
		}

		//Return the function call node
		return func_call;

	//Generic fail case
	} else {
		sprintf(info, "Expected identifier, constant or (<expression>), but got %s", lookahead.lexeme);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}
}


/**
 * An assignment expression can decay into a conditional expression or it
 * can actually do assigning. There is no chaining in Ollie language of assignments. There are two
 * options for treenodes here. If we see an actual assignment, there is a special assignment node
 * that will be made. If not, we will simply pass the parent along. An assignment expression will return
 * a reference to the subtree created by it
 *
 * BNF Rule: <assignment-expression> ::= <logical-or-expression> 
 * 									   | <unary-expression> := <logical-or-expression>
 *
 */
static generic_ast_node_t* assignment_expression(FILE* fl){
	//Info array for error printing
	char info[ERROR_SIZE];
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	//Lookahead token
	Lexer_item lookahead;

	//Did we find the assignment op?
	u_int8_t found_asn_op = FALSE;

	//Probably way too much, just to be safe
	Lexer_item items[200];
	//How many we have
	u_int16_t idx = 0;

	//Grab the next token
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//So long as we don't see a semicolon(end) or an assignment op, or a left or right curly
	while(lookahead.tok != COLONEQ && lookahead.tok != SEMICOLON && lookahead.tok != L_CURLY && lookahead.tok != R_CURLY){
		//Put this onto the items we've seen
		items[idx] = lookahead;
		idx++;

		//Otherwise refresh
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	}

	//Did we see the coloneq? If not, not an assingment operation
	if(lookahead.tok == COLONEQ){
		found_asn_op = TRUE;
	} else {
		//Otherwise we did not find it
		found_asn_op = FALSE;
	}

	//First push back lookahead
	push_back_token(lookahead);

	//Once we get here, we either found the assignment op or we didn't. First though, let's
	//put everything back where we found it
	for(int16_t i = idx - 1; i >= 0; i--){
		//Push these all back
		push_back_token(items[i]);
	}

	//If we didn't find an assignment operator, we just return the logical or expression
	if(found_asn_op == FALSE){
		return logical_or_expression(fl);
	}

	//If we make it here however, that means that we did see the assign keyword. Since
	//this is the case, we'll make a new assignment node and take the appropriate actions here 
	generic_ast_node_t* asn_expr_node = ast_node_alloc(AST_NODE_CLASS_ASNMNT_EXPR);	
	//Add in the line number
	asn_expr_node->line_number = current_line;

	//Now we must see a valid unary expression. The unary expression's parent
	//will itself be the assignment expression node
	
	//We'll let this rule handle it
	generic_ast_node_t* left_hand_unary = unary_expression(fl);

	//Fail out here
	if(left_hand_unary->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid left hand side given to assignment expression", current_line);
		//Return the erroneous node as we fail up the tree
		return left_hand_unary;
	}
	
	//If it isn't assignable, we also fail
	if(left_hand_unary->is_assignable == NOT_ASSIGNABLE){
		print_parse_message(PARSE_ERROR, "Expression is not assignable", left_hand_unary->line_number);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Otherwise it worked, so we'll add it in as the left child
	add_child_node(asn_expr_node, left_hand_unary);

	//Now if we get here, there is the chance that this left hand unary is constant. If it is, then
	//this assignment is illegal
	if(current_var->initialized == 1 && current_var->is_mutable == 0){
		sprintf(info, "Variable \"%s\" is not mutable. Use mut keyword if you wish to mutate. First defined here:", current_var->var_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		print_variable_name(current_var);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//If it was already intialized, this means that it's been "assigned to"
	if(current_var->initialized == 1){
		current_var->assigned_to = 1;
	} else {
		//Mark that this var was in fact initialized
		current_var->initialized = 1;
	}

	//Now we are required to see the := terminal
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	
	//Fail case here
	if(lookahead.tok != COLONEQ){
		sprintf(info, "Expected := symbol in assignment expression, instead got %s", lookahead.lexeme);
		printf("%d", lookahead.tok);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		//Return a special kind of error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now that we're here we must see a valid conditional expression
	generic_ast_node_t* expr = logical_or_expression(fl);

	//Fail case here
	if(expr->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid right hand side given to assignment expression", current_line);
		num_errors++;
		//The conditional is already an error, so we'll just return it
		return expr;
	}

	//Let's now see if we have compatible types
	generic_type_t* left_hand_type = left_hand_unary->inferred_type;
	generic_type_t* right_hand_type =  expr->inferred_type;

	//Final type here
	generic_type_t* final_type = types_compatible(left_hand_type, right_hand_type);
	
	//If they're not, we fail here
	if(final_type == NULL){
		sprintf(info, "Attempt to assign expression of type %s to variable of type %s", right_hand_type->type_name, left_hand_type->type_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//If the return type of the logical or expression is an address, is it an address of a mutable variable?
	if(expr->inferred_type->type_class == TYPE_CLASS_POINTER){
		if(expr->variable->is_mutable == 0 && left_hand_unary->variable->is_mutable == 1){
			print_parse_message(PARSE_ERROR, "Mutable references to immutable variables are forbidden", parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}
	}

	//Otherwise the overall type is the final type
	asn_expr_node->inferred_type = final_type;


	//Otherwise we know it worked, so we'll add the conditional in as the right child
	add_child_node(asn_expr_node, expr);

	//Return the reference to the overall node
	return asn_expr_node;
}


/**
 * A construct accessor is used to access a construct either on the heap of or on the stack.
 * Like all rules, it will return a reference to the root node of the tree that it created
 *
 * A constructor accessor node will be a subtree with the parent holding the actual operator
 * and its child holding the variable identifier
 *
 * We will expect to see the => or : here
 *
 * BNF Rule: <construct-accessor> ::= => <variable-identifier> 
 * 								    | : <variable-identifier>
 */
static generic_ast_node_t* construct_accessor(FILE* fl, generic_type_t** current_type){
	//For error printing
	char info[ERROR_SIZE];
	//Freeze the current line
	u_int16_t current_line = parser_line_num;
	//The lookahead token
	Lexer_item lookahead;

	//We'll first grab whatever token that we have here
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//This would be incredibly bizarre, as we know that they are already here
	if(lookahead.tok != ARROW_EQ && lookahead.tok != COLON){
		print_parse_message(PARSE_ERROR, "Fatal internal parser error at construct accessor", parser_line_num);
		num_errors++;
		//Error out
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Otherwise we'll now make the node here
	generic_ast_node_t* const_access_node = ast_node_alloc(AST_NODE_CLASS_CONSTRUCT_ACCESSOR);
	//Is assignable
	const_access_node->is_assignable = ASSIGNABLE;
	//Add the line number
	const_access_node->line_number = current_line;

	//Put the token in to show what we have
	const_access_node->construct_accessor_tok = lookahead.tok;

	//Grab a convenient reference to the type that we're working with
	generic_type_t* working_type = dealias_type(*current_type);

	//What is the type that we're referencing here
	generic_type_t* referenced_type;

	//If we have a =>, we need to have seen a pointer to a struct
	if(lookahead.tok == ARROW_EQ){
		//We need to specifically see a pointer to a struct for the current type
		//If it's something else, we fail out here
		if(working_type->type_class != TYPE_CLASS_POINTER){
			sprintf(info, "Type \"%s\" cannot be accessed with the => operator. First defined here:", working_type->type_name);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			print_type_name(lookup_type(type_symtab, working_type->type_name));
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//We can now pick out what type we're referencing(should be construct)
		referenced_type = working_type->pointer_type->points_to;

		//Now we know that its a pointer, but what does it point to?
		if(referenced_type->type_class != TYPE_CLASS_CONSTRUCT){
			sprintf(info, "Type \"%s\" is not a struct and cannot be accessed with the => operator. First defined here:", referenced_type->type_name);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			print_type_name(lookup_type(type_symtab, referenced_type->type_name));
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//The working type is whatever we reference here
		working_type = referenced_type;

	//Otherwise we know that we have some kind of non-pointer here(or so we hope)
	} else {
		//We need to specifically see a struct here
		if(working_type->type_class != TYPE_CLASS_CONSTRUCT){
			sprintf(info, "Type \"%s\" cannot be accessed with the : operator. First defined here:", working_type->type_name);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			print_type_name(lookup_type(type_symtab, working_type->type_name));
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//If we make it here we know that working type is a struct
		//We'll assign here for convenience
		referenced_type = working_type;
	}

	//Now we are required to see a valid variable identifier. TODO TYPE CHECKING
	generic_ast_node_t* ident = identifier(fl); 

	//For now we're just doing error checking TODO TYPE AND EXISTENCE CHECKING
	if(ident->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Construct accessor could not find valid identifier", current_line);
		num_errors++;
		//It already is an error node so we'll return it
		return ident;
	}

	//Grab this for nicety
	char* member_name = ident->identifier;

	//Let's find this type
	symtab_variable_record_t* var_record = lookup_variable(variable_symtab, member_name); 

	//If we can't find it we're out
	if(var_record == NULL){
		sprintf(info, "Variable \"%s\" is not a known member of any construct", member_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Otherwise we've found it, but is it a part of our struct?
	if(var_record->struct_defined_in == NULL){
		sprintf(info, "Variable \"%s\" is not a known member of any construct. First defined here:", member_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		print_variable_name(var_record);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}	

	//If we get here we know that it was defined in a struct, but is it in our struct?
	if(strcmp(var_record->struct_defined_in->type_name, working_type->type_name) != 0){
		sprintf(info, "Construct \"%s\" does not have a member named \"%s\". First defined here:", working_type->type_name, member_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		print_type_name(lookup_type(type_symtab, working_type->type_name));
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Otherwise, we finally know that its correct. As such, we'll update the current type to be whatever this is
	*current_type = var_record->type;

	//Mark the current variable too
	current_var = var_record;

	//Otherwise we know that it worked, so we'll add this guy in as a child of the overall construct
	//accessor
	add_child_node(const_access_node, ident);

	//And now we're all done, so we'll just give back the root reference
	return const_access_node;
}


/**
 * An array accessor represents a request to get something from an array memory region. Like all
 * nodes, an array accessor will return a reference to the subtree that it creates
 *
 * We expect that the caller has given back the [ token for this rule
 *
 * BNF Rule: <array-accessor> ::= [ <logical-or-expression> ]
 *
 */
static generic_ast_node_t* array_accessor(FILE* fl){
	//For error printing
	char info[ERROR_SIZE];
	//The lookahead token
	Lexer_item lookahead;
	//Freeze the current line
	u_int16_t current_line = parser_line_num;

	//We expect to see the left bracket here
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	
	//If we didn't see it, that's some weird internal error
	if(lookahead.tok != L_BRACKET){
		print_parse_message(PARSE_ERROR, "Fatal internal compiler error. Array accessor did not see [", current_line);
		num_errors++;
		//Fail out here
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Otherwise it all went well, so we'll push this onto the stack
	push_token(grouping_stack, lookahead);

	//Now we are required to see a valid constant expression representing what
	//the actual index is.
	generic_ast_node_t* expr = logical_or_expression(fl);

	//If we fail, automatic exit here
	if(expr->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid conditional expression given to array accessor", current_line);
		num_errors++;
		//It's already an error so we'll just return it
		return expr;
	}

	//We use a u_int32 as our reference
	generic_type_t* reference_type = lookup_type(type_symtab, "u32")->type;

	//Let's make sure that this is an int
	if(types_compatible(reference_type, expr->inferred_type) == NULL){
		sprintf(info, "Array accessing requires types compatible with \"u32\", but instead got \"%s\"", expr->inferred_type->type_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Otherwise, once we get here we need to check for matching brackets
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If wedon't see a right bracket, we'll fail out
	if(lookahead.tok != R_BRACKET){
		print_parse_message(PARSE_ERROR, "Right bracket expected at the end of array accessor", parser_line_num);
		num_errors++;
		//Give back an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//We also must check for matching with the brackets
	if(pop_token(grouping_stack).tok != L_BRACKET){
		print_parse_message(PARSE_ERROR, "Unmatched brackets detected in array accessor", current_line);
		num_errors++;
		//Again give back an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now that we've done all of our checks have been done, we can create the actual node
	generic_ast_node_t* array_acc_node = ast_node_alloc(AST_NODE_CLASS_ARRAY_ACCESSOR);
	//Add the line number
	array_acc_node->line_number = current_line;
	//TODO encode type info later
	//The conditional expression is a child of this node
	add_child_node(array_acc_node, expr);

	//And now we're done so give back the root reference
	return array_acc_node;
}


/**
 * A postfix expression decays into a primary expression, and there are certain
 * operators that can be chained if context allows. Like all other rules, this rule
 * returns a reference to the root node that it creates
 *
 * As an important note here: We can chain construct accessors and array accessors as much as we wish, 
 * but seeing a plusplus or minusminus is the defintive end of this rule if we see it
 *
 * <postfix-expression> ::= <primary-expression> 
 *						  | <primary-expression> {{<construct-accessor>}*{<array-accessor>*}}* {++|--}?
 */ 
static generic_ast_node_t* postfix_expression(FILE* fl){
	//For error printing
	char info[ERROR_SIZE];
	//Lookahead token
	Lexer_item lookahead;
	//Freeze the current line number
	u_int16_t current_line = parser_line_num;

	//No matter what, we have to first see a valid primary expression
	generic_ast_node_t* result = primary_expression(fl);

	//If we fail, then we're bailing out here
	if(result->CLASS == AST_NODE_CLASS_ERR_NODE){
		//Just return, no need for any errors here
		return result;
	}

	//Peek at the next token
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	
	//Let's just check if we're able to get out immediately
	if(lookahead.tok != L_BRACKET && lookahead.tok != COLON && lookahead.tok != ARROW_EQ
	   && lookahead.tok != PLUSPLUS && lookahead.tok != MINUSMINUS){
		//Put the token back
		push_back_token(lookahead);
		//Just return what primary expr gave us
		return result;
	}
	
	//If we make it down to here, we know that we're trying to access a variable. As such, 
	//we need to make sure that we don't see a constant here
	if(result->CLASS == AST_NODE_CLASS_CONSTANT){
		print_parse_message(PARSE_ERROR, "Constants are not assignable", current_line);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Otherwise we at least know that it isn't a constant

	//Otherwise if we make it here, we know that we will have some kind of complex accessor or 
	//post operation, so we can make the node for it
	generic_ast_node_t* postfix_expr_node = ast_node_alloc(AST_NODE_CLASS_POSTFIX_EXPR);
	//Add the line number
	postfix_expr_node->line_number = current_line;

	//This node will always have the primary expression as its first child
	add_child_node(postfix_expr_node, result);

	//Let's grab whatever type that we currently have
	generic_type_t* current_type = result->inferred_type;
	//Do any kind of dealiasing that we need to do
	current_type = dealias_type(current_type);

	//We assume it's assignable, that will only change if we have a basic type that is 
	//post inc'd/dec'd
	postfix_expr_node->is_assignable = ASSIGNABLE;

	//Now we can see as many construct accessor and array accessors as we can take
	while(lookahead.tok == L_BRACKET || lookahead.tok == COLON || lookahead.tok == ARROW_EQ){
		//Let's see which rule it is
		//We have an array accessor
		if(lookahead.tok == L_BRACKET){
			//Put the token back
			push_back_token(lookahead);

			//Before we go on, let's see what we have as the current type here. Both arrays and pointers are subscriptable items
			if(current_type->type_class != TYPE_CLASS_ARRAY && current_type->type_class != TYPE_CLASS_POINTER){
				sprintf(info, "Type \"%s\" is not subscriptable. First declared here:", current_type->type_name);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				//Print it out
				print_type_name(lookup_type(type_symtab, current_type->type_name));
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}

			//Let the array accessor handle it
			generic_ast_node_t* array_acc = array_accessor(fl);
			
			//Let's see if it actually worked
			if(array_acc->CLASS == AST_NODE_CLASS_ERR_NODE){
				print_parse_message(PARSE_ERROR, "Invalid array accessor found in postfix expression", current_line);
				num_errors++;
				//It's already an error, so we'll just give it back
				return array_acc;
			}

			//Otherwise we know it worked. Since this is the case, we can add it as a child to the overall
			//node
			add_child_node(postfix_expr_node, array_acc);

			//Based on this, the current type is whatever this array contains. We'll also use this for size determinations
			if(current_type->type_class == TYPE_CLASS_ARRAY){
				current_type = dealias_type(current_type->array_type->member_type);
			} else {
				//Otherwise we know that it must be a pointer
				current_type = dealias_type(current_type->pointer_type->points_to);
			}

		//Otherwise we have a construct accessor
		} else {
			//Put it back for the rule to deal with
			push_back_token(lookahead);

			//Let's have the rule do it. (This will update current_type for us)
			generic_ast_node_t* constr_acc = construct_accessor(fl, &current_type);

			//We have our fail case here
			if(constr_acc->CLASS == AST_NODE_CLASS_ERR_NODE){
				print_parse_message(PARSE_ERROR, "Invalid construct accessor found in postfix expression", current_line);
				num_errors++;
				//It's already an error so send it up
				return constr_acc;
			}

			//Otherwise we know it's good, so we'll add it in as a child
			add_child_node(postfix_expr_node, constr_acc);
		}
		
		//refresh the lookahead for the next iteration
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	}

	//We now have whatever the end type is stored in current type. Let's make sure it's raw
	generic_type_t* return_type = dealias_type(current_type);

	//Now once we get here, we know that we have something that isn't one of accessor rules
	//It could however be postinc/postdec. Let's first see if it isn't
	if(lookahead.tok != PLUSPLUS && lookahead.tok != MINUSMINUS){
		//Put the token back
		push_back_token(lookahead);
		//Assign the type
		postfix_expr_node->inferred_type = return_type;
		//This was assigned to
		result->variable->assigned_to = 1;
		//And we'll give back what we had constructed so far
		return postfix_expr_node;
	}

	//If it's a complex type we fail immediately
	if(return_type->type_class == TYPE_CLASS_ENUMERATED || return_type->type_class == TYPE_CLASS_CONSTRUCT){
		sprintf(info, "Type %s is an invalid operand for ++ or -- operand", return_type->type_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//One other potential issue -- if we see  void here
	if(return_type->type_class == TYPE_CLASS_BASIC && return_type->basic_type->basic_type == VOID){
		sprintf(info, "Type %s is an invalid operand for ++ or -- operand", return_type->type_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//You cannot assign to a basic variable that is
	//post-inc'd
	if(return_type->type_class == TYPE_CLASS_BASIC){
		postfix_expr_node->is_assignable = NOT_ASSIGNABLE;
	}

	//Otherwise if we get here we know that we either have post inc or dec
	//Create the unary operator node
	generic_ast_node_t* unary_post_op = ast_node_alloc(AST_NODE_CLASS_UNARY_OPERATOR);

	//Store the token
	unary_post_op->unary_operator = lookahead.tok;

	//This will always be the last child of whatever we've built so far
	add_child_node(postfix_expr_node, unary_post_op);
	
	//Add the inferred type in
	postfix_expr_node->inferred_type = return_type;

	//Carry through
	postfix_expr_node->variable = result->variable;

	//Now that we're done, we can get out
	return postfix_expr_node;
}


/**
 * This helper function negates a constant node's value
 */
static void negate_constant_value(generic_ast_node_t* constant_node){
	//Grab the constant node out
	constant_ast_node_t* const_node = ((constant_ast_node_t*)(constant_node->node));

	//Switch based on the value here
	switch(const_node->constant_type){
		//Negate these accordingly
		case INT_CONST_FORCE_U:
		case INT_CONST:
			const_node->int_val = const_node->int_val * -1;
			break;
		case FLOAT_CONST:
			const_node->float_val = const_node->float_val * -1;
			break;
		case CHAR_CONST:
			const_node->char_val = const_node->char_val * -1;
		case LONG_CONST_FORCE_U:
		case LONG_CONST:
			const_node->long_val = const_node->long_val * -1;
		//This should never happen
		default:
			print_parse_message(PARSE_ERROR, "Attempt to negate an invalid value", constant_node->line_number);
			exit(0);
	}
}


/**
 * This helper function decrements a constant node's value
 */
static void decrement_constant_value(generic_ast_node_t* constant_node){
	//Grab the constant node out
	constant_ast_node_t* const_node = ((constant_ast_node_t*)(constant_node->node));

	//Switch based on the value here
	switch(const_node->constant_type){
		//Negate these accordingly
		case INT_CONST_FORCE_U:
		case INT_CONST:
			const_node->int_val = const_node->int_val - 1;
			break;
		case FLOAT_CONST:
			const_node->float_val = const_node->float_val - 1;
			break;
		case CHAR_CONST:
			const_node->char_val = const_node->char_val - 1;
		case LONG_CONST_FORCE_U:
		case LONG_CONST:
			const_node->long_val = const_node->long_val - 1;
		//This should never happen
		default:
			print_parse_message(PARSE_ERROR, "Attempt to decrement an invalid value", constant_node->line_number);
			exit(0);
	}
}


/**
 * This helper function increments a constant node's value
 */
static void increment_constant_value(generic_ast_node_t* constant_node){
	//Grab the constant node out
	constant_ast_node_t* const_node = ((constant_ast_node_t*)(constant_node->node));

	//Switch based on the value here
	switch(const_node->constant_type){
		//Negate these accordingly
		case INT_CONST_FORCE_U:
		case INT_CONST:
			const_node->int_val = const_node->int_val + 1;
			break;
		case FLOAT_CONST:
			const_node->float_val = const_node->float_val + 1;
			break;
		case CHAR_CONST:
			const_node->char_val = const_node->char_val + 1;
		case LONG_CONST_FORCE_U:
		case LONG_CONST:
			const_node->long_val = const_node->long_val + 1;
		//This should never happen
		default:
			print_parse_message(PARSE_ERROR, "Attempt to increment an invalid value", constant_node->line_number);
			exit(0);
	}
}


/**
 * This helper function will logically not a consant node's value
 */
static void logical_not_constant_value(generic_ast_node_t* constant_node){
	//Grab the constant node out
	constant_ast_node_t* const_node = ((constant_ast_node_t*)(constant_node->node));

	//Switch based on the value here
	switch(const_node->constant_type){
		//Negate these accordingly
		case INT_CONST_FORCE_U:
		case INT_CONST:
			const_node->int_val = !(const_node->int_val);
			break;
		case CHAR_CONST:
			const_node->char_val = !(const_node->char_val);
		case LONG_CONST_FORCE_U:
		case LONG_CONST:
			const_node->long_val = !(const_node->long_val);
		//This should never happen
		default:
			print_parse_message(PARSE_ERROR, "Attempt to logically not an invalid value", constant_node->line_number);
			exit(0);
	}
}


/**
 * This helper function will logically not a consant node's value
 */
static void bitwise_not_constant_value(generic_ast_node_t* constant_node){
	//Grab the constant node out
	constant_ast_node_t* const_node = ((constant_ast_node_t*)(constant_node->node));

	//Switch based on the value here
	switch(const_node->constant_type){
		//Negate these accordingly
		case INT_CONST_FORCE_U:
		case INT_CONST:
			const_node->int_val = ~(const_node->int_val);
			break;
		case CHAR_CONST:
			const_node->char_val = ~(const_node->char_val);
		case LONG_CONST_FORCE_U:
		case LONG_CONST:
			const_node->long_val = ~(const_node->long_val);
		//This should never happen
		default:
			print_parse_message(PARSE_ERROR, "Attempt to bitwise not an invalid value", constant_node->line_number);
			exit(0);
	}
}


/**
 * A unary expression decays into a postfix expression. With a unary expression, we are able to
 * apply unary operators and take the size of given types. Like all rules, a unary expression
 * will always return a pointer to the root node of the tree that it creates
 *
 * BNF Rule: <unary-expression> ::= <postfix-expression> 
 * 								  | <unary-operator> <cast-expression> 
 * 								  | typesize(<type-specifier>) * compiler directive *
 * 								  | sizeof(<logical-or-expression>) * compiler directive *
 *
 * Important notes for typesize: It is assumed that the type-specifier node will handle
 * any/all error checking that we need. Type specifier will throw an error if the type has 
 * not been defined
 *
 * For convenience, we will also handle any/all unary operators here
 *
 * TYPE INFERENCE RULES: There are several separate rules that work here
 * 	1.) * operator only works on pointers, and the return type is what the point points to
 * 	2.) & operator works on everything except for void(you can't take the address of nothing)
 *
 * BNF Rule: <unary-operator> ::= & 
 * 								| * 
 * 								| - 
 * 								| ~ 
 * 								| ! 
 * 								| ++ 
 * 								| --
 */
static generic_ast_node_t* unary_expression(FILE* fl){
	//For error printing
	char info[ERROR_SIZE];
	//The lookahead token
	Lexer_item lookahead;
	//Is this assignable
	variable_assignability_t is_assignable = ASSIGNABLE;
	//For folding cases
	Token unary_op_tok = BLANK;

	//Let's see what we have
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we see the typesize keyword, we are locked in to the typesize rule
	//The typesize rule is a compiler only directive. Since we know the size of all
	//valid types at compile-time, we will be able to return an INT-CONST node with the
	//size here
	if(lookahead.tok == TYPESIZE){
		//Not assignable
		is_assignable = NOT_ASSIGNABLE;
		//We must then see left parenthesis
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

		//Fail case here
		if(lookahead.tok != L_PAREN){
			print_parse_message(PARSE_ERROR, "Left parenthesis expected after typesize call", parser_line_num);
			num_errors++;
			//Create and return an error node
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Otherwise we'll push to the stack for checking
		push_token(grouping_stack, lookahead);

		//Now we need to see a valid type-specifier. It is important to note that the type
		//specifier requires that a type has actually been defined. If it wasn't defined,
		//then this will return an error node
		generic_ast_node_t* type_spec = type_specifier(fl);

		//If it's an error
		if(type_spec->CLASS == AST_NODE_CLASS_ERR_NODE){
			print_parse_message(PARSE_ERROR, "Unable to use typesize on undefined type",  parser_line_num);
			num_errors++;
			//It's already an error, so give it back that way
			return type_spec;
		}

		//Once we've done this, we can grab the actual size of the type-specifier
		u_int32_t type_size = type_spec->inferred_type->type_size;

		//And then we no longer need the type-spec node, we can just remove it

		//Otherwise if we get here it actually was defined, so now we'll look for an R_PAREN
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

		//Fail out here if we don't see it
		if(lookahead.tok != R_PAREN){
			print_parse_message(PARSE_ERROR, "Right parenthesis expected after type specifer", parser_line_num);
			num_errors++;
			//Create and return the error
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//We can also fail if we somehow see unmatched parenthesis
		if(pop_token(grouping_stack).tok != L_PAREN){
			print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected in typesize expression", parser_line_num);
			num_errors++;
			//Create and return the error
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Create a constant node
		generic_ast_node_t* const_node = ast_node_alloc(AST_NODE_CLASS_CONSTANT);
		//Add the line number
		const_node->line_number = parser_line_num;
		//Add the constant
		((constant_ast_node_t*)(const_node->node))->constant_type = INT_CONST;
		//Store the actual value
		((constant_ast_node_t*)(const_node->node))->int_val = type_size;
		//Grab and store type info
		//Constants are ALWAYS of type s_int32
		const_node->inferred_type = lookup_type(type_symtab, "i32")->type;

		//Create the unary expression node
		generic_ast_node_t* unary_expr = ast_node_alloc(AST_NODE_CLASS_UNARY_EXPR);
		//Add the constant as a child
		add_child_node(unary_expr, const_node);

		//Finally we'll return this constant node
		return unary_expr;

	} else if(lookahead.tok == SIZEOF){
		//Not assignable
		is_assignable = NOT_ASSIGNABLE;
		//We must then see left parenthesis
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

		//Fail case here
		if(lookahead.tok != L_PAREN){
			print_parse_message(PARSE_ERROR, "Left parenthesis expected after sizeof call", parser_line_num);
			num_errors++;
			//Create and return an error node
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Otherwise we'll push to the stack for checking
		push_token(grouping_stack, lookahead);

		//We now need to see a valid logical or expression. This expression will contain everything that we need to know, and the
		//actual expression result will be unused
		generic_ast_node_t* expr_node = logical_or_expression(fl);
		
		//If it's an error
		if(expr_node->CLASS == AST_NODE_CLASS_ERR_NODE){
			print_parse_message(PARSE_ERROR, "Unable to use varsize on invalid expression",  parser_line_num);
			num_errors++;
			//It's already an error, so give it back that way
			return expr_node;
		}

		//Otherwise if we get here it actually was defined, so now we'll look for an R_PAREN
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

		//Fail out here if we don't see it
		if(lookahead.tok != R_PAREN){
			print_parse_message(PARSE_ERROR, "Right parenthesis expected after type specifer", parser_line_num);
			num_errors++;
			//Create and return the error
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//We can also fail if we somehow see unmatched parenthesis
		if(pop_token(grouping_stack).tok != L_PAREN){
			print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected in typesize expression", parser_line_num);
			num_errors++;
			//Create and return the error
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Now we know that we have an entirely syntactically valid call to sizeof. Let's now extract the 
		//type information for ourselves
		generic_type_t* return_type = expr_node->inferred_type;

		//One we get here, we have both nodes that we need
		generic_ast_node_t* unary_node = ast_node_alloc(AST_NODE_CLASS_UNARY_EXPR);
		//Add the line number
		unary_node->line_number = parser_line_num;

		//Create a constant node
		generic_ast_node_t* const_node = ast_node_alloc(AST_NODE_CLASS_CONSTANT);
		((constant_ast_node_t*)(const_node->node))->constant_type = INT_CONST;
		//Store the actual value of the type size
		((constant_ast_node_t*)(const_node->node))->int_val = return_type->type_size;
		//Grab and store type info
		//Constants are ALWAYS of type s_int32
		const_node->inferred_type = lookup_type(type_symtab, "i32")->type;

		//The first child is always the constant type
		add_child_node(unary_node, const_node); 
		//The second child is always the expression that we need to do
		add_child_node(unary_node, expr_node);

		//TODO NEEDS TO BE FIXED
		//Finally we'll return this constant node
		return unary_node;

	//Otherwise there is a potential for us to have any other unary operator. If we see any of these, we'll handle them
	//the exact same way
	} else if(lookahead.tok == PLUSPLUS || lookahead.tok == MINUS || lookahead.tok == MINUSMINUS
		     || lookahead.tok == STAR || lookahead.tok == AND || lookahead.tok == B_NOT || lookahead.tok == L_NOT){

		//We'll first create the unary operateor node for ourselves here
		generic_ast_node_t* unary_op = ast_node_alloc(AST_NODE_CLASS_UNARY_OPERATOR);
		//Assign the typesize operator to this
		unary_op->unary_operator = lookahead.tok;
		//Save this for later too
		unary_op_tok = lookahead.tok;
		
		//Following this, we are required to see a valid cast expression
		generic_ast_node_t* cast_expr = cast_expression(fl);

		//Let's check for errors
		if(cast_expr->CLASS == AST_NODE_CLASS_ERR_NODE){
			print_parse_message(PARSE_ERROR, "Invalid cast expression given after unary operator", parser_line_num);
			//If it is bad, we'll just propogate it up the chain
			return cast_expr;
		}

		//Holder for our return type
		generic_type_t* return_type;

		//Let's check the * case
		if(lookahead.tok == STAR){
			//If this is the case, then the cast expression had to have been a pointer or an array
			if(cast_expr->inferred_type->type_class != TYPE_CLASS_POINTER && cast_expr->inferred_type->type_class != TYPE_CLASS_ARRAY){
				sprintf(info, "Attempt to deference non-pointer type %s", cast_expr->inferred_type->type_name);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}

			//Otherwise if we made it here, we only have one final tripping point
			//Ensure that we aren't trying to deref a null pointer
			if(cast_expr->inferred_type->type_class == TYPE_CLASS_POINTER && strcmp(cast_expr->inferred_type->type_name, "void*") == 0){
				print_parse_message(PARSE_ERROR, "Attempt to derefence void*, you must cast before derefencing", parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}

			//Otherwise our dereferencing worked, so the return type will be whatever this points to
			//Grab what it references whether its a pointer or an array
			if(cast_expr->inferred_type->type_class == TYPE_CLASS_POINTER){
				return_type = cast_expr->inferred_type->pointer_type->points_to;
			} else {
				return_type = cast_expr->inferred_type->array_type->member_type;
			}

			//This is assignable
			is_assignable = ASSIGNABLE;

		//Let's now check the & case
		} else if (lookahead.tok == AND){
			//Is there an attempt to take the address of a constant
			if(cast_expr->CLASS == AST_NODE_CLASS_CONSTANT){
				print_parse_message(PARSE_ERROR, "The address of a constant cannot be taken", parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}

			//Let's double check that we aren't taking the address of nothing
			if(cast_expr->inferred_type->type_class == TYPE_CLASS_BASIC && cast_expr->inferred_type->basic_type->basic_type == VOID){
				print_parse_message(PARSE_ERROR, "Type \"void\" cannot have it's address taken", parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}

			//Otherwise it worked just fine, so we'll create a type of pointer to whatever it's type was
			generic_type_t* pointer = create_pointer_type(cast_expr->inferred_type, parser_line_num);

			//We'll check to see if this type is already in existence
			symtab_type_record_t* type_record = lookup_type(type_symtab, pointer->type_name);
			
			//It didn't exist, so we'll add it
			if(type_record == NULL){
				insert_type(type_symtab, create_type_record(pointer));
				//Set the return type to be a pointer
				return_type = pointer;
			//Otherwise it does exist so we'll just grab whatever we got
			} else {
				return_type = type_record->type;
			}

			//This is not assignable
			is_assignable = NOT_ASSIGNABLE;

		//Logical not(!) works on all basic types(minus void) and pointers
		} else if(lookahead.tok == L_NOT){
			//Let's check that we aren't trying to not a complex type
			if(cast_expr->inferred_type->type_class == TYPE_CLASS_ENUMERATED || cast_expr->inferred_type->type_class == TYPE_CLASS_CONSTRUCT){
				sprintf(info, "Type %s is an invalid operand for logical not(!)", cast_expr->inferred_type->type_name);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}

			//Make sure that this isn't a void type either
			if(cast_expr->inferred_type->type_class == TYPE_CLASS_BASIC && cast_expr->inferred_type->basic_type->basic_type == VOID){
				sprintf(info, "Type %s is an invalid operand for logical not(!)", cast_expr->inferred_type->type_name);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}

			//Otherwise it is fine. Logical not returns 0 or 1, so it's return type will be of u_int8
			return_type = lookup_type(type_symtab, "u8")->type;
			
			//This is not assignable
			is_assignable = NOT_ASSIGNABLE;
		
		//Bitwise not works on integers only
		} else if(lookahead.tok == B_NOT){
			//If it's not a basic type, we fail immediately
			if(cast_expr->inferred_type->type_class != TYPE_CLASS_BASIC){
				sprintf(info, "Type %s is an invalid operand for bitwise not(~)", cast_expr->inferred_type->type_name);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}

			//Otherwise if we make it down here, it still may not be good. We can not use bitwise not on floats or void
			if(cast_expr->inferred_type->basic_type->basic_type == FLOAT32 || cast_expr->inferred_type->basic_type->basic_type == FLOAT64
			   || cast_expr->inferred_type->basic_type->basic_type == VOID){
				sprintf(info, "Type %s is an invalid operand for bitwise not(~)", cast_expr->inferred_type->type_name);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}

			//Otherwise if we make it down here, the return type will be whatever type we put in
			return_type = cast_expr->inferred_type;

			//This is not assignable
			is_assignable = NOT_ASSIGNABLE;

		//Positive and negative sign works on integers and floats, but nothing else
		} else if(lookahead.tok == MINUS){
			//If it's not a basic type, we fail immediately
			if(cast_expr->inferred_type->type_class != TYPE_CLASS_BASIC){
				sprintf(info, "Type %s is an invalid operand for minus operand", cast_expr->inferred_type->type_name);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}

			//Otherwise if we make it down here, it still may not be good. We can not use bitwise not on floats or void
			if(cast_expr->inferred_type->basic_type->basic_type == VOID){
				sprintf(info, "Type %s is an invalid operand for minus operand", cast_expr->inferred_type->type_name);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}

			//If we get all the way down here, the return type is what we had to begin with
			return_type = cast_expr->inferred_type;

			//This is not assignable
			is_assignable = NOT_ASSIGNABLE;

		//preincrement and predecrement work on everything besides complex types
		} else if(lookahead.tok == PLUSPLUS || lookahead.tok == MINUSMINUS){
			//If it's a complex type we fail immediately
			if(cast_expr->inferred_type->type_class == TYPE_CLASS_ENUMERATED || cast_expr->inferred_type->type_class == TYPE_CLASS_CONSTRUCT){
				sprintf(info, "Type %s is an invalid operand for ++ or -- operand", cast_expr->inferred_type->type_name);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}

			//One other potential issue -- if we see  void here
			if(cast_expr->inferred_type->type_class == TYPE_CLASS_BASIC && cast_expr->inferred_type->basic_type->basic_type == VOID){
				sprintf(info, "Type %s is an invalid operand for ++ or -- operand", cast_expr->inferred_type->type_name);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}

			//Otherwise it worked just fine here. The return type is the same type that we had initially
			return_type = cast_expr->inferred_type;


			//This counts as mutation -- unless it's a constant
			if(cast_expr->variable != NULL){
				cast_expr->variable->assigned_to = 1;
			}

			//This is only not assignable if we have a basic variable
			if(return_type->type_class == TYPE_CLASS_BASIC){
				is_assignable = NOT_ASSIGNABLE;
			} else {
				//Otherwise it is assignable
				is_assignable = ASSIGNABLE;
			}
		}

		//If we have a special "fold" case here, i.e. we have something
		//like -2, we can actually do that right now
		if(cast_expr->first_child->CLASS == AST_NODE_CLASS_CONSTANT){
			//We'll now try to perform some folding
			if(unary_op_tok == MINUS){
				negate_constant_value(cast_expr->first_child);
			} else if(unary_op_tok == MINUSMINUS){
				decrement_constant_value(cast_expr->first_child);
			} else if(unary_op_tok == PLUSPLUS){
				increment_constant_value(cast_expr->first_child);
			} else if(unary_op_tok == L_NOT){
				logical_not_constant_value(cast_expr->first_child);
			} else if(unary_op_tok == B_NOT){
				bitwise_not_constant_value(cast_expr->first_child);
			}

			//Give back this node and we're done
			return cast_expr;
		}

		//One we get here, we have both nodes that we need
		generic_ast_node_t* unary_node = ast_node_alloc(AST_NODE_CLASS_UNARY_EXPR);
		
		//The unary operator always comes first
		add_child_node(unary_node, unary_op);

		//The cast expression will be linked in last
		add_child_node(unary_node, cast_expr);

		//Store the type that we have here
		unary_node->inferred_type = return_type;
		//Store the line number
		unary_node->line_number = parser_line_num;
		//Store the variable
		unary_node->variable = cast_expr->variable;
		//Is it assignable
		unary_node->is_assignable = is_assignable;

		//Finally we're all done, so we can just give this back
		return unary_node;
	
	//If we get here we will just put the token back and pass the responsibility on to the
	//postifix expression rule
	} else {
		push_back_token(lookahead);
		//We'll still make a top level tree here to avoid ambiguity
		generic_ast_node_t* unary_expr_node = ast_node_alloc(AST_NODE_CLASS_UNARY_EXPR);

		//Let this handle the heavy lifting
		generic_ast_node_t* postfix_expr_node = postfix_expression(fl);

		//If this is NULL, just send it up the chain
		if(postfix_expr_node->CLASS == AST_NODE_CLASS_ERR_NODE){
			return postfix_expr_node;
		}

		//Otherwise he is the unary expression node here
		add_child_node(unary_expr_node, postfix_expr_node);

		//Carry the var through
		unary_expr_node->variable = postfix_expr_node->variable;

		//This type info is also sent up
		unary_expr_node->inferred_type = postfix_expr_node->inferred_type;
		//Store the line number
		unary_expr_node->line_number = parser_line_num;
		//Duplicate the assignability
		unary_expr_node->is_assignable = postfix_expr_node->is_assignable;

		//Postfix already has type inference built in
		return unary_expr_node;
	}
}


/**
 * A cast expression decays into a unary expression
 *
 * BNF Rule: <cast-expression> ::= <unary-expression> 
 * 						    	| < <type-specifier> > <unary-expression>
 */
static generic_ast_node_t* cast_expression(FILE* fl){
	//For error printing
	char info[ERROR_SIZE];
	//The lookahead token
	Lexer_item lookahead;

	//If we first see an angle bracket, we know that we are truly doing
	//a cast. If we do not, then this expression is just a pass through for
	//a unary expression
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	
	//If it's not the <, put the token back and just return the unary expression
	if(lookahead.tok != L_THAN){
		push_back_token(lookahead);

		//Let this handle it
		return unary_expression(fl);
	}

	//Push onto the stack for matching
	push_token(grouping_stack, lookahead);

	//Grab the type specifier
	generic_ast_node_t* type_spec = type_specifier(fl);

	//If it's an error, we'll print and propagate it up
	if(type_spec->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid type specifier given to cast expression", parser_line_num);
		num_errors++;
		//It is the error, so we can return it
		return type_spec;
	}

	//We now have to see the closing braces that we need
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we didn't see a match
	if(lookahead.tok != G_THAN){
		print_parse_message(PARSE_ERROR, "Expected closing > at end of cast", parser_line_num);
		num_errors++;
		//Create and give back an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Make sure we match
	if(pop_token(grouping_stack).tok != L_THAN){
		print_parse_message(PARSE_ERROR, "Unmatched angle brackets given to cast statement", parser_line_num);
		num_errors++;
		//Create and give back an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now we have to see a valid unary expression. This is our last potential fail case in the chain
	//The unary expression will handle this for us
	generic_ast_node_t* right_hand_unary = unary_expression(fl);

	//If it's an error we'll jump out
	if(right_hand_unary->CLASS == AST_NODE_CLASS_ERR_NODE){
		return right_hand_unary;
	}

	//No we'll need to determine if we can actually cast here
	//What we're trying to cast to
	generic_type_t* casting_to_type = dealias_type(type_spec->inferred_type);
	//What is being casted
	generic_type_t* being_casted_type = dealias_type(right_hand_unary->inferred_type);

	//You can never cast a "void" to anything
	if(being_casted_type->type_class == TYPE_CLASS_BASIC && being_casted_type->basic_type->basic_type == VOID){
		sprintf(info, "Type %s cannot be casted to any other type", being_casted_type->type_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Likewise, you can never cast anything to void
	if(casting_to_type->type_class == TYPE_CLASS_BASIC && casting_to_type->basic_type->basic_type == VOID){
		sprintf(info, "Type %s cannot be casted to type %s", being_casted_type->type_name, casting_to_type->type_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//You can never cast anything to be a construct
	if(casting_to_type->type_class == TYPE_CLASS_CONSTRUCT){
		print_parse_message(PARSE_ERROR, "No type can be casted to a construct type", parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Otherwise if we've made it down to here, we can just use the types_compatible function to see what we can do
	generic_type_t* return_type = types_compatible(casting_to_type, being_casted_type);

	//This is our fail case
	if(return_type == NULL){
		sprintf(info, "Type %s cannot be casted to type %s", being_casted_type->type_name, casting_to_type->type_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//These types are now inferenced
	right_hand_unary->inferred_type = type_spec->inferred_type;

	//Once we get here, we no longer need the type specifier

	//Finally, we're all set to go here, so we can return the root reference
	return right_hand_unary;
}


/**
 * A multiplicative expression can be chained and decays into a cast expression. This method
 * will return a pointer to the root of the subtree that is created by it, whether that subtree
 * originated here or not
 *
 * TYPE INFERENCE RULES: Multiplicative expressions always return the dominating type
 * 	1.) Arrays, constructs enums and pointers are prohibited
 *  2.) Floating point types dominate all other types
 *  3.) If an operation contains signed and unsigned types, unsigned wins
 *  4.) Larger bit count values dominate smaller ones
 *  5.) Modular operators always return a type of u_int64
 *
 * BNF Rule: <multiplicative-expression> ::= <cast-expression>{ (* | / | %) <cast-expression>}*
 */
static generic_ast_node_t* multiplicative_expression(FILE* fl){
	//For error printing
	char info[ERROR_SIZE];
	//Lookahead token
	Lexer_item lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;
	//Holding the return type
	generic_type_t* return_type;

	//No matter what, we do need to first see a valid cast expression expression
	generic_ast_node_t* sub_tree_root = cast_expression(fl);

	//Obvious fail case here
	if(sub_tree_root->CLASS == AST_NODE_CLASS_ERR_NODE){
		//If this is an error, we can just propogate it up
		return sub_tree_root;
	}

	//There are now two options. If we do not see any *'s or %'s or /, we just add 
	//this node in as the child and move along. But if we do see * or % or / symbols,
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	
	//As long as we have a multiplication operators(* or % or /) 
	while(lookahead.tok == MOD || lookahead.tok == STAR || lookahead.tok == F_SLASH){
		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//Off the bat, if we have a construct or enum or array type here, we can't add to it
		//Store this
		TYPE_CLASS temp_holder_type_class = temp_holder->inferred_type->type_class;

		//Fail case right here
		if(temp_holder_type_class != TYPE_CLASS_BASIC){
			sprintf(info, "Type %s is invalid for operators *, / and %s", temp_holder->inferred_type->type_name, "%");
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//We also are not allowed to see a void type here
		if(temp_holder->inferred_type->basic_type->basic_type == VOID){
			print_parse_message(PARSE_ERROR, "Void types are invalid for operators *, / and %", parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//If we saw a modulus operator and a float, that's also not allowed
		if(lookahead.tok == MOD){
			if(temp_holder->inferred_type->basic_type->basic_type == FLOAT32 || temp_holder->inferred_type->basic_type->basic_type == FLOAT64){
				sprintf(info, "Type %s is invalid for modulus operator", temp_holder->inferred_type->type_name);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}
		}

		//We now need to make an operator node
		sub_tree_root = ast_node_alloc(AST_NODE_CLASS_BINARY_EXPR);
		//We'll now assign the binary expression it's operator
		sub_tree_root->binary_operator = lookahead.tok;

		//We actually already know this guy's first child--it's the previous root currently
		//being held in temp_holder. We'll add the temp holder in as the subtree root
		add_child_node(sub_tree_root, temp_holder);

		//Now we have no choice but to see a valid cast expression again
		right_child = cast_expression(fl);

		//If it's an error, just fail out
		if(right_child->CLASS == AST_NODE_CLASS_ERR_NODE){
			//If this is an error we can just propogate it up
			return right_child;
		}

		//Let's now check to make sure that the right child also isn't some kind of disallowed
		TYPE_CLASS right_child_type_class = right_child->inferred_type->type_class;

		//Fail case right here
		if(right_child_type_class != TYPE_CLASS_BASIC){
			sprintf(info, "Type %s is invalid for operators *, / and %s", temp_holder->inferred_type->type_name, "%");
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//We also are not allowed to see a void type here
		if(right_child->inferred_type->basic_type->basic_type == VOID){
			print_parse_message(PARSE_ERROR, "Void types cannot be added to or subtracted from", parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//If we saw a modulus operator and a float, that's also not allowed
		if(lookahead.tok == MOD){
			if(right_child->inferred_type->basic_type->basic_type == FLOAT32 || right_child->inferred_type->basic_type->basic_type == FLOAT64){
				sprintf(info, "Type %s is invalid for modulus operator", right_child->inferred_type->type_name);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}
		}

		//Once we get here, we know that the type class is a basic type class for both
		Token temp_holder_type = temp_holder->inferred_type->basic_type->basic_type;
		Token right_child_type = right_child->inferred_type->basic_type->basic_type;

		//If the temp holder is a float64, the dominates so the return type will be too
		if(temp_holder_type == FLOAT64 || right_child_type == FLOAT64){
			return_type = temp_holder->inferred_type;

			//Jump out when done
			goto multiplicative_loop_end;
		}

		//Let's now check for type compatibility
		if(temp_holder_type == FLOAT32){
			//If we make it here then the final return type will be a float64
			if(right_child_type == U_INT64 || right_child_type == S_INT64){
				return_type = lookup_type(type_symtab, "f64")->type;
			//Otherwise the float dominates
			} else {
				return_type = temp_holder->inferred_type;
			}

			//Hop out of here now
			goto multiplicative_loop_end;
		}

		//Otherwise if the roles are reversed...
		//Let's now check for type compatibility
		if(right_child_type == FLOAT32){
			if(right_child_type == U_INT64 || right_child_type == S_INT64){
				return_type = lookup_type(type_symtab, "f64")->type;
			//Otherwise the float dominates
			} else {
				return_type = right_child->inferred_type;
			}
		
			//Hop out of here now
			goto multiplicative_loop_end;
		}

		//If we make it here we know that we have ints for types, so we'll check according to our int rules
		if(right_child_type == U_INT64 || temp_holder_type == U_INT64){
			//Return type is by default u_int64
			return_type = right_child->inferred_type;

			goto multiplicative_loop_end;
		}

		//If the temp holder is large and signed
		if(temp_holder_type == S_INT64){
			//If anything below this is unsigned, the whole thing becomes u_int64
			if(right_child_type == U_INT32 || right_child_type == U_INT16 || right_child_type == U_INT8){
				//Implicit case to unsigned
				return_type = lookup_type(type_symtab, "u64")->type;
			//Otherwise it's signed so the top level one will be signed
			} else {
				//Otherwise it's what temp holder had
				return_type = temp_holder->inferred_type;
			}

			goto multiplicative_loop_end;
		}

		//Now if the roles are reversed..
		if(right_child_type == S_INT64){
			//If anything below this is unsigned, the whole thing becomes u_int64
			if(temp_holder_type == U_INT32 || temp_holder_type == U_INT16 || temp_holder_type == U_INT8){
				//Implicit case to signed
				return_type = lookup_type(type_symtab, "u64")->type;
			//Otherwise it's signed so the top level one will be signed
			} else {
				//Otherwise it's what the right child had
				return_type = right_child->inferred_type;
			}

			goto multiplicative_loop_end;
		}

		//Now check for S-int32
		if(temp_holder_type == S_INT32){
			//If anything below this is unsigned, the whole thing becomes u_int64
			if(right_child_type == U_INT32 || right_child_type == U_INT16 || right_child_type == U_INT8){
				//Implicit case to signed
				return_type = lookup_type(type_symtab, "u32")->type;
			//Otherwise it's signed so the top level one will be signed
			} else {
				//Otherwise it's what the right child had
				return_type = right_child->inferred_type;
			}

			goto multiplicative_loop_end;
		}

		//Now check for S-int32
		if(right_child_type == S_INT32){
			//If anything below this is unsigned, the whole thing becomes u_int64
			if(temp_holder_type == U_INT32 || temp_holder_type == U_INT16 || temp_holder_type == U_INT8){
				//Implicit case to signed
				return_type = lookup_type(type_symtab, "u32")->type;
			//Otherwise it's signed so the top level one will be signed
			} else {
				//Otherwise it's what the right child had
				return_type = right_child->inferred_type;
			}

			goto multiplicative_loop_end;
		}

		//Now check for S-int16
		if(right_child_type == S_INT16){
			//If anything below this is unsigned, the whole thing becomes u_int64
			if(temp_holder_type == U_INT32){
				//Casted to unsigned
				return_type = temp_holder->inferred_type;
			} else if(temp_holder_type == U_INT16 || temp_holder_type == U_INT8){
				//Cast to unsigned
				return_type = lookup_type(type_symtab, "u16")->type;
			//Otherwise it's signed so the top level one will be signed
			} else {
				//Otherwise it's what the right child had
				return_type = right_child->inferred_type;
			}

			goto multiplicative_loop_end;
		}

		//Now check for S-int16
		if(temp_holder_type == S_INT16){
			//If anything below this is unsigned, the whole thing becomes u_int64
			if(right_child_type == U_INT32){
				//Casted to unsigned
				return_type = right_child->inferred_type;
			} else if(right_child_type == U_INT16 || right_child_type == U_INT8){
				//Cast to unsigned
				return_type = lookup_type(type_symtab, "u16")->type;
			//Otherwise it's signed so the top level one will be signed
			} else {
				//Otherwise it's what the right child had
				return_type = temp_holder->inferred_type;
			}

			goto multiplicative_loop_end;
		}

		//Now check for S-int8 and char(same thing)
		if(temp_holder_type == S_INT8 || temp_holder_type == CHAR){
			//If anything below this is unsigned, the whole thing becomes u_int64
			if(right_child_type == U_INT32 || right_child_type == U_INT16){
				//Casted to unsigned
				return_type = right_child->inferred_type;
			} else if(right_child_type == U_INT8){
				//Cast to unsigned
				return_type = lookup_type(type_symtab, "u8")->type;
			//Otherwise it's signed so the top level one will be signed
			} else {
				//Otherwise it's what the right child had
				return_type = temp_holder->inferred_type;
			}

			goto multiplicative_loop_end;
		}

		//Now check for S-int8 and char(same thing)
		if(right_child_type == S_INT8 || right_child_type == CHAR){
			//If anything below this is unsigned, the whole thing becomes u_int64
			if(temp_holder_type == U_INT32 || temp_holder_type == U_INT16){
				//Casted to unsigned
				return_type = temp_holder->inferred_type;
			} else if(right_child_type == U_INT8){
				//Cast to unsigned
				return_type = lookup_type(type_symtab, "u8")->type;
			//Otherwise it's signed so the top level one will be signed
			} else {
				//Otherwise it's what the right child had
				return_type = temp_holder->inferred_type;
			}

			goto multiplicative_loop_end;
		}
		
		//If we make it down here, and one of them is u_int32, then the ret type is u_int32
		if(right_child_type == U_INT32){
			return_type = right_child->inferred_type;
		} else if(temp_holder_type == U_INT32){
			return_type = temp_holder->inferred_type;
		} else if(right_child_type == U_INT16){
			return_type = right_child->inferred_type;
		} else if(temp_holder_type == U_INT16){
			return_type = temp_holder->inferred_type;
		} else if(right_child_type == U_INT8){
			return_type = right_child->inferred_type;
		} else {
			return_type = temp_holder->inferred_type;
		}

	//We end up here after all type checking
	multiplicative_loop_end:
		//Otherwise, he is the right child of the sub_tree_root, so we'll add it in
		add_child_node(sub_tree_root, right_child);

		//Assign the node type
		sub_tree_root->inferred_type = return_type;

		//By the end of this, we always have a proper subtree with the operator as the root, being held in 
		//"sub-tree root". We'll now refresh the token to keep looking
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	}

	//If we get here, it means that we did not see the token we need, so we are done. We'll put
	//the token back and return our subtree
	push_back_token(lookahead);
	//Store the line number
	sub_tree_root->line_number = parser_line_num;

	//We simply give back the sub tree root
	return sub_tree_root;
}


/**
 * Additive expressions can be chained like some of the other expressions that we see below. It is guaranteed
 * to return a pointer to a sub-tree, whether that subtree is created here or elsewhere.
 *
 * TYPE INFERENCE RULES: The return type here will be whichever type dominates. Dominance rules are as follows:
 * 	1.) Larger bit-count values dominate smaller ones
 * 	2.) Pointers dominate integers. Pointers and floating points are incompatible
 *  3.) Floating point numbers dominate integers
 *  4.) Unsigned will always dominate signed
 *
 * BNF Rule: <additive-expression> ::= <multiplicative-expression>{ (+ | -) <multiplicative-expression>}*
 */
static generic_ast_node_t* additive_expression(FILE* fl){
	//For error printing
	char info[ERROR_SIZE];
	//Lookahead token
	Lexer_item lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;
	//Hold the return type for us here
	generic_type_t* return_type;

	//No matter what, we do need to first see a valid multiplicative expression
	generic_ast_node_t* sub_tree_root = multiplicative_expression(fl);

	//Obvious fail case here
	if(sub_tree_root->CLASS == AST_NODE_CLASS_ERR_NODE){
		//If this is an error, we can just propogate it up
		return sub_tree_root;
	}
	
	//There are now two options. If we do not see any +'s or -'s, we just add 
	//this node in as the child and move along. But if we do see + or - symbols,
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	
	//As long as we have a relational operators(+ or -) 
	while(lookahead.tok == PLUS || lookahead.tok == MINUS){
		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//Off the bat, if we have a construct or enum or array type here, we can't add to it
		//Store this
		TYPE_CLASS temp_holder_type_class = temp_holder->inferred_type->type_class;

		//Fail case right here
		if(temp_holder_type_class == TYPE_CLASS_CONSTRUCT || temp_holder_type_class == TYPE_CLASS_ARRAY){
			sprintf(info, "Type %s cannot be added or subtracted from", temp_holder->inferred_type->type_name);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//We also are not allowed to see a void type here
		if(temp_holder_type_class == TYPE_CLASS_BASIC && temp_holder->inferred_type->basic_type->basic_type == VOID){
			print_parse_message(PARSE_ERROR, "Void types cannot be added to or subtracted from", parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//We now need to make an operator node
		sub_tree_root = ast_node_alloc(AST_NODE_CLASS_BINARY_EXPR);
		//We'll now assign the binary expression it's operator
		sub_tree_root->binary_operator = lookahead.tok;

		//We actually already know this guy's first child--it's the previous root currently
		//being held in temp_holder. We'll add the temp holder in as the subtree root
		add_child_node(sub_tree_root, temp_holder);

		//Now we have no choice but to see a valid multiplicative expression again
		right_child = multiplicative_expression(fl);

		//If it's an error, just fail out
		if(right_child->CLASS == AST_NODE_CLASS_ERR_NODE){
			//If this is an error we can just propogate it up
			return right_child;
		}

		//Let's now check to make sure that the right child also isn't some kind of disallowed
		TYPE_CLASS right_child_type_class = right_child->inferred_type->type_class;

		//Fail case right here
		if(right_child_type_class == TYPE_CLASS_CONSTRUCT || right_child_type_class == TYPE_CLASS_ARRAY){
			sprintf(info, "Type %s cannot be added or subtracted from", right_child->inferred_type->type_name);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//We also are not allowed to see a void type here
		if(right_child_type_class == TYPE_CLASS_BASIC && right_child->inferred_type->basic_type->basic_type == VOID){
			print_parse_message(PARSE_ERROR, "Void types cannot be added to or subtracted from", parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//If the temp holder is a pointer, the other one may not be a float of any kind
		if(temp_holder_type_class == TYPE_CLASS_POINTER){
			//One other basic check here. If the right child is also a pointer but they're different pointer types, we
			//can't add them
			if(right_child_type_class == TYPE_CLASS_POINTER){
				//Let's see if they're the exact same pointer type
				u_int8_t same = strcmp(temp_holder->inferred_type->type_name, right_child->inferred_type->type_name);

				//We're attempting to add different pointer types
				if(same != 0){
					sprintf(info, "Attempt to add differnet pointer types of %s and %s", temp_holder->inferred_type->type_name, right_child->inferred_type->type_name);
					print_parse_message(PARSE_ERROR, info, parser_line_num);
					num_errors++;
					return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
				}

				//Otherwise they are the same, but pointer addition is a real bad idea. We'll throw a warning
				sprintf(info, "Adding two pointers of type %s will likely lead to segmentation faults", right_child->inferred_type->type_name);
				print_parse_message(WARNING, info, parser_line_num);
				num_warnings++;

			//If it's a basic type
			} else if(right_child_type_class == TYPE_CLASS_BASIC){
				//We cannot add pointers and floating point numbers
				if(right_child->inferred_type->basic_type->basic_type == FLOAT32 
				  || right_child->inferred_type->basic_type->basic_type == FLOAT64){
					print_parse_message(PARSE_ERROR, "Floating point numbers and pointers cannot added together", parser_line_num);
					num_errors++;
					return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
				}
			}

			//If we make it all the way down here, we know that we have a pointer + int or pointer + pointer. Either way, 
			//the pointer will dominate
			return_type = temp_holder->inferred_type;

			//We've done all of our type checking, just jump out
			goto additive_loop_end;
		}

		//We'll just see what the other thing here says
		if(right_child_type_class == TYPE_CLASS_ENUMERATED || temp_holder_type_class == TYPE_CLASS_ENUMERATED){
			//Are they compatible?
			generic_type_t* result = types_compatible(temp_holder->inferred_type, right_child->inferred_type);

			//If no, this will be null
			if(result == NULL){
				sprintf(info, "Types \"%s\" and \"%s\" cannot be added together", temp_holder->inferred_type->type_name, right_child->inferred_type->type_name);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}

			//Otherwise the return type is this
			return_type = result;
			//Get out
			goto additive_loop_end;
		}

		//Now let's check and see if the right child is a pointer and the roles are reversed
		if(right_child_type_class == TYPE_CLASS_POINTER){
			//If it's a basic type
			if(temp_holder_type_class == TYPE_CLASS_BASIC){
				//We cannot add pointers and floating point numbers
				if(temp_holder->inferred_type->basic_type->basic_type == FLOAT32 
				  || temp_holder->inferred_type->basic_type->basic_type == FLOAT64){
					print_parse_message(PARSE_ERROR, "Floating point numbers and pointers cannot added together", parser_line_num);
					num_errors++;
					return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
				}
			}

			//If we make it all the way down here, we know that we have a pointer + int or pointer + pointer. Either way, 
			//the pointer will dominate
			return_type = right_child->inferred_type;

			//We've done all of our type checking, just get out
			goto additive_loop_end;
		}
		
		//Once we get here, we know that the type class is a basic type class for both
		Token temp_holder_type = temp_holder->inferred_type->basic_type->basic_type;
		Token right_child_type = right_child->inferred_type->basic_type->basic_type;

		//If the temp holder is a float64, the dominates so the return type will be too
		if(temp_holder_type == FLOAT64 || right_child_type == FLOAT64){
			return_type = temp_holder->inferred_type;
		}

		//Let's now check for type compatibility
		if(temp_holder_type == FLOAT32){
			//If we make it here then the final return type will be a float64
			if(right_child_type == U_INT64 || right_child_type == S_INT64){
				return_type = lookup_type(type_symtab, "f64")->type;
			//Otherwise the float dominates
			} else {
				return_type = temp_holder->inferred_type;
			}

			//Hop out of here now
			goto additive_loop_end;
		}

		//Otherwise if the roles are reversed...
		//Let's now check for type compatibility
		if(right_child_type == FLOAT32){
			if(right_child_type == U_INT64 || right_child_type == S_INT64){
				return_type = lookup_type(type_symtab, "f64")->type;
			//Otherwise the float dominates
			} else {
				return_type = right_child->inferred_type;
			}
		
			//Hop out of here now
			goto additive_loop_end;
		}

		//If we make it here we know that we have ints for types, so we'll check according to our int rules
		if(right_child_type == U_INT64 || temp_holder_type == U_INT64){
			//Return type is by default u_int64
			return_type = right_child->inferred_type;
			goto additive_loop_end;
		}

		//If the temp holder is large and signed
		if(temp_holder_type == S_INT64){
			//If anything below this is unsigned, the whole thing becomes u_int64
			if(right_child_type == U_INT32 || right_child_type == U_INT16 || right_child_type == U_INT8){
				//Implicit case to unsigned
				return_type = lookup_type(type_symtab, "u64")->type;
			//Otherwise it's signed so the top level one will be signed
			} else {
				//Otherwise it's what temp holder had
				return_type = temp_holder->inferred_type;
			}

			goto additive_loop_end;
		}

		//Now if the roles are reversed..
		if(right_child_type == S_INT64){
			//If anything below this is unsigned, the whole thing becomes u_int64
			if(temp_holder_type == U_INT32 || temp_holder_type == U_INT16 || temp_holder_type == U_INT8){
				//Implicit case to signed
				return_type = lookup_type(type_symtab, "u64")->type;
			//Otherwise it's signed so the top level one will be signed
			} else {
				//Otherwise it's what the right child had
				return_type = right_child->inferred_type;
			}

			goto additive_loop_end;
		}

		//Now check for S-int32
		if(temp_holder_type == S_INT32){
			//If anything below this is unsigned, the whole thing becomes u_int64
			if(right_child_type == U_INT32 || right_child_type == U_INT16 || right_child_type == U_INT8){
				//Implicit case to signed
				return_type = lookup_type(type_symtab, "u32")->type;
			//Otherwise it's signed so the top level one will be signed
			} else {
				//Otherwise it's what the right child had
				return_type = right_child->inferred_type;
			}

			goto additive_loop_end;
		}

		//Now check for S-int32
		if(right_child_type == S_INT32){
			//If anything below this is unsigned, the whole thing becomes u_int64
			if(temp_holder_type == U_INT32 || temp_holder_type == U_INT16 || temp_holder_type == U_INT8){
				//Implicit case to signed
				return_type = lookup_type(type_symtab, "u32")->type;
			//Otherwise it's signed so the top level one will be signed
			} else {
				//Otherwise it's what the right child had
				return_type = right_child->inferred_type;
			}

			goto additive_loop_end;
		}

		//Now check for S-int16
		if(right_child_type == S_INT16){
			//If anything below this is unsigned, the whole thing becomes u_int64
			if(temp_holder_type == U_INT32){
				//Casted to unsigned
				return_type = temp_holder->inferred_type;
			} else if(temp_holder_type == U_INT16 || temp_holder_type == U_INT8){
				//Cast to unsigned
				return_type = lookup_type(type_symtab, "u16")->type;
			//Otherwise it's signed so the top level one will be signed
			} else {
				//Otherwise it's what the right child had
				return_type = right_child->inferred_type;
			}

			goto additive_loop_end;
		}

		//Now check for S-int16
		if(temp_holder_type == S_INT16){
			//If anything below this is unsigned, the whole thing becomes u_int64
			if(right_child_type == U_INT32){
				//Casted to unsigned
				return_type = right_child->inferred_type;
			} else if(right_child_type == U_INT16 || right_child_type == U_INT8){
				//Cast to unsigned
				return_type = lookup_type(type_symtab, "u16")->type;
			//Otherwise it's signed so the top level one will be signed
			} else {
				//Otherwise it's what the right child had
				return_type = temp_holder->inferred_type;
			}

			goto additive_loop_end;
		}

		//Now check for S-int8 and char(same thing)
		if(temp_holder_type == S_INT8 || temp_holder_type == CHAR){
			//If anything below this is unsigned, the whole thing becomes u_int64
			if(right_child_type == U_INT32 || right_child_type == U_INT16){
				//Casted to unsigned
				return_type = right_child->inferred_type;
			} else if(right_child_type == U_INT8){
				//Cast to unsigned
				return_type = lookup_type(type_symtab, "u8")->type;
			//Otherwise it's signed so the top level one will be signed
			} else {
				//Otherwise it's what the right child had
				return_type = temp_holder->inferred_type;
			}

			goto additive_loop_end;
		}

		//Now check for S-int8 and char(same thing)
		if(right_child_type == S_INT8 || right_child_type == CHAR){
			//If anything below this is unsigned, the whole thing becomes u_int64
			if(temp_holder_type == U_INT32 || temp_holder_type == U_INT16){
				//Casted to unsigned
				return_type = temp_holder->inferred_type;
			} else if(right_child_type == U_INT8){
				//Cast to unsigned
				return_type = lookup_type(type_symtab, "u8")->type;
			//Otherwise it's signed so the top level one will be signed
			} else {
				//Otherwise it's what the right child had
				return_type = temp_holder->inferred_type;
			}

			goto additive_loop_end;
		}
		
		//If we make it down here, and one of them is u_int32, then the ret type is u_int32
		if(right_child_type == U_INT32){
			return_type = right_child->inferred_type;
		} else if(temp_holder_type == U_INT32){
			return_type = temp_holder->inferred_type;
		} else if(right_child_type == U_INT16){
			return_type = right_child->inferred_type;
		} else if(temp_holder_type == U_INT16){
			return_type = temp_holder->inferred_type;
		} else if(right_child_type == U_INT8){
			return_type = right_child->inferred_type;
		} else {
			return_type = temp_holder->inferred_type;
		}

	additive_loop_end:
		//Otherwise, he is the right child of the sub_tree_root, so we'll add it in
		add_child_node(sub_tree_root, right_child);
		
		//Now we can finally assign the sub tree type
		sub_tree_root->inferred_type = return_type;

		//By the end of this, we always have a proper subtree with the operator as the root, being held in 
		//"sub-tree root". We'll now refresh the token to keep looking
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	}

	//If we get here, it means that we did not see the token we need, so we are done. We'll put
	//the token back and return our subtree
	push_back_token(lookahead);
	//Store the line number
	sub_tree_root->line_number = parser_line_num;

	//We simply give back the sub tree root
	return sub_tree_root;
}


/**
 * A shift expression cannot be chained, so no recursion is needed here. It decays into an additive expression.
 * Just like other expression rules, a shift expression will return a subtree root, whether that subtree is 
 * rooted here or elsewhere
 * 
 * TYPE INFERENCE RULE: Shifting only works on integer types, and you can only shift by an integer amount
 *
 * BNF Rule: <shift-expression> ::= <additive-expression> 
 *								 |  <additive-expression> << <additive-expression> 
 *								 |  <additive-expression> >> <additive-expression>
 */
static generic_ast_node_t* shift_expression(FILE* fl){
	//For error printing
	char info[ERROR_SIZE];
	//Lookahead token
	Lexer_item lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;

	//No matter what, we do need to first see a valid additive expression
	generic_ast_node_t* sub_tree_root = additive_expression(fl);

	//Obvious fail case here
	if(sub_tree_root->CLASS == AST_NODE_CLASS_ERR_NODE){
		//If this is an error, we can just propogate it up
		return sub_tree_root;
	}
	
	//There are now two options. If we do not see any shift operators, we just add 
	//this node in as the child and move along. But if we do see shift operator symbols,
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	
	//We can optionally see some shift operators here
	if(lookahead.tok == L_SHIFT || lookahead.tok == R_SHIFT){
		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//Let's check the type of the temp holder. If it isn't a basic type, it's ruled out
		if(temp_holder->inferred_type->type_class != TYPE_CLASS_BASIC){
			sprintf(info, "Type %s cannot be bitwise shifted", temp_holder->inferred_type->type_name); 
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Even if it is a basic type, we need to ensure that it isn't a float or void.
		//Extract for convenience
		Token temp_holder_type = temp_holder->inferred_type->basic_type->basic_type;

		//We can't have floats or voids
		if(temp_holder_type == FLOAT32 || temp_holder_type == FLOAT64 || temp_holder_type == VOID){
			sprintf(info, "Type %s cannot be bitwise shifted", temp_holder->inferred_type->type_name);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//We now need to make an operator node
		sub_tree_root = ast_node_alloc(AST_NODE_CLASS_BINARY_EXPR);
		//We'll now assign the binary expression it's operator
		sub_tree_root->binary_operator = lookahead.tok;

		//We actually already know this guy's first child--it's the previous root currently
		//being held in temp_holder. We'll add the temp holder in as the subtree root
		add_child_node(sub_tree_root, temp_holder);

		//Now we have no choice but to see a valid additive expression again
		right_child = additive_expression(fl);

		//If it's an error, just fail out
		if(right_child->CLASS == AST_NODE_CLASS_ERR_NODE){
			//If this is an error we can just propogate it up
			return right_child;
		}

		//Let's check the type of the right child. If it isn't a basic type, it's ruled out
		if(right_child->inferred_type->type_class != TYPE_CLASS_BASIC){
			sprintf(info, "Type %s cannot be used as a shift amount", right_child->inferred_type->type_name); 
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Even if it is a basic type, we need to ensure that it isn't a float or void.
		//Extract for convenience
		Token right_child_type = right_child->inferred_type->basic_type->basic_type;

		//We can't have floats or voids
		if(right_child_type == FLOAT32 || right_child_type == FLOAT64 || right_child_type == VOID){
			sprintf(info, "Type %s cannot be used as a shift amount", right_child->inferred_type->type_name);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}
		
		//Additionally, if it's a negative shift amount, we'll throw a warning
		if(right_child_type == S_INT8 || right_child_type == S_INT16 || right_child_type == S_INT32 
		   || right_child_type == S_INT64){
			print_parse_message(WARNING, "Negative shift amounts will be treated as unsigned. Highly advised against using", parser_line_num);
		}

		//Otherwise, he is the right child of the sub_tree_root, so we'll add it in
		add_child_node(sub_tree_root, right_child);
		//The return type is always the left child's type
		sub_tree_root->inferred_type = temp_holder->inferred_type;

	} else {
		//Otherwise just push the token back
		push_back_token(lookahead);
	}

	//Once we make it here, the subtree root is either just the shift expression or it is the
	//shift expression rooted at the relational operator
	//Store the line number
	sub_tree_root->line_number = parser_line_num;

	//We simply give back the sub tree root
	return sub_tree_root;
}


/**
 * A relational expression will descend into a shift expression. Ollie language does not allow for
 * chaining in relational expressions, so there will be no while loop like other rules. Just like
 * other expression rules, a relational expression will return a subtree, whether that subtree
 * is made here or elsewhere
 *
 * TYPE INFERENCE RULES: Relational expressions work on anything with the exception of arrays, constructs and
 * enum types. A relational expression always returns a value of u_int8(boolean)
 *
 * <relational-expression> ::= <shift-expression> 
 * 						     | <shift-expression> > <shift-expression> 
 * 						     | <shift-expression> < <shift-expression> 
 * 						     | <shift-expression> >= <shift-expression> 
 * 						     | <shift-expression> <= <shift-expression>
 */
static generic_ast_node_t* relational_expression(FILE* fl){
	//For error printing
	char info[ERROR_SIZE];
	//Lookahead token
	Lexer_item lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;
	//For hold the return type: always u_int8
	generic_type_t* rel_expr_ret_type = lookup_type(type_symtab, "u8")->type;

	//No matter what, we do need to first see a valid shift expression
	generic_ast_node_t* sub_tree_root = shift_expression(fl);

	//Obvious fail case here
	if(sub_tree_root->CLASS == AST_NODE_CLASS_ERR_NODE){
		//If this is an error, we can just propogate it up
		return sub_tree_root;
	}
	
	//There are now two options. If we do not see any relational operators, we just add 
	//this node in as the child and move along. But if we do see relational operator symbols,
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	
	//If we have relational operators here
	if(lookahead.tok == G_THAN || lookahead.tok == L_THAN || lookahead.tok == G_THAN_OR_EQ
	   || lookahead.tok == L_THAN_OR_EQ){
		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//We now need to make an operator node
		sub_tree_root = ast_node_alloc(AST_NODE_CLASS_BINARY_EXPR);
		//We'll now assign the binary expression it's operator
		sub_tree_root->binary_operator = lookahead.tok;

		//We actually already know this guy's first child--it's the previous root currently
		//being held in temp_holder. We'll add the temp holder in as the subtree root
		add_child_node(sub_tree_root, temp_holder);

		//Now we have no choice but to see a valid shift again
		right_child = shift_expression(fl);

		//If it's an error, just fail out
		if(right_child->CLASS == AST_NODE_CLASS_ERR_NODE){
			//If this is an error we can just propogate it up
			return right_child;
		}

		/**
		 * We now must do type-legality checking. Inclusive or works
		 * except for constructs, arrays and enums
		 */
		generic_type_t* temp_holder_type = temp_holder->inferred_type;
		generic_type_t* right_child_type = right_child->inferred_type;
		
		//We do not allow bitwise or to be done on arrays, enums or constructs
		if(temp_holder_type->type_class == TYPE_CLASS_ARRAY || temp_holder_type->type_class == TYPE_CLASS_CONSTRUCT
		  || right_child_type->type_class == TYPE_CLASS_ARRAY || right_child_type->type_class == TYPE_CLASS_CONSTRUCT
		  || right_child_type->type_class == TYPE_CLASS_ENUMERATED || temp_holder_type->type_class == TYPE_CLASS_ENUMERATED){
			print_parse_message(PARSE_ERROR, "Relational operators do not work with arrays, enums or constructs", parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Additionally, we cannot use logical or with floats or voids
		//Check the first node
		if(temp_holder_type->type_class == TYPE_CLASS_BASIC){
			//We do not allow the use of void types here
			if(temp_holder_type->basic_type->basic_type == VOID){
				sprintf(info, "Attempt to compare incompatible types %s and %s", temp_holder_type->type_name, right_child_type->type_name); 
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}
		}

		//Check the second node
		if(right_child_type->type_class == TYPE_CLASS_BASIC){
			//We do not allow the use of void types here
			if(right_child_type->basic_type->basic_type == VOID){
				sprintf(info, "Attempt to compare incompatible types %s and %s", temp_holder_type->type_name, right_child_type->type_name); 
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}
		}

		//Store what the type of this operation is
		sub_tree_root->inferred_type = rel_expr_ret_type;

		//Otherwise, he is the right child of the sub_tree_root, so we'll add it in
		add_child_node(sub_tree_root, right_child);
	} else {
		//Otherwise just push the token back
		push_back_token(lookahead);
	}

	//Once we make it here, the subtree root is either just the shift expression or it is the
	//shift expression rooted at the relational operator
	//Store the line number
	sub_tree_root->line_number = parser_line_num;

	//We simply give back the sub tree root
	return sub_tree_root;
}


/**
 * An equality expression can be chained and descends into a relational expression. It will
 * always return a pointer to the subtree, whether that subtree is made here or elsewhere
 *
 * TYPE INFERENCE RULES: An equality expression is guaranteed to return a type of u_int8(boolean).
 * It can take in as input anything besides an array, construct, enum or void
 *
 * BNF Rule: <equality-expression> ::= <relational-expression>{ (==|!=) <relational-expression> }*
 */
static generic_ast_node_t* equality_expression(FILE* fl){
	//For error printing
	char info[ERROR_SIZE];
	//Lookahead token
	Lexer_item lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;
	//Grab and hold what we'll be returning
	generic_type_t* equality_expr_ret_type = lookup_type(type_symtab, "u8")->type;

	//No matter what, we do need to first see a valid relational expression
	generic_ast_node_t* sub_tree_root = relational_expression(fl);

	//Obvious fail case here
	if(sub_tree_root->CLASS == AST_NODE_CLASS_ERR_NODE){
		//If this is an error, we can just propogate it up
		return sub_tree_root;
	}
	
	//There are now two options. If we do not see any =='s or !='s, we just add 
	//this node in as the child and move along. But if we do see == or != symbols,
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	
	//As long as we have a relational operators(== or !=) 
	while(lookahead.tok == NOT_EQUALS || lookahead.tok == D_EQUALS){
		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//We now need to make an operator node
		sub_tree_root = ast_node_alloc(AST_NODE_CLASS_BINARY_EXPR);
		//We'll now assign the binary expression it's operator
		sub_tree_root->binary_operator = lookahead.tok;

		//We actually already know this guy's first child--it's the previous root currently
		//being held in temp_holder. We'll add the temp holder in as the subtree root
		add_child_node(sub_tree_root, temp_holder);

		//Now we have no choice but to see a valid relational expression again
		right_child = relational_expression(fl);

		//If it's an error, just fail out
		if(right_child->CLASS == AST_NODE_CLASS_ERR_NODE){
			//If this is an error we can just propogate it up
			return right_child;
		}

		/**
		 * We now must do type-legality checking. Inclusive or works
		 * except for constructs, arrays and enums
		 */
		generic_type_t* temp_holder_type = temp_holder->inferred_type;
		generic_type_t* right_child_type = right_child->inferred_type;
		
		//We do not allow bitwise or to be done on arrays, enums or constructs
		if(temp_holder_type->type_class == TYPE_CLASS_ARRAY || temp_holder_type->type_class == TYPE_CLASS_CONSTRUCT
		  || right_child_type->type_class == TYPE_CLASS_ARRAY || right_child_type->type_class == TYPE_CLASS_CONSTRUCT){
			print_parse_message(PARSE_ERROR, "Equality operators do not work with arrays or constructs", parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Additionally, we cannot use logical or with floats or voids
		//Check the first node
		if(temp_holder_type->type_class == TYPE_CLASS_BASIC){
			//We do not allow the use of void types here
			if(temp_holder_type->basic_type->basic_type == VOID){
				sprintf(info, "Attempt to compare incompatible types %s and %s", temp_holder_type->type_name, right_child_type->type_name); 
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}
		}

		//Check the second node
		if(right_child_type->type_class == TYPE_CLASS_BASIC){
			//We do not allow the use of void types here
			if(right_child_type->basic_type->basic_type == VOID){
				sprintf(info, "Attempt to compare incompatible types %s and %s", temp_holder_type->type_name, right_child_type->type_name); 
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}
		}

		//Otherwise, he is the right child of the sub_tree_root, so we'll add it in
		add_child_node(sub_tree_root, right_child);

		//Store what our return type is too
		sub_tree_root->inferred_type = equality_expr_ret_type;

		//By the end of this, we always have a proper subtree with the operator as the root, being held in 
		//"sub-tree root". We'll now refresh the token to keep looking
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	}

	//If we get here, it means that we did not see the "DOUBLE AND" token, so we are done. We'll put
	//the token back and return our subtree
	push_back_token(lookahead);
	//Store the line number
	sub_tree_root->line_number = parser_line_num;

	//We simply give back the sub tree root
	return sub_tree_root;
}


/**
 * An and-expression descends into an equality expression and can be chained. This function
 * will always return a pointer to the root of the subtree, whether that subtree is made here or
 * at a rule lower down on the tree
 *
 * TYPE INFERENCE Rules: And expressions are only legal on integer, pointer or char types. All other
 * types will result in a failure. An and expression, if it actually happens, always returns a type of
 * s_int64
 *
 * BNF Rule: <and-expression> ::= <equality-expression>{& <equality-expression>}* 
 */
static generic_ast_node_t* and_expression(FILE* fl){
	//For error printing
	char info[ERROR_SIZE];
	//Lookahead token
	Lexer_item lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;

	//No matter what, we do need to first see a valid equality expression
	generic_ast_node_t* sub_tree_root = equality_expression(fl);

	//Obvious fail case here
	if(sub_tree_root->CLASS == AST_NODE_CLASS_ERR_NODE){
		//If this is an error, we can just propogate it up
		return sub_tree_root;
	}
	
	//There are now two options. If we do not see any ^'s, we just add 
	//this node in as the child and move along. But if we do see ^ symbols, 
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	
	//As long as we have a single and(&) 
	while(lookahead.tok == AND){
		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//We now need to make an operator node
		sub_tree_root = ast_node_alloc(AST_NODE_CLASS_BINARY_EXPR);
		//We'll now assign the binary expression it's operator
		sub_tree_root->binary_operator = lookahead.tok;

		//We actually already know this guy's first child--it's the previous root currently
		//being held in temp_holder. We'll add the temp holder in as the subtree root
		add_child_node(sub_tree_root, temp_holder);

		//Now we have no choice but to see a valid equality expression again
		right_child = equality_expression(fl);

		//If it's an error, just fail out
		if(right_child->CLASS == AST_NODE_CLASS_ERR_NODE){
			//If this is an error we can just propogate it up
			return right_child;
		}

		/**
		 * We now must do type-legality checking. Inclusive or works
		 * except for constructs, arrays and enums
		 */
		generic_type_t* temp_holder_type = temp_holder->inferred_type;
		generic_type_t* right_child_type = right_child->inferred_type;
		
		//We do not allow bitwise or to be done on arrays, enums or constructs
		if(temp_holder_type->type_class == TYPE_CLASS_ARRAY || temp_holder_type->type_class == TYPE_CLASS_CONSTRUCT
		  || right_child_type->type_class == TYPE_CLASS_ARRAY || right_child_type->type_class == TYPE_CLASS_CONSTRUCT
		  || right_child_type->type_class == TYPE_CLASS_ENUMERATED || temp_holder_type->type_class == TYPE_CLASS_ENUMERATED){
			print_parse_message(PARSE_ERROR, "Bitwise and operator does not work with arrays, enums or constructs", parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Additionally, we cannot use logical or with floats or voids
		//Check the first node
		if(temp_holder_type->type_class == TYPE_CLASS_BASIC){
			if(temp_holder_type->basic_type->basic_type == FLOAT32
			  || temp_holder_type->basic_type->basic_type == FLOAT64
			  || temp_holder_type->basic_type->basic_type == VOID){
				sprintf(info, "Attempt to bitwise and incompatible types %s and %s", temp_holder_type->type_name, right_child_type->type_name); 
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}
		}

		//Check the second node
		if(right_child_type->type_class == TYPE_CLASS_BASIC){
			if(right_child_type->basic_type->basic_type == FLOAT32
			  || right_child_type->basic_type->basic_type == FLOAT64
			  || right_child_type->basic_type->basic_type == VOID){
				sprintf(info, "Attempt to bitwise and incompatible types %s and %s", temp_holder_type->type_name, right_child_type->type_name); 
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}
		}

		//Otherwise, he is the right child of the sub_tree_root, so we'll add it in
		add_child_node(sub_tree_root, right_child);

		//Make sure we mark the root node's return type after it's been anded
		sub_tree_root->inferred_type = temp_holder_type;

		//By the end of this, we always have a proper subtree with the operator as the root, being held in 
		//"sub-tree root". We'll now refresh the token to keep looking
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	}

	//If we get here, it means that we did not see the "DOUBLE AND" token, so we are done. We'll put
	//the token back and return our subtree
	push_back_token(lookahead);
	//Store the line number
	sub_tree_root->line_number = parser_line_num;

	//We simply give back the sub tree root
	return sub_tree_root;
}


/**
 * An exclusive or expression can be chained, and descends into an and-expression. It will always return
 * a node pointer to the root of the subtree, whether that subtree is made here or in a rule lower down
 * the chain
 *
 * An exclusive or expression always returns a type of a signed 64 bit integer. We do not allow the use
 * of this operator on void, float, construct or enum types
 *
 * BNF Rule: <exclusive-or-expression> ::= <and-expression>{^ <and-expression}*
 */
static generic_ast_node_t* exclusive_or_expression(FILE* fl){
	//For error printing
	char info[ERROR_SIZE];
	//Lookahead token
	Lexer_item lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;

	//No matter what, we do need to first see a valid and expression
	generic_ast_node_t* sub_tree_root = and_expression(fl);

	//Obvious fail case here
	if(sub_tree_root->CLASS == AST_NODE_CLASS_ERR_NODE){
		//If this is an error, we can just propogate it up
		return sub_tree_root;
	}
	
	//There are now two options. If we do not see any ^'s, we just add 
	//this node in as the child and move along. But if we do see ^ symbols, 
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	
	//As long as we have a single xor(^)
	while(lookahead.tok == CARROT){
		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//We now need to make an operator node
		sub_tree_root = ast_node_alloc(AST_NODE_CLASS_BINARY_EXPR);
		//We'll now assign the binary expression it's operator
		sub_tree_root->binary_operator = lookahead.tok;
		//TODO handle type stuff later on

		//We actually already know this guy's first child--it's the previous root currently
		//being held in temp_holder. We'll add the temp holder in as the subtree root
		add_child_node(sub_tree_root, temp_holder);

		//Now we have no choice but to see a valid and expression again
		right_child = and_expression(fl);

		//If it's an error, just fail out
		if(right_child->CLASS == AST_NODE_CLASS_ERR_NODE){
			//If this is an error we can just propogate it up
			return right_child;
		}

		/**
		 * We now must do type-legality checking. Inclusive or works
		 * except for constructs, arrays and enums
		 */
		generic_type_t* temp_holder_type = temp_holder->inferred_type;
		generic_type_t* right_child_type = right_child->inferred_type;
		
		//We do not allow bitwise or to be done on arrays, enums or constructs
		if(temp_holder_type->type_class == TYPE_CLASS_ARRAY || temp_holder_type->type_class == TYPE_CLASS_CONSTRUCT
		  || right_child_type->type_class == TYPE_CLASS_ARRAY || right_child_type->type_class == TYPE_CLASS_CONSTRUCT
		  || right_child_type->type_class == TYPE_CLASS_ENUMERATED || temp_holder_type->type_class == TYPE_CLASS_ENUMERATED){
			print_parse_message(PARSE_ERROR, "Exclusive or operator does not work with arrays, enums or constructs", parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Additionally, we cannot use logical or with floats or voids
		//Check the first node
		if(temp_holder_type->type_class == TYPE_CLASS_BASIC){
			if(temp_holder_type->basic_type->basic_type == FLOAT32
			  || temp_holder_type->basic_type->basic_type == FLOAT64
			  || temp_holder_type->basic_type->basic_type == VOID){
				sprintf(info, "Attempt to exclusive-or incompatible types %s and %s", temp_holder_type->type_name, right_child_type->type_name); 
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}
		}

		//Check the second node
		if(right_child_type->type_class == TYPE_CLASS_BASIC){
			if(right_child_type->basic_type->basic_type == FLOAT32
			  || right_child_type->basic_type->basic_type == FLOAT64
			  || right_child_type->basic_type->basic_type == VOID){
				sprintf(info, "Attempt to exclusive-or incompatible types %s and %s", temp_holder_type->type_name, right_child_type->type_name); 
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}
		}

		//Otherwise, he is the right child of the sub_tree_root, so we'll add it in
		add_child_node(sub_tree_root, right_child);

		//Ensure that we denote what type this subtree is now
		sub_tree_root->inferred_type = temp_holder_type;

		//By the end of this, we always have a proper subtree with the operator as the root, being held in 
		//"sub-tree root". We'll now refresh the token to keep looking
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	}

	//If we get here, it means that we did not see the "DOUBLE AND" token, so we are done. We'll put
	//the token back and return our subtree
	push_back_token(lookahead);
	//Store the line number
	sub_tree_root->line_number = parser_line_num;

	//We simply give back the sub tree root
	return sub_tree_root;
}


/**
 * An inclusive or expression will always return a reference to the root node of it's subtree. That node
 * could be an operator or it could be a passthrough
 *
 * Type inference rule here: Inclusive or expression is valid on everything except for arrays, constructs and
 * enums. We always return a type of s_int64 as a result
 *
 * BNF rule: <inclusive-or-expression> ::= <exclusive-or-expression>{ | <exclusive-or-expression>}*
 */
static generic_ast_node_t* inclusive_or_expression(FILE* fl){
	//For error printing
	char info[ERROR_SIZE];
	//Lookahead token
	Lexer_item lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;
	//Hold the return type if we need it
	generic_type_t* i_or_ret_type = lookup_type(type_symtab, "i64")->type;

	//No matter what, we do need to first see a valid exclusive or expression
	generic_ast_node_t* sub_tree_root = exclusive_or_expression(fl);

	//Obvious fail case here
	if(sub_tree_root->CLASS == AST_NODE_CLASS_ERR_NODE){
		//If this is an error, we can just propogate it up
		return sub_tree_root;
	}
	
	//There are now two options. If we do not see any |'s, we just add 
	//this node in as the child and move along. But if we do see | symbols, 
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	
	//As long as we have a single or(|)
	while(lookahead.tok == OR){
		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//We now need to make an operator node
		sub_tree_root = ast_node_alloc(AST_NODE_CLASS_BINARY_EXPR);
		//We'll now assign the binary expression it's operator
		sub_tree_root->binary_operator = lookahead.tok;

		//We actually already know this guy's first child--it's the previous root currently
		//being held in temp_holder. We'll add the temp holder in as the subtree root
		add_child_node(sub_tree_root, temp_holder);

		//Now we have no choice but to see a valid exclusive or expression again
		right_child = exclusive_or_expression(fl);

		//If it's an error, just fail out
		if(right_child->CLASS == AST_NODE_CLASS_ERR_NODE){
			//If this is an error we can just propogate it up
			return right_child;
		}

		/**
		 * We now must do type-legality checking. Inclusive or works
		 * except for constructs, arrays and enums
		 */
		generic_type_t* temp_holder_type = temp_holder->inferred_type;
		generic_type_t* right_child_type = right_child->inferred_type;
		
		//We do not allow bitwise or to be done on arrays, enums or constructs
		if(temp_holder_type->type_class == TYPE_CLASS_ARRAY || temp_holder_type->type_class == TYPE_CLASS_CONSTRUCT
		  || right_child_type->type_class == TYPE_CLASS_ARRAY || right_child_type->type_class == TYPE_CLASS_CONSTRUCT
		  || right_child_type->type_class == TYPE_CLASS_ENUMERATED || temp_holder_type->type_class == TYPE_CLASS_ENUMERATED){
			print_parse_message(PARSE_ERROR, "Bitwise or operator does not work with arrays, enums or constructs", parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Additionally, we cannot use logical or with floats or voids
		//Check the first node
		if(temp_holder_type->type_class == TYPE_CLASS_BASIC){
			if(temp_holder_type->basic_type->basic_type == FLOAT32
			  || temp_holder_type->basic_type->basic_type == FLOAT64
			  || temp_holder_type->basic_type->basic_type == VOID){
				sprintf(info, "Attempt to bitwise-or incompatible types %s and %s", temp_holder_type->type_name, right_child_type->type_name); 
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}
		}

		//Check the second node
		if(right_child_type->type_class == TYPE_CLASS_BASIC){
			if(right_child_type->basic_type->basic_type == FLOAT32
			  || right_child_type->basic_type->basic_type == FLOAT64
			  || right_child_type->basic_type->basic_type == VOID){
				sprintf(info, "Attempt to bitwise-or incompatible types %s and %s", temp_holder_type->type_name, right_child_type->type_name); 
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}
		}

		//Otherwise, he is the right child of the sub_tree_root, so we'll add it in
		add_child_node(sub_tree_root, right_child);

		//Add what the type is in here(always an s_int64)
		sub_tree_root->inferred_type = i_or_ret_type;

		//By the end of this, we always have a proper subtree with the operator as the root, being held in 
		//"sub-tree root". We'll now refresh the token to keep looking
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	}

	//If we get here, it means that we did not see the "DOUBLE AND" token, so we are done. We'll put
	//the token back and return our subtree
	push_back_token(lookahead);
	//Store the line number
	sub_tree_root->line_number = parser_line_num;

	//We simply give back the sub tree root
	return sub_tree_root;
}


/**
 * A logical-and-expression will always return a reference to the root node of its subtree. That
 * root node can very well be an operator or it could just be a pass through
 *
 * Type inference rule here: If we don't see any &&, we have whatever type the root has. Otherwise,
 * we return a type of u_int8(boolean value)
 *
 * BNF Rule: <logical-and-expression> ::= <inclusive-or-expression>{&&<inclusive-or-expression>}*
 */
static generic_ast_node_t* logical_and_expression(FILE* fl){
	//For error printing
	char info[ERROR_SIZE];
	//Lookahead token
	Lexer_item lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;
	//Grab and hold this just in case we need it later
	generic_type_t* l_and_ret_type = lookup_type(type_symtab, "u8")->type;

	//No matter what, we do need to first see a valid inclusive or expression
	generic_ast_node_t* sub_tree_root = inclusive_or_expression(fl);

	//Obvious fail case here
	if(sub_tree_root->CLASS == AST_NODE_CLASS_ERR_NODE){
		//If this is an error, we can just propogate it up
		return sub_tree_root;
	}
	
	//There are now two options. If we do not see any &&'s, we just add 
	//this node in as the child and move along. But if we do see && symbols, 
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//As long as we have a double and 
	while(lookahead.tok == DOUBLE_AND){
		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//We now need to make an operator node
		sub_tree_root = ast_node_alloc(AST_NODE_CLASS_BINARY_EXPR);
		//We'll now assign the binary expression it's operator
		sub_tree_root->binary_operator = lookahead.tok;

		//We actually already know this guy's first child--it's the previous root currently
		//being held in temp_holder. We'll add the temp holder in as the subtree root
		add_child_node(sub_tree_root, temp_holder);

		//Now we have no choice but to see a valid inclusive or expression again
		right_child = inclusive_or_expression(fl);

		//If it's an error, just fail out
		if(right_child->CLASS == AST_NODE_CLASS_ERR_NODE){
			//If this is an error we can just propogate it up
			return right_child;
		}

		/**
		 * We now must do type-legality checking. Logical and works on basically everything
		 * except for constructs, arrays and enums
		 */
		generic_type_t* temp_holder_type = temp_holder->inferred_type;
		generic_type_t* right_child_type = right_child->inferred_type;
		
		//We do not allow logical or to be done on arrays, enums or constructs
		if(temp_holder_type->type_class == TYPE_CLASS_ARRAY || temp_holder_type->type_class == TYPE_CLASS_CONSTRUCT
		  || right_child_type->type_class == TYPE_CLASS_ARRAY || right_child_type->type_class == TYPE_CLASS_CONSTRUCT
		  || right_child_type->type_class == TYPE_CLASS_ENUMERATED || temp_holder_type->type_class == TYPE_CLASS_ENUMERATED){
			sprintf(info, "Attempt to logically-and incompatible types %s and %s", temp_holder_type->type_name, right_child_type->type_name); 
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Additionally, we cannot use logical or with floats or voids
		//Check the first node
		if(temp_holder_type->type_class == TYPE_CLASS_BASIC){
			if(temp_holder_type->basic_type->basic_type == FLOAT32
			  || temp_holder_type->basic_type->basic_type == FLOAT64
			  || temp_holder_type->basic_type->basic_type == VOID){
				sprintf(info, "Attempt to logically-and incompatible types %s and %s", temp_holder_type->type_name, right_child_type->type_name); 
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}
		}

		//Check the second node
		if(right_child_type->type_class == TYPE_CLASS_BASIC){
			if(right_child_type->basic_type->basic_type == FLOAT32
			  || right_child_type->basic_type->basic_type == FLOAT64
			  || right_child_type->basic_type->basic_type == VOID){
				sprintf(info, "Attempt to logically-and types incompatible types %s and %s", temp_holder_type->type_name, right_child_type->type_name); 
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}
		}

		//Otherwise, he is the right child of the sub_tree_root, so we'll add it in
		add_child_node(sub_tree_root, right_child);

		//We now know that the subtree root has a type of u_int8(boolean)
		sub_tree_root->inferred_type = l_and_ret_type;

		//By the end of this, we always have a proper subtree with the operator as the root, being held in 
		//"sub-tree root". We'll now refresh the token to keep looking
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	}
	
	//If we get here, it means that we did not see the "DOUBLE AND" token, so we are done. We'll put
	//the token back and return our subtree
	push_back_token(lookahead);
	//Store the line number
	sub_tree_root->line_number = parser_line_num;

	//We simply give back the sub tree root
	return sub_tree_root;
}


/**
 * A logical or expression can be chained together as many times as we want, and
 * descends into a logical and expression
 *
 * This will always return a reference to the root node of the tree
 *
 * Type inference rules here: If we see one or more || symbols, we will return a type of u_int8(boolean).
 * Otherwise, we return a type of whatever the first rule was. Logical or does not work on constructs, enums,
 * arrays and floats and void
 *
 * BNF Rule: <logical-or-expression> ::= <logical-and-expression>{||<logical-and-expression>}*
 */
static generic_ast_node_t* logical_or_expression(FILE* fl){
	//For error printing
	char info[ERROR_SIZE];
	//Lookahead token
	Lexer_item lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;
	//Hold this type just in case we need it
	generic_type_t* l_or_ret_type = lookup_type(type_symtab, "u8")->type;

	//No matter what, we do need to first see a logical and expression
	generic_ast_node_t* sub_tree_root = logical_and_expression(fl);

	//Obvious fail case here
	if(sub_tree_root->CLASS == AST_NODE_CLASS_ERR_NODE){
		//It's already an error node, so allow it to propogate
		return sub_tree_root;
	}
	
	//There are now two options. If we do not see any ||'s, we just add 
	//this node in as the child and move along. But if we do see || symbols, 
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	
	//As long as we have a double or
	while(lookahead.tok == DOUBLE_OR){
		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//We now need to make an operator node
		sub_tree_root = ast_node_alloc(AST_NODE_CLASS_BINARY_EXPR);
		//We'll now assign the binary expression it's operator
		sub_tree_root->binary_operator = lookahead.tok;

		//We actually already know this guy's first child--it's the previous root currently
		//being held in temp_holder. We'll add the temp holder in as the subtree root
		add_child_node(sub_tree_root, temp_holder);

		//Now we have no choice but to see a valid logical and expression again
		right_child = logical_and_expression(fl);

		//If it's an error, just fail out
		if(right_child->CLASS == AST_NODE_CLASS_ERR_NODE){
			//It's already an error node, so allow it to propogate
			return right_child;
		}

		/**
		 * We now must do type-legality checking. Logical or works on basically everything
		 * except for constructs, arrays and enums
		 */
		generic_type_t* temp_holder_type = temp_holder->inferred_type;
		generic_type_t* right_child_type = right_child->inferred_type;
		
		//We do not allow logical or to be done on arrays, enums or constructs
		if(temp_holder_type->type_class == TYPE_CLASS_ARRAY || temp_holder_type->type_class == TYPE_CLASS_CONSTRUCT
		  || right_child_type->type_class == TYPE_CLASS_ARRAY || right_child_type->type_class == TYPE_CLASS_CONSTRUCT
		  || right_child_type->type_class == TYPE_CLASS_ENUMERATED || temp_holder_type->type_class == TYPE_CLASS_ENUMERATED){
			sprintf(info, "Attempt to logically-or(||) incompatible types %s and %s", temp_holder_type->type_name, right_child_type->type_name); 
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Additionally, we cannot use logical or with floats or voids
		//Check the first node
		if(temp_holder_type->type_class == TYPE_CLASS_BASIC){
			if(temp_holder_type->basic_type->basic_type == FLOAT32
			  || temp_holder_type->basic_type->basic_type == FLOAT64
			  || temp_holder_type->basic_type->basic_type == VOID){
				sprintf(info, "Attempt to logically-or types incompatible types %s and %s", temp_holder_type->type_name, right_child_type->type_name); 
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}
		}

		//Check the second node
		if(right_child_type->type_class == TYPE_CLASS_BASIC){
			if(right_child_type->basic_type->basic_type == FLOAT32
			  || right_child_type->basic_type->basic_type == FLOAT64
			  || right_child_type->basic_type->basic_type == VOID){
				sprintf(info, "Attempt to logically-or types incompatible types %s and %s", temp_holder_type->type_name, right_child_type->type_name); 
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}
		}

		//If we make it all the way down here, we know that we have valid types for our logical or operation

		//Otherwise, he is the right child of the sub_tree_root, so we'll add it in
		add_child_node(sub_tree_root, right_child);
		
		//This means that the sub-tree-root has a type of u_int8(boolean value)
		sub_tree_root->inferred_type = l_or_ret_type;

		//By the end of this, we always have a proper subtree with the operator as the root, being held in 
		//"sub-tree root". We'll now refresh the token to keep looking
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	}

	//If we get here, it means that we did not see the "DOUBLE OR" token, so we are done. We'll put
	//the token back and add this in as a subtree of the parent
	push_back_token(lookahead);
	//Store the line number
	sub_tree_root->line_number = parser_line_num;

	//Return the reference to the root node
	return sub_tree_root;
}


/**
 * A construct member is something like a variable declaration. Like all rules in the parser,
 * the construct member will return a reference to the root node of the subtree it creates
 *
 * As a reminder, type specifier will give us an error if the type is not defined
 *
 * BNF Rule: <construct-member> ::= <identifier> : <type-specifier> 
 */
static generic_ast_node_t* construct_member(FILE* fl){
	//The error printing string
	char info[ERROR_SIZE];
	//The lookahead token
	Lexer_item lookahead;

	//Otherwise we know that it worked here
	//Now we need to see a valid ident and check it for duplication
	generic_ast_node_t* ident = identifier(fl);	

	//Let's make sure it actually worked
	if(ident->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid identifier given as construct member name", parser_line_num);
		num_errors++;
		//It's an error, so we'll propogate it up
		return ident;
	}

	//Grab this for convenience
	char* name = ident->identifier;

	//Array bounds checking real quick
	if(strlen(name) > MAX_TYPE_NAME_LENGTH){
		sprintf(info, "Variable names may only be at most 200 characters long, was given: %s", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Check that it isn't some duplicated function name
	symtab_function_record_t* found_func = lookup_function(function_symtab, name);

	//Fail out here
	if(found_func != NULL){
		sprintf(info, "Attempt to redefine function \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the function declaration
		print_function_name(found_func);
		num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Check that it isn't some duplicated variable name
	symtab_variable_record_t* found_var = lookup_variable(variable_symtab, name);

	//Fail out here
	if(found_var != NULL){
		sprintf(info, "Attempt to redefine variable \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_variable_name(found_var);
		num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Finally check that it isn't a duplicated type name
	symtab_type_record_t* found_type = lookup_type(type_symtab, name);

	//Fail out here
	if(found_type!= NULL){
		sprintf(info, "Attempt to redefine type \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_type_name(found_type);
		num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//After the ident, we need to see a colon
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//Fail out here
	if(lookahead.tok != COLON){
		print_parse_message(PARSE_ERROR, "Colon required between ident and type specifier in construct member declaration", parser_line_num);
		num_errors++;
		//Error out
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now we are required to see a valid type specifier
	generic_ast_node_t* type_spec = type_specifier(fl);

	//If this is an error, the whole thing fails
	if(type_spec->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Attempt to use undefined type in construct member", parser_line_num);
		num_errors++;
		//It's already an error, so just send it up
		return type_spec;
	}


	//Now if we finally make it all of the way down here, we are actually set. We'll construct the
	//node that we have and also add it into our symbol table
	
	//We'll first create the symtab record
	symtab_variable_record_t* member_record = create_variable_record(name, STORAGE_CLASS_NORMAL);
	member_record->is_construct_member = 1;
	member_record->line_number = parser_line_num;
	//Store what the type is
	member_record->type = type_spec->inferred_type;
	//Is it mutable or not
	member_record->is_mutable = 0;
	
	//We can now add this into the symbol table
	insert_variable(variable_symtab, member_record);

	//We can now also construct the entire subtree
	generic_ast_node_t* member_node = ast_node_alloc(AST_NODE_CLASS_CONSTRUCT_MEMBER);
	//Store the variable record here
	member_node->variable = member_record;

	//The second child will be the ident node
	add_child_node(member_node, ident);

	//Destory the type spec node when here

	//Store the line number
	member_node->line_number = parser_line_num;

	//All went well so we can send this up the chain
	return member_node;
}


/**
 * A construct member list holds all of the nodes that themselves represent construct members. Like all
 * other rules, this function returns the root node of the subtree that it creates
 *
 * BNF Rule: <construct-member-list> ::= { <construct-member> ; }*
 */
static generic_ast_node_t* construct_member_list(FILE* fl){
	//Lookahead token
	Lexer_item lookahead;

	//Let's first declare the root node. This node will have children that are each construct members
	generic_ast_node_t* member_list = ast_node_alloc(AST_NODE_CLASS_CONSTRUCT_MEMBER_LIST);

	//This is just to seed our search
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//We can see as many construct members as we please here, all delimited by semicols
	do{
		//Put what we saw back
		push_back_token(lookahead);

		//We must first see a valid construct member
		generic_ast_node_t* member_node = construct_member(fl);

		//If it's an error, we'll fail right out
		if(member_node->CLASS == AST_NODE_CLASS_ERR_NODE){
			print_parse_message(PARSE_ERROR, "Invalid construct member declaration", parser_line_num);
			//It's already an error node so just let it propogate
			return member_node;
		}
		
		//Otherwise, we'll add it in as one of the children
		add_child_node(member_list, member_node);

		//Now we will refresh the lookahead
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

		//We must now see a valid semicolon
		if(lookahead.tok != SEMICOLON){
			print_parse_message(PARSE_ERROR, "Construct members must be delimited by ;", parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Refresh it once more
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//So long as we don't see the end
	} while (lookahead.tok != R_CURLY);

	//Once we get here, what we know is that lookahead was not a semicolon. We know that it should
	//be a closing curly brace, so in the interest of better error messages, we'll do a little pre-check
	if(lookahead.tok != R_CURLY){
		print_parse_message(PARSE_ERROR, "Construct members must be delimited by ;", parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//If we get here we know that it's right, but we'll still allow the other rule to handle it
	push_back_token(lookahead);
	//Store the parser line number
	member_list->line_number = parser_line_num;

	//Give the member list back
	return member_list;
}


/**
 * A construct definer is the definition of a construct. We require all parts of the construct to be defined here.
 * We also allow the potential for aliasing as a different type right off of the bat here. Since this is a compiler-specific
 * rule, we only return success or failer
 *
 * REMEMBER: By the time we get here, we've already seen the define and construct keywords due to lookahead rules
 *
 * This rule also handles everything with identifiers to avoid excessive confusion
 *
 * BNF Rule: <construct-definer> ::= define construct <identifier> { <construct-member-list> } {as <identifer>}?;
 */
static u_int8_t construct_definer(FILE* fl){
	//For error printing
	char info[ERROR_SIZE];
	//Freeze the line num
	u_int16_t current_line = parser_line_num;
	//Lookahead token for our uses
	Lexer_item lookahead;
	//The actual type name that we have
	char type_name[MAX_TYPE_NAME_LENGTH];
	//The alias name
	char alias_name[MAX_TYPE_NAME_LENGTH];
	
	//We already know that the type name will have enumerated in it
	strcpy(type_name, "construct ");

	//We are now required to see a valid identifier
	generic_ast_node_t* ident = identifier(fl);

	//Fail case
	if(ident->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Valid identifier required after construct keyword", parser_line_num);
		num_errors++;
		//Destroy the node
		//Fail out
		return FAILURE;
	}

	//Otherwise, we'll now add this identifier into the type name
	strcat(type_name, ident->identifier);	

	//Once we have this, the actual node is useless so we'll free it

	//Now we will reference against the symtab to see if this type name has ever been used before. We only need
	//to check against the type symtab because that is the only place where anything else could start with "enumerated"
	symtab_type_record_t* found = lookup_type(type_symtab, type_name);

	//This means that we are attempting to redefine a type
	if(found != NULL){
		sprintf(info, "Type with name \"%s\" was already defined. First defined here:", type_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the type
		print_type_name(found);
		num_errors++;
		//Fail out
		return FAILURE;
	}

	//Now we are required to see a curly brace
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//Fail case here
	if(lookahead.tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Unelaborated construct definition is not supported", parser_line_num);
		num_errors++;
		//Fail out
		return FAILURE;
	}

	//Otherwise we'll push onto the stack for later matching
	push_token(grouping_stack, lookahead);

	//We are now required to see a valid construct member list
	generic_ast_node_t* mem_list = construct_member_list(fl);

	//Automatic fail case here
	if(mem_list->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid construct member list given in construct definition", parser_line_num);
		//We'll destroy it first
		return FAILURE;
	}

	//Otherwise we got past the list, and now we need to see a closing curly
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//Bail out if this happens
	if(lookahead.tok != R_CURLY){
		print_parse_message(PARSE_ERROR, "Closing curly brace required after member list", parser_line_num);
		num_errors++;
		//Fail out here
		return FAILURE;
	}
	
	//Check for unamtched curlies
	if(pop_token(grouping_stack).tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Unmatched curly braces in construct definition", parser_line_num);
		num_errors++;
		//Fail out here
		return FAILURE;
	}
	
	//If we make it here, we've made it far enough to know what we need to build our type for this construct
	generic_type_t* construct_type = create_constructed_type(type_name, current_line);

	//Now we're going to have to walk the members of the member list, and add in their references to the type.
	//Doing this work now saves us a lot of steps later on for not much duplicated space
	//Start off with the first child
	generic_ast_node_t* cursor = mem_list->first_child;

	//As long as there are more children
	while(cursor != NULL){
		//Sanity check
		if(cursor->CLASS != AST_NODE_CLASS_CONSTRUCT_MEMBER){
			print_parse_message(PARSE_ERROR, "Fatal internal parse error. Found non-construct member in member list", parser_line_num);
			return FAILURE;
		}

		//Pick out the variable record
		symtab_variable_record_t* var = cursor->variable;

		//We'll now add this into the parameter list
		construct_type->construct_type->members[construct_type->construct_type->num_members] = var;
		//Increment the number of members
		(construct_type->construct_type->num_members)++;
		//Store what construct it came from
		var->struct_defined_in = construct_type;

		//Now that we've added it in, advance the cursor
		cursor = cursor->next_sibling;
	}

	//Once we're here, the construct type is fully defined. We can now add it into the symbol table
	insert_type(type_symtab, create_type_record(construct_type));

	//Once we're done with this, the mem list itself has no use so we'll destroy it
	
	//Now we have one final thing to account for. The syntax allows for us to alias the type right here. This may
	//be preferable to doing it later, and is certainly more convenient. If we see a semicol right off the bat, we'll
	//know that we're not aliasing however
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//We're out of here, just return the node that we made
	if(lookahead.tok == SEMICOLON){
		return SUCCESS;
	}
	
	//Otherwise, if this is correct, we should've seen the as keyword
	if(lookahead.tok != AS){
		print_parse_message(PARSE_ERROR, "Semicolon expected after construct definition", parser_line_num);
		num_errors++;
		//Make an error and get out of here
		return FAILURE;
	}

	//Now if we get here, we know that we are aliasing. We won't have a separate node for this, as all
	//we need to see now is a valid identifier. We'll add the identifier as a child of the overall node
	generic_ast_node_t* alias_ident = identifier(fl);

	//If it was invalid
	if(alias_ident->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid identifier given as alias", parser_line_num);
		num_errors++;
		//Deallocate and fail
		return FAILURE;
	}

	//Let's grab the actual name out
	strcpy(alias_name, alias_ident->identifier);

	//Once we have this, the alias ident is of no use to us

	//Real quick, let's check to see if we have the semicol that we need now
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//Last chance for us to fail syntactically 
	if(lookahead.tok != SEMICOLON){
		print_parse_message(PARSE_ERROR, "Semicolon expected after construct definition",  parser_line_num);
		num_errors++;
		//Fail out
		return FAILURE;
	}

	//Check that it isn't some duplicated function name
	symtab_function_record_t* found_func = lookup_function(function_symtab, alias_name);

	//Fail out here
	if(found_func != NULL){
		sprintf(info, "Attempt to redefine function \"%s\". First defined here:", alias_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the function declaration
		print_function_name(found_func);
		num_errors++;
		//Fail out
		return FAILURE;
	}

	//Check that it isn't some duplicated variable name
	symtab_variable_record_t* found_var = lookup_variable(variable_symtab, alias_name);

	//Fail out here
	if(found_var != NULL){
		sprintf(info, "Attempt to redefine variable \"%s\". First defined here:", alias_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_variable_name(found_var);
		num_errors++;
		//Fail out
		return FAILURE;
	}

	//Finally check that it isn't a duplicated type name
	symtab_type_record_t* found_type = lookup_type(type_symtab, alias_name);

	//Fail out here
	if(found_type!= NULL){
		sprintf(info, "Attempt to redefine type \"%s\". First defined here:", alias_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_type_name(found_type);
		num_errors++;
		//Fail out
		return FAILURE;
	}

	//Now we'll make the actual record for the aliased type
	generic_type_t* aliased_type = create_aliased_type(alias_name, construct_type, parser_line_num);

	//Once we've made the aliased type, we can record it in the symbol table
	insert_type(type_symtab, create_type_record(aliased_type));

	//Succeeded so
	return SUCCESS;
}


/**
 * An enum member is simply an identifier. This rule performs all the needed checks to ensure
 * that it's not a duplicate of anything else that we've currently seen. Like all rules, this function
 * returns a reference to the root of the tree it created
 *
 * BNF Rule: <enum-member> ::= <identifier>
 */
static generic_ast_node_t* enum_member(FILE* fl, u_int16_t current_member_val){
	//For error printing
	char info[ERROR_SIZE];

	//We really just need to see a valid identifier here
	generic_ast_node_t* ident = identifier(fl);

	//If it fails, we'll blow the whole thing up
	if(ident->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid identifier given as enum member", parser_line_num);
		num_errors++;
		//It's already an error, so we'll just send it back
		return ident;
	}

	//Now if we make it here, we'll need to check and make sure that it isn't a duplicate of anything else
	//Grab this for convenience
	char* name = ident->identifier;

	//Check that it isn't some duplicated function name
	symtab_function_record_t* found_func = lookup_function(function_symtab, name);

	//Fail out here
	if(found_func != NULL){
		sprintf(info, "Attempt to redefine function \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the function declaration
		print_function_name(found_func);
		num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Check that it isn't some duplicated variable name
	symtab_variable_record_t* found_var = lookup_variable(variable_symtab, name);

	//Fail out here
	if(found_var != NULL){
		sprintf(info, "Attempt to redefine variable \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_variable_name(found_var);
		num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Finally check that it isn't a duplicated type name
	symtab_type_record_t* found_type = lookup_type(type_symtab, name);

	//Fail out here
	if(found_type!= NULL){
		sprintf(info, "Attempt to redefine type \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_type_name(found_type);
		num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Once we make it all the way down here, we know that we don't have any duplication
	//We can now make the record of the enum
	symtab_variable_record_t* enum_record = create_variable_record(name, STORAGE_CLASS_NORMAL);
	//Store the current value
	enum_record->enum_member_value = current_member_val;
	//It is an enum member
	enum_record->is_enumeration_member = 1;
	//This is always initialized
	enum_record->initialized = 1;
	//Later down the line, we'll assign the type that this thing is
	
	//We can now add it into the symtab
	insert_variable(variable_symtab,  enum_record);

	//Finally, we'll construct the node that holds this item and send it out
	generic_ast_node_t* enum_member = ast_node_alloc(AST_NODE_CLASS_ENUM_MEMBER);
	//Store the record in this for ease of access/modification
	enum_member->variable = enum_record;
	//Add the identifier as the child of this node
	add_child_node(enum_member, ident);

	//Finally we'll give the reference back
	return enum_member;
}


/**
 * An enumeration list guarantees that we have at least one enumerator. It also allows us to
 * chain enumerators using commas. Like all rules, this rule returns a reference to the root of 
 * the subtree that it creates
 *
 * BNF Rule: <enum-member-list> ::= <enum-member>{, <enum-member>}*
 */
static generic_ast_node_t* enum_member_list(FILE* fl){
	//Lookahead token
	Lexer_item lookahead;
	//The enum member current number
	u_int16_t current_member_val = 0;

	//We will first create the list node
	generic_ast_node_t* enum_list_node = ast_node_alloc(AST_NODE_CLASS_ENUM_MEMBER_LIST);

	//Now, we can see as many enumerators as we'd like here, each separated by a comma
	do{
		//First we need to see a valid enum member
		generic_ast_node_t* member = enum_member(fl, current_member_val);
		//Increment this
		current_member_val++;

		//If the member is bad, we bail right out
		if(member->CLASS == AST_NODE_CLASS_ERR_NODE){
			print_parse_message(PARSE_ERROR, "Invalid member given in enum definition", parser_line_num);
			num_errors++;
			//It's already an error so we'll just send it up the chain
			return member;
		}

		//We can now add this in as a child of the enum list
		add_child_node(enum_list_node, member);

		//Finally once we make it here we'll refresh the lookahead
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//Keep going as long as we see commas
	} while(lookahead.tok == COMMA);

	//Once we make it out here, we know that we didn't see a comma. We know that we really need to see an
	//R_CURLY when we get here, so if we didn't we can give a more helpful error message here
	if(lookahead.tok != R_CURLY){
		print_parse_message(PARSE_ERROR, "Enum members must be separated by commas in defintion", parser_line_num);
		num_errors++;
		//Return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}
	
	//Otherwise if we end up here all went well. We'll let the caller do the final checking with the R_CURLY so 
	//we'll give it back
	push_back_token(lookahead);

	//We'll now give back the list node itself
	return enum_list_node;
}


/**
 * An enumeration definition is where we see the actual definition of an enum. Since this is a compiler
 * only rule, we will only return success or failure from this node
 *
 * Important note: By the time we get here, we will have already consume the "define" and "enum" tokens
 *
 * BNF Rule: <enum-definer> ::= define enum <identifier> { <enum-member-list> } {as <identifier>}?;
 */
static u_int8_t enum_definer(FILE* fl){
	//For error printing
	char info[ERROR_SIZE];
	//Freeze the current line number
	u_int16_t current_line = parser_line_num;
	//Lookahead token
	Lexer_item lookahead;
	//The actual name of the enum
	char name[MAX_TYPE_NAME_LENGTH];
	//The alias name
	char alias_name[MAX_TYPE_NAME_LENGTH];

	//We already know that it will have this in the name
	strcpy(name, "enum ");

	//We now need to see a valid identifier to round out the name
	generic_ast_node_t* ident = identifier(fl);

	//Fail case here
	if(ident->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid name given to enum definition", parser_line_num);
		num_errors++;
		//Deallocate and fail
		return FAILURE;
	}

	//Now if we get here we know that we found a valid ident, so we'll add it to the name
	strcat(name, ident->identifier);

	//Once we have this, we no longer need the ident node

	//Now we need to check that this name isn't already currently in use. We only need to check against the
	//type symtable, because nothing else could have enum in the name
	symtab_type_record_t* found_type = lookup_type(type_symtab, name);

	//If we found something, that's an illegal redefintion
	if(found_type != NULL){
		sprintf(info, "Type \"%s\" has already been defined. First defined here:", name); 
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Print out the actual type too
		print_type_name(found_type);
		num_errors++;
		return FAILURE;
	}

	//Now that we know we don't have a duplicate, we can now start looking for the enum list
	//We must first see an L_CURLY
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//Fail case here
	if(lookahead.tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Left curly expected before enumerator list", parser_line_num);
		num_errors++;
		//Fail out
		return FAILURE;
	}

	//Push onto the stack for grouping
	push_token(grouping_stack, lookahead);
	
	//Now we must see a valid enum member list
	generic_ast_node_t* member_list = enum_member_list(fl);

	//If it failed, we bail out
	if(member_list->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid enumeration member list given in enum definition", current_line);
		//Destroy the member list
		return FAILURE;
	}

	//Now that we get down here the only thing left syntatically is to check for the closing curly
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//First chance to fail
	if(lookahead.tok != R_CURLY){
		print_parse_message(PARSE_ERROR, "Closing curly brace expected after enum member list", parser_line_num);
		num_errors++;
		//Destroy the member list
		return FAILURE;
	}

	//We must also see matching ones here
	if(pop_token(grouping_stack).tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Unmatched curly braces detected in enum defintion", parser_line_num);
		num_errors++;
		//Destroy the member list
		return FAILURE;
	}

	//Now that we know everything here has worked, we can finally create the enum type
	generic_type_t* enum_type = create_enumerated_type(name, current_line);

	//Now we will crawl through all of the types that we had and add their references into this enum type's list
	//This should in theory be an enum member node
	generic_ast_node_t* cursor = member_list->first_child;	

	//Go through while the cursor isn't null
	while(cursor != NULL){
		//Sanity check here, this should be of type enum member
		if(cursor->CLASS != AST_NODE_CLASS_ENUM_MEMBER){
			print_parse_message(PARSE_ERROR, "Fatal internal compiler error. Found non-member node in member list for enum", parser_line_num);
			return FAILURE;
		}

		//Otherwise we're fine
		//We'll now extract the symtab record that this node holds onto
		symtab_variable_record_t* variable_rec = cursor->variable;

		//Associate the type here as well
		variable_rec->type = enum_type;

		//Increment the size here
		enum_type->type_size += variable_rec->type->type_size;

		//We will store this in the enum types records
		enum_type->enumerated_type->tokens[enum_type->enumerated_type->token_num] = variable_rec;
		//Increment the number of tokens by one
		(enum_type->enumerated_type->token_num)++;

		//Move the cursor up by one
		cursor = cursor->next_sibling;
	}

	//Now that this is all filled out, we can add this to the type symtab
	insert_type(type_symtab, create_type_record(enum_type));

	//Once we're here the member list is useless, so we'll deallocate it

	//Now once we are here, we can optionally see an alias command. These alias commands are helpful and convenient
	//for redefining variables immediately upon declaration. They are prefaced by the "As" keyword
	//However, before we do that, we can first see if we have a semicol
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//This means that we're out, so just give back the root node
	if(lookahead.tok == SEMICOLON){
		//We're done
		return SUCCESS;
	}

	//Otherwise, it is a requirement that we see the as keyword, so if we don't we're in trouble
	if(lookahead.tok != AS){
		print_parse_message(PARSE_ERROR, "Semicolon expected after enum definition", parser_line_num);
		num_errors++;
		//Fail out
		return FAILURE;
	}

	//Now if we get here, we know that we are aliasing. We won't have a separate node for this, as all
	//we need to see now is a valid identifier. We'll add the identifier as a child of the overall node
	generic_ast_node_t* alias_ident = identifier(fl);

	//If it was invalid
	if(alias_ident->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid identifier given as alias", parser_line_num);
		num_errors++;
		//Deallocate and fail
		return FAILURE;
	}

	//Extract the alias name
	strcpy(alias_name, alias_ident->identifier);

	//Now that we're here we don't need the node anymore
	
	//Real quick, let's check to see if we have the semicol that we need now
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//Last chance for us to fail syntactically 
	if(lookahead.tok != SEMICOLON){
		print_parse_message(PARSE_ERROR, "Semicolon expected after enum definition",  parser_line_num);
		num_errors++;
		//Fail out
		return FAILURE;
	}

	//Check that it isn't some duplicated function name
	symtab_function_record_t* found_func = lookup_function(function_symtab, alias_name);

	//Fail out here
	if(found_func != NULL){
		sprintf(info, "Attempt to redefine function \"%s\". First defined here:", alias_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the function declaration
		print_function_name(found_func);
		num_errors++;
		//Fail out
		return FAILURE;
	}

	//Check that it isn't some duplicated variable name
	symtab_variable_record_t* found_var = lookup_variable(variable_symtab, alias_name);

	//Fail out here
	if(found_var != NULL){
		sprintf(info, "Attempt to redefine variable \"%s\". First defined here:", alias_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_variable_name(found_var);
		num_errors++;
		//Fail out
		return FAILURE;
	}

	//Finally check that it isn't a duplicated type name
	found_type = lookup_type(type_symtab, alias_name);

	//Fail out here
	if(found_type!= NULL){
		sprintf(info, "Attempt to redefine type \"%s\". First defined here:", alias_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_type_name(found_type);
		num_errors++;
		//Fail out
		return FAILURE;
	}

	//Now we'll make the actual record for the aliased type
	generic_type_t* aliased_type = create_aliased_type(alias_name, enum_type, parser_line_num);

	//Once we've made the aliased type, we can record it in the symbol table
	insert_type(type_symtab, create_type_record(aliased_type));

	return SUCCESS;
}


/**
 * A type address specifier allows us to specify that a type is actually an address(&) or some kind of array of these types.
 * For type safety, ollie lang requires array bounds to be known at comptime. Like all other rules in the language, this 
 * function returns a reference to the root node that it created
 *
 * As a reminder, we will assume that the caller has put back everything for us(the tokens and such) before they call
 *
 * BNF Rule: {type-address-specifier} ::= [<constant>]
 * 										| * 
 */
static generic_ast_node_t* type_address_specifier(FILE* fl){
	//The lookahead token
	Lexer_item lookahead;
	//The node that we'll be giving back
	generic_ast_node_t* type_addr_node = ast_node_alloc(AST_NODE_CLASS_TYPE_ADDRESS_SPECIFIER);

	//Let's see what we have as the address specifier
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	
	//Very easy to handle this, we'll just construct the node and give it back
	if(lookahead.tok == STAR){
		//This is an address specifier
		type_addr_node->address_type = ADDRESS_SPECIFIER_ADDRESS;
		//And we're done, just give it back
		return type_addr_node;
	}

	//If we get here, we know that this has to be the case, so if it's not we're out
	if(lookahead.tok != L_BRACKET){
		print_parse_message(PARSE_ERROR, "Array [] or address & required in type address specifier", parser_line_num);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Otherwise if we get here we know that it was an L_BRACKET, so we'll push to the stack for matching
	push_token(grouping_stack, lookahead);

	//Now once we end up here, we need to see a valid constant that is an integer
	generic_ast_node_t* constant_node = constant(fl, SEARCHING_FOR_CONSTANT);

	//If we fail here it's an automatic out
	if(constant_node->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid constant given to array specifier", parser_line_num);
		num_errors++;
		//It's already an error so we'll just give it back
		return constant_node;
	}

	//Now if we make it here, we must also check that it's an integer
	if(((constant_ast_node_t*)(constant_node->node))->constant_type != INT_CONST){
		print_parse_message(PARSE_ERROR, "Array bounds must be an integer constant", parser_line_num);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now we've seen a valid integer, so the only other thing that we need syntactically is the closing brace
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we don't see an R_BRACKET
	if(lookahead.tok != R_BRACKET){
		print_parse_message(PARSE_ERROR, "Array specifier must have enclosed square brackets", parser_line_num);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now we must also check for matching
	if(pop_token(grouping_stack).tok != L_BRACKET){
		print_parse_message(PARSE_ERROR, "Unmatched square brackets detected in array specifier", parser_line_num);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}
	
	//Once we get here, we know that everything is syntactically valid. We'll now build and return our node
	//Declare what kind of node this is
	type_addr_node->address_type = ADDRESS_SPECIFIER_ARRAY;
	//Since it's an array, it will have a child that is the constant node
	add_child_node(type_addr_node, constant_node);
	//Store the line number
	type_addr_node->line_number = parser_line_num;
	
	//Finally we'll give the node back
	return type_addr_node;
}


/**
 * A type name node is always a child of a type specifier. It consists
 * of all of our primitive types and any defined construct or
 * aliased types that we may have. It is important to note that any
 * non-primitive type needs to have been previously defined for it to be
 * valid. Like all rules in the language, this rule returns a root reference 
 * of the subtree that it creates
 * 
 * If we are using this rule, we are assuming that this type exists in the system
 * 
 * BNF Rule: <type-name> ::= void 
 * 						   | u8 
 * 						   | i8 
 * 						   | u16 
 * 						   | i16 
 * 						   | u32 
 * 						   | i32 
 * 						   | u64 
 * 						   | i64 
 * 						   | f32 
 * 						   | f64 
 * 						   | char 
 * 						   | enum <identifier>
 * 						   | construct <identifier>
 * 						   | <identifier>
 */
static generic_ast_node_t* type_name(FILE* fl){
	//For error printing
	char info[ERROR_SIZE];
	//Lookahead token
	Lexer_item lookahead;
	//A temporary holder for the type name
	char type_name[MAX_TYPE_NAME_LENGTH];

	//Let's create the type name node
	generic_ast_node_t* type_name_node = ast_node_alloc(AST_NODE_CLASS_TYPE_NAME);

	//Let's see what we have
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	
	//These are all of our basic types
	if(lookahead.tok == VOID || lookahead.tok == U_INT8 || lookahead.tok == S_INT8 || lookahead.tok == U_INT16
	   || lookahead.tok == S_INT16 || lookahead.tok == U_INT32 || lookahead.tok == S_INT32 || lookahead.tok == U_INT64
	   || lookahead.tok == S_INT64 || lookahead.tok == FLOAT32 || lookahead.tok == FLOAT64 || lookahead.tok == CHAR){

		//Copy the lexeme into the node, no need for intermediaries here
		strcpy(type_name_node->type_name, lookahead.lexeme);

		//We will now grab this record from the symtable to make our life easier
		symtab_type_record_t* record = lookup_type(type_symtab, lookahead.lexeme);

		//Sanity check, if this is null something is very wrong
		if(record == NULL){
			print_parse_message(PARSE_ERROR, "Fatal internal compiler error. Primitive type could not be found in symtab", parser_line_num);
			//Create and give back an error node
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Link this record in with the actual node
		type_name_node->type_record = record;

		//This one is now all set to send up. We will not store any children if this is the case
		return type_name_node;

	//There's also a chance that we see an enum type
	} else if(lookahead.tok == ENUM){
		//We know that this keyword is in the name, so we'll add it in
		strcpy(type_name, "enum ");

		//It is required that we now see a valid identifier
		generic_ast_node_t* type_ident = identifier(fl);

		//If we fail, we'll bail out
		if(type_ident->CLASS == AST_NODE_CLASS_ERR_NODE){
			print_parse_message(PARSE_ERROR, "Invalid identifier given as enum type name", parser_line_num);
			//It's already an error so just give it back
			return type_ident;
		}

		//Array bounds checking
		if(strlen(type_ident->identifier) > MAX_TYPE_NAME_LENGTH - 10){
			sprintf(info, "Type names may only be 200 characters long, but was given %s", type_ident->identifier);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Otherwise it actually did work, so we'll add it's name onto the already existing type node
		strcat(type_name, type_ident->identifier);

		//Now we'll look up the record in the symtab. As a reminder, it is required that we see it here
		symtab_type_record_t* record = lookup_type(type_symtab, type_name);

		//If we didn't find it it's an instant fail
		if(record == NULL){
			sprintf(info, "Enum %s was never defined. Types must be defined before use", type_name);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			num_errors++;
			//Create and return an error node
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Otherwise we were able to find the record, so we'll add in to the node
		type_name_node->type_record = record;
		//Copy the name over here for convenience later
		strcpy(type_name_node->type_name, type_name);

		//We can also add in the type ident as a child node of the type name node
		add_child_node(type_name_node, type_ident);

		//Once we make it here, we should be all set to get out
		return type_name_node;

	//Construct names are pretty much the same as enumerated names
	} else if(lookahead.tok == CONSTRUCT){
		//We know that this keyword is in the name, so we'll add it in
		strcpy(type_name, "construct ");

		//It is required that we now see a valid identifier
		generic_ast_node_t* type_ident = identifier(fl);

		//If we fail, we'll bail out
		if(type_ident->CLASS == AST_NODE_CLASS_ERR_NODE){
			print_parse_message(PARSE_ERROR, "Invalid identifier given as construct type name", parser_line_num);
			//It's already an error so just give it back
			return type_ident;
		}

		//Array bounds checking
		if(strlen(type_ident->identifier) > MAX_TYPE_NAME_LENGTH - 10){
			sprintf(info, "Type names may only be 200 characters long, but was given %s", type_ident->identifier);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Otherwise it actually did work, so we'll add it's name onto the already existing type node
		strcat(type_name, type_ident->identifier);

		//Now we'll look up the record in the symtab. As a reminder, it is required that we see it here
		symtab_type_record_t* record = lookup_type(type_symtab, type_name);

		//If we didn't find it it's an instant fail
		if(record == NULL){
			sprintf(info, "Construct %s was never defined. Types must be defined before use", type_name);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			num_errors++;
			//Create and return an error node
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Otherwise we were able to find the record, so we'll add in to the node
		type_name_node->type_record = record;
		//Copy the name over here for convenience later
		strcpy(type_name_node->type_name, type_name);

		//We can also add in the type ident as a child node of the type name node
		add_child_node(type_name_node, type_ident);

		//Once we make it here, we should be all set to get out
		return type_name_node;

	//If this is the case then we have to see some user defined name, which is an ident
	} else {
		//Put the token back for the ident rule
		push_back_token(lookahead);

		//We will let the identifier rule handle it
		generic_ast_node_t* type_ident = identifier(fl);

		//If we fail, we'll bail out
		if(type_ident->CLASS == AST_NODE_CLASS_ERR_NODE){
			print_parse_message(PARSE_ERROR, "Invalid identifier given as type name", parser_line_num);
			//It's already an error so just give it back
			return type_ident;
		}

		//Array bounds checking
		if(strlen(type_ident->identifier) > MAX_TYPE_NAME_LENGTH - 10){
			sprintf(info, "Type names may only be 200 characters long, but was given %s", type_ident->identifier);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Grab a pointer for it for convenience
		char* temp_name = type_ident->identifier;

		//Now we'll look up the record in the symtab. As a reminder, it is required that we see it here
		symtab_type_record_t* record = lookup_type(type_symtab, temp_name);

		//If we didn't find it it's an instant fail
		if(record == NULL){
			sprintf(info, "Type %s was never defined. Types must be defined before use", temp_name);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			num_errors++;
			//Create and return an error node
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}
		
		//Dealias the type here
		generic_type_t* dealiased_type = dealias_type(record->type);

		//The true type record
		symtab_type_record_t* true_type = lookup_type(type_symtab, dealiased_type->type_name);

		//Otherwise if we get here we were able to find it, so we're good to move on
		type_name_node->type_record = true_type;
		//Copy the name over here for convenience later
		strcpy(type_name_node->type_name, dealiased_type->type_name);
		//We can also add in the type ident as a child node of the type name node
		add_child_node(type_name_node, type_ident);
		//Store the line number
		type_name_node->line_number = parser_line_num;

		//Once we make it here, we should be all set to get out
		return type_name_node;
	}
}



/**
 * A type specifier is a type name that is then followed by an address specifier, this being  
 * the array brackets or address indicator. Like all rules, the type specifier rule will
 * always return a reference to the root of the subtree it creates
 *
 * The type specifier itself is comprised of some type name and potential address specifiers
 * 
 * NOTE: This rule REQUIRES that the name actually be defined. This rule is not used for the 
 * definition of names itself
 *
 * BNF Rule: <type-specifier> ::= <type-name>{<type-address-specifier>}*
 */
static generic_ast_node_t* type_specifier(FILE* fl){
	//Lookahead var
	Lexer_item lookahead;

	//We'll first create and attach the type specifier node
	//At this point the node will be entirely blank
	generic_ast_node_t* type_spec_node = ast_node_alloc(AST_NODE_CLASS_TYPE_SPECIFIER);

	//Now we'll hand off the rule to the <type-name> function. The type name function will
	//return a record of the node that the type name has. If the type name function could not
	//find the name, then it will send back an error that we can handle here
	generic_ast_node_t* name_node = type_name(fl);

	//We'll just fail here, no need for any error printing
	if(name_node->CLASS == AST_NODE_CLASS_ERR_NODE){
		//It's already and error so just give it back
		return name_node;
	}

	//The name node will always be a child of the specifier node, so we'll add it now
	add_child_node(type_spec_node, name_node);

	//Now once we make it here, we know that we have a name that actually exists in the symtab
	//The current type record is what we will eventually point our node to
	symtab_type_record_t* current_type_record = name_node->type_record;
	
	//Let's see where we go from here
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//As long as we are seeing address specifiers
	while(lookahead.tok == STAR || lookahead.tok == L_BRACKET){
		//Put the token back
		push_back_token( lookahead);
		//We'll now let the other rule handle it
		generic_ast_node_t* address_specifier = type_address_specifier(fl);

		//If it's invalid for some reason, we'll fail out here
		if(address_specifier->CLASS == AST_NODE_CLASS_ERR_NODE){
			print_parse_message(PARSE_ERROR, "Invalid address specifier given in type specifier", parser_line_num);
			num_errors++;
			//It's already an error so just send it up
			return address_specifier;
		}

		//Now we know that we have a valid address specifier. We'll first add it as a child to the
		//parent node
		add_child_node(type_spec_node, address_specifier);

		//We'll now create a type that represents either a pointer or an array. This type may or may not
		//currently be in the symbol table, but that doesn't matter. These are not truly new types, as they
		//are only aggregate types
		
		//If it's a pointer type
		if(address_specifier->address_type == ADDRESS_SPECIFIER_ADDRESS){
			//Let's create the pointer type. This pointer type will point to the current type
			generic_type_t* pointer = create_pointer_type(current_type_record->type, parser_line_num);

			//We'll now add it into the type symbol table. If it's already in there, which it very well may be, that's
			//also not an issue
			symtab_type_record_t* found_pointer = lookup_type(type_symtab, pointer->type_name);

			//If we did not find it, we will add it into the symbol table
			if(found_pointer == NULL){
				//Create the type record
				symtab_type_record_t* created_pointer = create_type_record(pointer);
				//Insert it into the symbol table
				insert_type(type_symtab, created_pointer);
				//We'll also set the current type record to be this
				current_type_record = created_pointer;
			} else {
				//Otherwise, just set the current type record to be what we found
				current_type_record = found_pointer;
				//We don't need the other ponter if this is the case
				type_dealloc(pointer);
			}

		//Otherwise we know that we found an array pointer
		} else {
			//Let's grab the constant node out
			generic_ast_node_t* constant_node = address_specifier->first_child;

			//Sanity check here
			if(constant_node == NULL || constant_node->CLASS != AST_NODE_CLASS_CONSTANT){
				print_parse_message(PARSE_ERROR, "Fatal internal compiler error. Could not find constant node in array specifier", parser_line_num);
				//Get out if this happens
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}

			//Let's now grab the number of members
			u_int32_t num_members = ((constant_ast_node_t*)(constant_node->node))->int_val;

			//Lets create the array type
			generic_type_t* array_type = create_array_type(current_type_record->type, parser_line_num, num_members);

			//We'll now add it into the type symbol table. If it's already in there, which it very well may be, that's
			//also not an issue
			symtab_type_record_t* found_array = lookup_type(type_symtab, array_type->type_name);

			//If we did not find it, we will add it into the symbol table
			if(found_array == NULL){
				//Create the type record
				symtab_type_record_t* created_array = create_type_record(array_type);
				//Insert it into the symbol table
				insert_type(type_symtab, created_array);
				//We'll also set the current type record to be this
				current_type_record = created_array;
			} else {
				//Otherwise, just set the current type record to be what we found
				current_type_record = found_array;
				//We don't need the other one if this is the case
				type_dealloc(array_type);
			}
		}

		//Refresh the lookahead
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	}

	//Once we get here we stopped seeing the type specifiers, so we'll give the token back
	push_back_token(lookahead);

	//The type specifier node has already been fully created, so we just need to add the type reference
	//and return it
	//This will store a reference to whatever the type record is
	type_spec_node->inferred_type =  current_type_record->type;
	//Store the line number
	type_spec_node->line_number = parser_line_num;

	//Finally we can give back the node
	return type_spec_node;
}


/**
 * A parameter declaration is a fancy kind of variable. It is stored in the symtable at the 
 * top lexical scope for the function itself. Like all rules, it returns a reference to the
 * root of the subtree that it creates
 *
 * BNF Rule: <parameter-declaration> ::= {mut}? <identifier> : <type-specifier>
 */
static generic_ast_node_t* parameter_declaration(FILE* fl){
	//For any needed error printing -- need more size
	char info[ERROR_SIZE + 500];
	//Is it mutable?
	u_int8_t is_mut = 0;
	//Lookahead token
	Lexer_item lookahead;

	//Let's first create the top level node here
	generic_ast_node_t* parameter_decl_node;

	//Now we can optionally see the constant keyword here
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	
	//Is this parameter constant? If so we'll just set a flag for later
	if(lookahead.tok == DOTDOTDOT){
		//This is a special elaborative param
		parameter_decl_node = ast_node_alloc(AST_NODE_CLASS_ELABORATIVE_PARAM);
		//We're done here
		return parameter_decl_node;
	} else {
		//Otherwise we have a regular param node
		parameter_decl_node = ast_node_alloc(AST_NODE_CLASS_PARAM_DECL);
	}

	if(lookahead.tok == MUT){
		is_mut = 1;
	} else {
		//Put it back and move on
		push_back_token(lookahead);
	}

	//Following the valid type specifier declaration, we are required to to see a valid variable. This
	//takes the form of an ident
	generic_ast_node_t* ident = identifier(fl);

	//If it didn't work we fail immediately
	if(ident->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid name given to parameter in function definition", parser_line_num);
		num_errors++;
		//It's already an error, so just return it
		return ident;
	}

	//Now we must perform all needed duplication checks for the name
	//Grab this for convenience
	char* name = ident->identifier;

	//Check that it isn't some duplicated function name
	symtab_function_record_t* found_func = lookup_function(function_symtab, name);

	//Fail out here
	if(found_func != NULL){
		sprintf(info, "Attempt to redefine function \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the function declaration
		print_function_name(found_func);
		num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Check that it isn't some duplicated variable name
	symtab_variable_record_t* found_var = lookup_variable(variable_symtab, name);

	//Fail out here
	if(found_var != NULL){
		sprintf(info, "Attempt to redefine variable \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_variable_name(found_var);
		num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Finally check that it isn't a duplicated type name
	symtab_type_record_t* found_type = lookup_type(type_symtab, name);

	//Fail out here
	if(found_type != NULL){
		sprintf(info, "Attempt to redefine type \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_type_name(found_type);
		num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now we need to see a colon
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If it isn't a colon, we're out
	if(lookahead.tok != COLON){
		print_parse_message(PARSE_ERROR, "Colon required between type specifier and identifier in paramter declaration", parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//We are now required to see a valid type specifier node
	generic_ast_node_t* type_spec_node = type_specifier(fl);
	
	//If the node fails, we'll just send the error up the chain
	if(type_spec_node->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid type specifier gien to function parameter", parser_line_num);
		//It's already an error, just propogate it up
		return type_spec_node;
	}

	//Once we get here, we have actually seen an entire valid parameter 
	//declaration. It is now incumbent on us to store it in the variable 
	//symbol table
	
	//Let's first construct the variable record
	symtab_variable_record_t* param_record = create_variable_record(name, STORAGE_CLASS_NORMAL);
	//It is a function parameter
	param_record->is_function_paramater = 1;
	//We assume that it was initialized
	param_record->initialized = 1;
	//Add the line number
	param_record->line_number = parser_line_num;
	//If it is mutable
	param_record->is_mutable = is_mut;
	//Store the type as well, very important
	param_record->type = type_spec_node->inferred_type;

	//We've now built up our param record, so we'll give add it to the symtab
	insert_variable(variable_symtab, param_record);

	//We'll also save the associated record in the node
	parameter_decl_node->variable = param_record;
	//Store the line number
	parameter_decl_node->line_number = parser_line_num;
	//Destroy the type specifier node
	//Destroy the ident node

	//Finally, we'll send this node back
	return parameter_decl_node;
}


/**
 * A paramater list will handle all of the parameters in a function definition. It is important
 * to note that a parameter list may very well be empty, and that this rule will handle that case.
 * Regardless of the number of parameters(maximum of 6), a paramter list node will always be returned
 *
 * <parameter-list> ::= <parameter-declaration> { ,<parameter-declaration>}*
 */
static generic_ast_node_t* parameter_list(FILE* fl){
	//Lookahead token
	Lexer_item lookahead;

	//Let's now create the parameter list node
	generic_ast_node_t* param_list_node = ast_node_alloc(AST_NODE_CLASS_PARAM_LIST);
	//Initially no params
	param_list_node->num_params = 0;

	//Now let's see what we have as the token. If it's an R_PAREN, we know that we're
	//done here and we'll just return an empty list
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If it's an R_PAREN, we'll just leave
	if(lookahead.tok == R_PAREN){
		//Return the list node
		push_back_token(lookahead);
		return param_list_node;
	} else {
		//Put it back for the search
		push_back_token(lookahead);
	}

	//We'll keep going as long as we see more commas
	do{
		//We must first see a valid parameter declaration
		generic_ast_node_t* param_decl = parameter_declaration(fl);

		//It's invalid, we'll just send it up the chain
		if(param_decl->CLASS == AST_NODE_CLASS_ERR_NODE){
			//It's already an error so send it on up
			return param_decl;
		}

		//Let's see if we have a special parameter elaboration type here
		if(param_decl->CLASS == AST_NODE_CLASS_ELABORATIVE_PARAM){
			//Add it as a child node
			add_child_node(param_list_node, param_decl);

			//Now we'll return the entire thing
			param_list_node->line_number = parser_line_num;
			return param_list_node;
		}

		//Add this in as a child node
		add_child_node(param_list_node, param_decl);

		//Otherwise it was valid, so we've seen one more parameter
		param_list_node->num_params++;

		//Refresh the lookahead token
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//We keep going as long as we see commas
	} while(lookahead.tok == COMMA);

	//Once we make it here, we know that we don't have another comma. We'll put it back
	//for the caller to handle
	push_back_token(lookahead);

	//Store the line number
	param_list_node->line_number = parser_line_num;
	//Now we're done here, so we'll just give the root node back
	return param_list_node;
}


/**
 * An expression statement can optionally have an expression in it. Like all rules, it returns 
 * a reference to the root of the subtree it creates
 *
 * BNF Rule: <expression-statement> ::= {<assignment-expression>}?;
 */
static generic_ast_node_t* expression_statement(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	//The lookahead token
	Lexer_item lookahead;

	//Let's see if we have a semicolon. If we do, we'll just jump right out
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//Empty expression, we're done here
	if(lookahead.tok == SEMICOLON){
		//Blank statement, simply leave
		return NULL;
	}

	//Otherwise, put it back and call expression
	push_back_token(lookahead);
	
	//Now we know that it's not empty, so we have to see a valid expression
	generic_ast_node_t* expr_node = assignment_expression(fl);

	//If this fails, the whole thing is over
	if(expr_node->CLASS == AST_NODE_CLASS_ERR_NODE){
		//It's already an error, so just send it back up
		return expr_node;
	}

	//Now to close out we must see a semicolon
	//Let's see if we have a semicolon
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//Empty expression, we're done here
	if(lookahead.tok != SEMICOLON){
		print_parse_message(PARSE_ERROR, "Semicolon expected after statement", current_line);
		num_errors++;
		//Create and send back an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Otherwise we're all set
	return expr_node;
}


/**
 * A labeled statement could come as part of a switch statement or could
 * simply be a label that can be used for jumping. Whatever it is, it is
 * always followed by a colon. Like all rules, this rule returns a reference to
 * it's root node
 *
 * We must also ensure, in the case of a case statement, that the type of what we're matching with
 * is compatible with what we're switching on
 *
 * <labeled-statement> ::= <label-identifier> : 
 */
static generic_ast_node_t* labeled_statement(FILE* fl){
	//For error printing -- need more
	char info[ERROR_SIZE + 500];
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	//Lookahead token
	Lexer_item lookahead;

	//Let's create the label ident node
	generic_ast_node_t* label_stmt = ast_node_alloc(AST_NODE_CLASS_LABEL_STMT);
	//Save our line number
	label_stmt->line_number = parser_line_num;

	//Let's see if we can find one
	generic_ast_node_t* label_ident = label_identifier(fl);

	//If it's bad we'll fail out here
	if(label_ident->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid label identifier given as label ident statement", current_line);
		num_errors++;
		//Return the label ident, it's already an error
		return label_ident;
	}
		
	//Let's also verify that we have the colon right now
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we don't see one, we need to scrap it
	if(lookahead.tok != COLON){
		print_parse_message(PARSE_ERROR, "Colon required after label statement", current_line);
		num_errors++;
		//Error node return
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}
	//Otherwise we are all good syntactically here

	//Grab the name out for convenience
	char* label_name = label_ident->identifier;

	//We now need to make sure that it isn't a duplicate
	symtab_variable_record_t* found = lookup_variable_lower_scope(variable_symtab, label_name);

	//If we did find it, that's bad
	if(found != NULL){
		sprintf(info, "Label identifier %s has already been declared. First declared here: ", label_name); 
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		print_variable_name(found);
		num_errors++;
		//give back an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Grab the label type
	//The label type is one of our core types
	symtab_type_record_t* label_type = lookup_type(type_symtab, "label");

	//Sanity check here
	if(label_type == NULL){
		print_parse_message(PARSE_ERROR, "Fatal internal compiler error. Basic type label was not found", parser_line_num);
		//Get out if this happens
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now that we know we didn't find it, we'll create it
	found = create_variable_record(label_name, STORAGE_CLASS_NORMAL);
	//Store the type
	found->type = label_type->type;
	//Store the fact that it is a label
	found->is_label = 1;
	//Store the line number
	found->line_number = parser_line_num;
	//Store what function it's defined in(important for later)
	found->function_declared_in = current_function;

	//Put into the symtab
	insert_variable(variable_symtab, found);

	//We'll also associate this variable with the node
	label_stmt->variable = found;

	//Now we can get out
	return label_stmt;
}




/**
 * The if statement has a variety of different nodes that it holds as children. Like all rules, this function
 * returns a reference to the root node that it creates
 *
 * NOTE: We assume that the caller has already seen and consumed the if token if they make it here
 *
 * BNF Rule: <if-statement> ::= if( <logical-or-expression> ) then <compound-statement> {else if statement}* {else-statement}?
 */
static generic_ast_node_t* if_statement(FILE* fl){
	//For error printing
	char info[ERROR_SIZE];

	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	//Lookahead tokens
	Lexer_item lookahead;
	Lexer_item lookahead2;

	//Let's first create our if statement. This is an overall header for the if statement as a whole. Everything
	//will be a child of this statement
	generic_ast_node_t* if_stmt = ast_node_alloc(AST_NODE_CLASS_IF_STMT);

	//Remember, we've already seen the if token, so now we just need to see an L_PAREN
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//Fail out if we don't have it
	if(lookahead.tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Left parenthesis expected after if statement", current_line);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Push onto the stack for matching later
	push_token(grouping_stack, lookahead);
	
	//We now need to see a valid conditional expression
	generic_ast_node_t* expression_node = logical_or_expression(fl);

	//If we see an invalid one
	if(expression_node->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid conditional expression given as if statement condition", current_line);
		num_errors++;
		//It's already an error so just return it
		return expression_node;
	}

	/**
	 * The expression of this type must be compatible at the very list with a u_int64
	 */
	symtab_type_record_t* int_type = lookup_type(type_symtab, "u64");

	//If it's not of this type or a compatible type(pointer, smaller int, etc, it is out)
	if(expression_node->inferred_type->type_class != TYPE_CLASS_POINTER && types_compatible(int_type->type, expression_node->inferred_type) == NULL){
		sprintf(info, "If statements require an int or pointer type to be in their condition, but was given type \"%s\"", expression_node->inferred_type->type_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Following the expression we need to see a closing paren
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we don't see the R_Paren
	if(lookahead.tok != R_PAREN){
		print_parse_message(PARSE_ERROR, "Right parenthesis expected after expression in if statement", current_line);
		num_errors++;
		//Return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now let's check the stack, we need to have matching ones here
	if(pop_token(grouping_stack).tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected", current_line);
		num_errors++;
		//Return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//If we make it to this point, we need to see the THEN keyword
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//Fail out if bad
	if(lookahead.tok != THEN){
		print_parse_message(PARSE_ERROR, "then keyword expected following expression in if statement", current_line);
		num_errors++;
		//Return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//If we make it here, we can add this in as the first child to the root node
	add_child_node(if_stmt, expression_node);

	//Now following this, we need to see a valid compound statement
	generic_ast_node_t* compound_stmt_node = compound_statement(fl);

	//If this node fails, whole thing is bad
	if(compound_stmt_node->CLASS == AST_NODE_CLASS_ERR_NODE){
		num_errors++;
		//It's already an error, so just send it back up
		return compound_stmt_node;
	}

	//If we make it down here, we know that it's valid. As such, we can now add it as a child node
	add_child_node(if_stmt, compound_stmt_node);

	//Now we're at the point where we can optionally see else if statements.
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	lookahead2 = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//So long as we see "else if's", we will keep repeating this process
	while(lookahead.tok == ELSE && lookahead2.tok == IF){
		//We've found one - let's create our fresh else if node
		generic_ast_node_t* else_if_node = ast_node_alloc(AST_NODE_CLASS_ELSE_IF_STMT);

		//Remember, we've already seen the if token, so now we just need to see an L_PAREN
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

		//Fail out if we don't have it
		if(lookahead.tok != L_PAREN){
			print_parse_message(PARSE_ERROR, "Left parenthesis expected after else if statement", current_line);
			num_errors++;
			//Create and return an error node
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Push onto the stack for matching later
		push_token(grouping_stack, lookahead);
	
		//We now need to see a valid conditional expression
		generic_ast_node_t* else_if_expression_node = logical_or_expression(fl);

		//If we see an invalid one
		if(else_if_expression_node->CLASS == AST_NODE_CLASS_ERR_NODE){
			print_parse_message(PARSE_ERROR, "Invalid conditional expression given as else if statement condition", current_line);
			num_errors++;
			//It's already an error so just return it
			return else_if_expression_node;
		}

		/**
		 * The expression of this type must be compatible at the very list with a u_int64
		 */
		symtab_type_record_t* int_type = lookup_type(type_symtab, "u64");

		//If it's not of this type or a compatible type(pointer, smaller int, etc, it is out)
		if(else_if_expression_node->inferred_type->type_class != TYPE_CLASS_POINTER && types_compatible(int_type->type, else_if_expression_node->inferred_type) == NULL){
			sprintf(info, "If statements require an int or pointer type to be in their condition, but was given type \"%s\"", else_if_expression_node->inferred_type->type_name);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Following the expression we need to see a closing paren
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

		//If we don't see the R_Paren
		if(lookahead.tok != R_PAREN){
			print_parse_message(PARSE_ERROR, "Right parenthesis expected after expression in else-if statement", current_line);
			num_errors++;
			//Return an error node
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Now let's check the stack, we need to have matching ones here
		if(pop_token(grouping_stack).tok != L_PAREN){
			print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected", current_line);
			num_errors++;
			//Return an error node
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//If we make it to this point, we need to see the THEN keyword
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

		//Fail out if bad
		if(lookahead.tok != THEN){
			print_parse_message(PARSE_ERROR, "then keyword expected following expression in else if statement", current_line);
			num_errors++;
			//Return an error node
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//If we make it here, we should be safe to add the conditional as an expression
		add_child_node(else_if_node, else_if_expression_node);

		//Now following this, we need to see a valid compound statement
		generic_ast_node_t* else_if_compound_stmt_node = compound_statement(fl);

		//If this node fails, whole thing is bad
		if(else_if_compound_stmt_node->CLASS == AST_NODE_CLASS_ERR_NODE){
			num_errors++;
			//It's already an error, so just send it back up
			return else_if_compound_stmt_node;
		}

		//Add the compound statement as a child to this node
		add_child_node(else_if_node, else_if_compound_stmt_node);

		//And now that we know everything is good, we can add the else if node as a child
		//to the if node
		add_child_node(if_stmt, else_if_node);
	
		//Refresh the lookahead tokens for the next round
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
		lookahead2 = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	}

	//If we get here, at the very least we know that lookahead2 is bad, so we'll put him back
	push_back_token(lookahead2);

	//We could've still had an else block here, so we'll check and handle it if we do
	if(lookahead.tok == ELSE){
		//We'll now handle the compound statement that comes with this
		generic_ast_node_t* else_compound_stmt = compound_statement(fl);
		
		//Let's see if it worked
		if(else_compound_stmt->CLASS == AST_NODE_CLASS_ERR_NODE){
			//It failed, send it up the chain
			return else_compound_stmt;
		}

		//Otherwise it worked, so add it in
		add_child_node(if_stmt, else_compound_stmt);
	} else {
		//Otherwise there was no else token, so put it back
		push_back_token(lookahead);
	}
	
	//Store the line number
	if_stmt->line_number = current_line;

	//Once we reach the end, return the root level node
	return if_stmt;
}


/**
 * A jump statement allows us to instantly relocate in the memory map of the program. Like all rules, 
 * a jump statement returns a reference to the root node that it created
 *
 * NOTE: By the time we get here, we will have already consumed the jump token
 *
 * All checking for jump statements has to occur after we add labels in. We cannot check if a label
 * is valid before the function is fully processed. As such, we add all of these jump statements into a
 * queue for processing
 *
 * BNF Rule: <jump-statement> ::= jump <label-identifier>;
 */
static generic_ast_node_t* jump_statement(FILE* fl){
	//Lookahead token
	Lexer_item lookahead;

	//We can off the bat create the jump statement node here
	generic_ast_node_t* jump_stmt = ast_node_alloc(AST_NODE_CLASS_JUMP_STMT); 

	//Once we've made it, we need to see a valid label identifier
	generic_ast_node_t* label_ident = label_identifier(fl);

	//If this failed, we're done
	if(label_ident->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid label given to jump statement", parser_line_num);
		num_errors++;
		//It's already an error, just give it back
		return label_ident;
	}

	//One last tripping point befor we create the node, we do need to see a semicolon
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we don't see a semicolon we bail
	if(lookahead.tok != SEMICOLON){
		print_parse_message(PARSE_ERROR, "Semicolon required after jump statement", parser_line_num);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}
	
	//Otherwise if we get here we know that it is a valid label and valid syntax
	//We'll now do the final assembly
	//First we'll add the label ident as a child of the jump
	add_child_node(jump_stmt, label_ident);

	//Store the line number
	jump_stmt->line_number = parser_line_num;

	//Add this jump statement into the queue for processing
	enqueue(current_function_jump_statements, jump_stmt); 

	//Finally we'll give back the root reference
	return jump_stmt;
}


/**
 * A continue statement is also related to a jump statement. Ollie language gives support for
 * conditional continues, and that is reflected in the BNF for this rule. Like all rules, this
 * function returns a reference to the root node that it created
 *
 * NOTE: By the time we get here, we will have already seen and consumed the continue keyword 
 *
 * BNF Rule: <continue-statement> ::=  continue {when(<conditional-expression>)}?; 
 *
 *
 * EXAMPLE CONTINUE WHEN USAGE
 *
 * -> continue when (i < 2); -> This will have an expression as it's one and only child 
 */
static generic_ast_node_t* continue_statement(FILE* fl){
	//Lookahead token
	Lexer_item lookahead;

	//Once we get here, we've already seen the continue keyword, so we can make the node
	generic_ast_node_t* continue_stmt = ast_node_alloc(AST_NODE_CLASS_CONTINUE_STMT);
	//Store the line number
	continue_stmt->line_number = parser_line_num;

	//Let's see what comes after this. If it's a semicol, we get right out
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If it's a semicolon we're done
	if(lookahead.tok == SEMICOLON){
		return continue_stmt;
	}

	//Otherwise, it has to have been a when keyword, so if it's not we have an error
	if(lookahead.tok != WHEN){
		print_parse_message(PARSE_ERROR, "Semicolon expected after continue statement", parser_line_num);
		num_errors++;
		//Return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}
	
	//If we get down here, we know that we are seeing a continue when statement
	//We now need to see an lparen
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we don't have one, it's an instant fail
	if(lookahead.tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Parenthesis expected after continue when keywords", parser_line_num);
		num_errors++;
		//Return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Push to the stack for grouping
	push_token(grouping_stack, lookahead);

	//Now we need to see a valid conditional expression
	generic_ast_node_t* expr_node = logical_or_expression(fl);

	//If it failed, we also fail
	if(expr_node->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid conditional expression given to continue when statement", parser_line_num);
		num_errors++;
		//It's already an error so we'll just give it back
		return expr_node;
	}

	//If this worked, we add it under the continue node
	add_child_node(continue_stmt, expr_node);

	//We need to now see a closing paren
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we don't see it fail out
	if(lookahead.tok != R_PAREN){
		print_parse_message(PARSE_ERROR, "Closing paren expected after when clause",  parser_line_num);
		num_errors++;
		//Return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Check for matching next
	if(pop_token(grouping_stack).tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected", parser_line_num);
		num_errors++;
		//Return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Finally if we make it all the way down here, we need to see a semicolon
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	if(lookahead.tok != SEMICOLON){
		print_parse_message(PARSE_ERROR, "Semicolon expected after statement", parser_line_num);
		num_errors++;
		//Return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//If we make it all the way down here, it worked so we can return the root node
	return continue_stmt;
}


/**
 * A break statement is also related to a jump statement. Ollie language gives support for
 * conditional breaks, and that is reflected in the BNF for this rule. Like all rules, this
 * function returns a reference to the root node that it created
 *
 * NOTE: By the time we get here, we will have already seen and consumed the break keyword 
 *
 * BNF Rule: <break-statement> ::=  break {when(<conditional-expression>)}?; 
 *
 * EXAMPLE BREAK WHEN USAGE:
 * -> break when (i < 2); -> This will have an expression right under its statement
 */
static generic_ast_node_t* break_statement(FILE* fl){
	//Lookahead token
	Lexer_item lookahead;

	//Once we get here, we've already seen the break keyword, so we can make the node
	generic_ast_node_t* break_stmt = ast_node_alloc(AST_NODE_CLASS_BREAK_STMT);
	//Store the line number
	break_stmt->line_number = parser_line_num;

	//Let's see what comes after this. If it's a semicol, we get right out
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If it's a semicolon we're done
	if(lookahead.tok == SEMICOLON){
		return break_stmt;
	}

	//Otherwise, it has to have been a when keyword, so if it's not we have an error
	if(lookahead.tok != WHEN){
		print_parse_message(PARSE_ERROR, "Semicolon expected after break statement", parser_line_num);
		num_errors++;
		//Return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}
	
	//If we get down here, we know that we are seeing a break when statement
	//We now need to see an lparen
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we don't have one, it's an instant fail
	if(lookahead.tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Parenthesis expected after break when keywords", parser_line_num);
		num_errors++;
		//Return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Push to the stack for grouping
	push_token(grouping_stack, lookahead);

	//Now we need to see a valid expression
	generic_ast_node_t* expr_node = logical_or_expression(fl);

	//If it failed, we also fail
	if(expr_node->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid conditional expression given to break when statement", parser_line_num);
		num_errors++;
		//It's already an error so we'll just give it back
		return expr_node;
	}

	//This is the first child of the if statement node
	add_child_node(break_stmt, expr_node);

	//We need to now see a closing paren
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we don't see it fail out
	if(lookahead.tok != R_PAREN){
		print_parse_message(PARSE_ERROR, "Closing paren expected after when clause",  parser_line_num);
		num_errors++;
		//Return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Check for matching next
	if(pop_token(grouping_stack).tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected", parser_line_num);
		num_errors++;
		//Return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Finally if we make it all the way down here, we need to see a semicolon
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	if(lookahead.tok != SEMICOLON){
		print_parse_message(PARSE_ERROR, "Semicolon expected after statement", parser_line_num);
		num_errors++;
		//Return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//If we make it all the way down here, it worked so we can return the root node
	return break_stmt;
}


/**
 * A return statement removes us from whatever function we are currently in. It can optionally
 * have an expression after it. Like all rules, a return statement returns a reference to the root
 * node that it created
 *
 * NOTE: By the time we get here, we will have already consumed the ret keyword
 *
 * BNF Rule: <return-statement> ::= ret {<logical-or-expression>}?;
 */
static generic_ast_node_t* return_statement(FILE* fl){
	//For error printing
	char info[1500];
	//Lookahead token
	Lexer_item lookahead;

	//We can create the node now
	generic_ast_node_t* return_stmt = ast_node_alloc(AST_NODE_CLASS_RET_STMT);

	//Now we can optionally see the semicolon immediately. Let's check if we have that
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we see a semicolon, we can just leave
	if(lookahead.tok == SEMICOLON){
		//If this is the case, the return type had better be void
		if(strcmp(current_function->return_type->type_name, "void") != 0){
			sprintf(info, "Function \"%s\" expects a return type of \"%s\", not \"void\". Empty ret statements not allowed", current_function->func_name, current_function->return_type->type_name);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			//Also print the function name
			print_function_name(current_function);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//If we get out then we're fine
		
		return return_stmt;

	} else {
		//If we get here, but we do expect a void return, then this is an issue
		if(strcmp(current_function->return_type->type_name, "void") == 0){
			sprintf(info, "Function \"%s\" expects a return type of \"void\". Use \"ret;\" for return statements in this function", current_function->func_name);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			//Also print the function name
			print_function_name(current_function);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}
		//Put it back if no
		push_back_token(lookahead);
	}

	//Otherwise if we get here, we need to see a valid conditional expression
	generic_ast_node_t* expr_node = logical_or_expression(fl);

	//If this is bad, we fail out
	if(expr_node->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid conditional expression given to return statement", parser_line_num);
		num_errors++;
		//It's already an error, so we'll just return it
		return expr_node;
	}

	//Let's do some type checking here
	if(current_function == NULL){
		print_parse_message(PARSE_ERROR, "Fatal internal compiler error. Saw a return statement while current function is null", parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//If the current function's return type is not compatible with the return type here, we'll bail out
	if(types_compatible(current_function->return_type, expr_node->inferred_type) == NULL){
		sprintf(info, "Function \"%s\" expects a return type of \"%s\", but was given an incompatible type \"%s\"", current_function->func_name, current_function->return_type->type_name,
		  		expr_node->inferred_type->type_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the function
		print_function_name(current_function);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Otherwise it worked, so we'll add it as a child of the other node
	add_child_node(return_stmt, expr_node);

	//After the conditional, we just need to see a semicolon
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//Fail case
	if(lookahead.tok != SEMICOLON){
		print_parse_message(PARSE_ERROR, "Semicolon expected after return statement", parser_line_num);
		num_errors++;
		//Return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Add in the line number
	return_stmt->line_number = parser_line_num;
	
	//If we have deferred statements
	if(deferred_stmts_node != NULL){
		//Then we'll duplicate
		generic_ast_node_t* deferred_stmts = duplicate_subtree(deferred_stmts_node);

		//This node will now come before the ret statement
		deferred_stmts->next_sibling = return_stmt;

		//We'll give this back instead
		return deferred_stmts;
	} else {
		//If we get here we're all good, just return the parent
		return return_stmt;
	}
}


/**
 * A branch statement is an entirely abstract rule that multiplexes for us based on what it sees.
 * Like all rules, it returns a reference to the root node that it creates, but that root node will
 * not be created here
 *
 * If we get here, it will have been because the caller has seen a jump, continue, break or return statement.
 * They will have pushed that token back for us to multiplex on here
 *
 * BNF Rule: <branch-statement> ::= <jump-statement> 
 * 								  | <continue-statement> 
 * 								  | <break-statement> 
 * 								  | <return-statement>
 */
static generic_ast_node_t* branch_statement(FILE* fl){
	//The lookahead token
	Lexer_item lookahead;

	//Let's see what we have
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//We'll now switch based on which token we have
	switch (lookahead.tok) {
		case JUMP:
			return jump_statement(fl);
		case RET:
			return return_statement(fl);
		case BREAK:
			return break_statement(fl);
		case CONTINUE:
			return continue_statement(fl);
		//This should never occur
		default:
			//For developer, something very wrong if this occurs
			print_parse_message(PARSE_ERROR, "Fatal internal compiler error in branch statement",  parser_line_num);
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}
}


/**
 * A switch statement allows us to to see one or more labels defined by a certain expression. It allows
 * for the use of labeled statements followed by statements in general. We will do more static analysis
 * on this later. Like all rules in the system, this function returns the root node that it creates
 *
 * NOTE: The caller has already consumed the switch keyword by the time we get here
 *
 * BNF Rule: <switch-statement> ::= switch on( <logical-or-expression> ) { {<case-statement | default-statement>}+ }
 */
static generic_ast_node_t* switch_statement(FILE* fl){
	//For error printing
	char info[ERROR_SIZE];
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	//Lookahead token
	Lexer_item lookahead;
	//Wipe this, it's no longer valid
	found_default_clause = 0;

	//Ollie language does not allow for nested switch statements. If this happens, we fail out
	if(current_switch_statement_type != NULL){
		print_parse_message(PARSE_ERROR, "Nested switch statements are not supported", parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//We've already seen the switch keyword, so now we have to see the on keyword
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//Fail case
	if(lookahead.tok != ON){
		print_parse_message(PARSE_ERROR, "on keyword expected after switch in switch statement", current_line);
		num_errors++;
		//Return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Once we get here, we can allocate the root level node
	generic_ast_node_t* switch_stmt_node = ast_node_alloc(AST_NODE_CLASS_SWITCH_STMT);

	//Now we must see an lparen
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//Fail case
	if(lookahead.tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Left parenthesis expected after on keyword", current_line);
		num_errors++;
		//Return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Push to stack for later matching
	push_token(grouping_stack, lookahead);

	//Now we must see a valid conditional expression
	generic_ast_node_t* expr_node = logical_or_expression(fl);

	//If we see an invalid one we fail right out
	if(expr_node->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid conditional expression provided to switch on", current_line);
		num_errors++;
		//It's already an error, so just send this up
		return expr_node;
	}
	
	//For a switch statement, we need an enum or some other kind of numeric type to switch based on. We cannot switch on
	//types like floats, etc
	
	//Grab the type info out
	generic_type_t* type = expr_node->inferred_type;

	//Let's see what kind of type we have here. If it isn't a basic type, it MUST be an enum
	if(type->type_class != TYPE_CLASS_BASIC){
		//Error out here
		if(type->type_class != TYPE_CLASS_ENUMERATED){
			sprintf(info, "Type \"%s\" cannot be switched", type->type_name);
			print_parse_message(PARSE_ERROR, info, expr_node->line_number);
			num_errors++;
			//Fail out
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}
	//Otherwise, it essentially needs to be an int or a char. Nothing else here is "switchable"	
	} else {
		//Grab the basic type
		Token basic_type = type->basic_type->basic_type;

		//It needs to be an int or char
		if(basic_type == VOID || basic_type == FLOAT32 || basic_type == FLOAT64){
			sprintf(info, "Type \"%s\" cannot be switched", type->type_name);
			print_parse_message(PARSE_ERROR, info, expr_node->line_number);
			num_errors++;
			//Fail out
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}
	}

	//Otherwise it's fine, set this for statements down the line
	current_switch_statement_type = type;

	//Since we know it's valid, we can add this in as a child
	add_child_node(switch_stmt_node, expr_node);

	//Now we must see a closing paren
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//Fail case
	if(lookahead.tok != R_PAREN){
		print_parse_message(PARSE_ERROR, "Right parenthesis expected after expression in switch statement", current_line);
		num_errors++;
		//Create and return an error
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Check to make sure that the parenthesis match up
	if(pop_token(grouping_stack).tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected", current_line);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now we must see an lcurly to begin the actual block
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//Fail case
	if(lookahead.tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Left curly brace expected after expression", current_line);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Push to stack for later matching
	push_token(grouping_stack, lookahead);

	/**
	 * IMPORTANT NOTE: Once we get here, we have officially reached a new lexical scope. To officiate this, 
	 * we will initialize a new scope for both the variable and type symtabs. This scope will be finalized once
	 * we leave the switch statement
	 */
	initialize_type_scope(type_symtab);
	initialize_variable_scope(variable_symtab);

	//Now we can see as many expressions as we'd like. We'll keep looking for expressions so long as
	//our lookahead token is not an R_CURLY. We'll use a do-while for this, because Ollie language requires
	//that switch statements have at least one thing in them

	//Seed our search here
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	//Is this statement occupied? Set this flag if no
	u_int8_t is_empty = 1;
	//Handle our statement here
	generic_ast_node_t* stmt;

	//So long as we don't see a right curly
	while(lookahead.tok != R_CURLY){
		//We need to see a valid case or default statement
		if(lookahead.tok == CASE){
			//Handle a case statement here
			stmt = case_statement(fl);

			//If it fails, then we're done
			if(stmt->CLASS == AST_NODE_CLASS_ERR_NODE){
				return stmt;
			}
			
		} else if(lookahead.tok == DEFAULT){
			//Handle a default statement
			stmt = default_statement(fl);

			//If it fails, then we're done
			if(stmt->CLASS == AST_NODE_CLASS_ERR_NODE){
				return stmt;
			}

		//We fail out here -- something went wrong
		} else {
			print_parse_message(PARSE_ERROR, "\"case\" or \"default\" keywords expected", parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//If we get here we know it worked, so we can add it in as a child
		add_child_node(switch_stmt_node, stmt);

		//No longer empty
		is_empty = 0;

		//Refresh the lookahead token
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	}

	//If we haven't found a default clause, it's a failure
	if(found_default_clause == 0){
		print_parse_message(PARSE_ERROR, "Switch statements are required to have a \"default\" clause", current_line);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}	

	//If we have an entirely empty switch statement
	if(is_empty == 1){
		print_parse_message(WARNING, "Switch statement is empty, has no effect", current_line);
		num_warnings++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//By the time we reach this, we should have seen a right curly
	//However, we could still have matching issues, so we'll check for that here
	if(pop_token(grouping_stack).tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Unmatched curly braces detected", current_line);
		num_errors++;
		//Return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now once we get here, we will finalize the variable and type scopes that we had initialized beforehand
	finalize_type_scope(type_symtab);
	finalize_variable_scope(variable_symtab);

	//Return the line number
	switch_stmt_node->line_number = current_line;
	//Reset this back to NULL for who comes next
	current_switch_statement_type = NULL;

	//If we make it here, all went well
	return switch_stmt_node;
}


/**
 * A while statement simply ensures that the check is executed before the body. Like all other rules, this
 * function returns a reference to the root node of the subtree that it creates
 *
 * NOTE: By the time that we make it here, we assume that we have already seen the while keyword
 *
 * BNF Rule: <while-statement> ::= while( <logical-or-expression> ) do <compound-statement> 
 */
static generic_ast_node_t* while_statement(FILE* fl){
	//For error printing
	char info[ERROR_SIZE];
	//The lookahead token
	Lexer_item lookahead;
	//Freeze the line number
	u_int16_t current_line = parser_line_num;

	//First create the actual node
	generic_ast_node_t* while_stmt_node = ast_node_alloc(AST_NODE_CLASS_WHILE_STMT);

	//We already have seen the while keyword, so now we need to see parenthesis surrounding a conditional expression
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	
	//Fail out if we don't see
	if(lookahead.tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Left parenthesis expected after while keyword", parser_line_num);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Push it to the stack for later matching
	push_token(grouping_stack, lookahead);

	//Now we need to see a valid conditional block in here
	generic_ast_node_t* conditional_expr = logical_or_expression(fl);

	//Fail out if this happens
	if(conditional_expr->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid expression in while statement",  parser_line_num);
		num_errors++;
		//It's already an error so just give this back
		return conditional_expr;
	}

	/**
	 * The expression of this type must be compatible at the very list with a u_int64
	 */
	symtab_type_record_t* int_type = lookup_type(type_symtab, "u64");

	//If it's not of this type or a compatible type(pointer, smaller int, etc, it is out)
	if(conditional_expr->inferred_type->type_class != TYPE_CLASS_POINTER && types_compatible(int_type->type, conditional_expr->inferred_type) == NULL){
		sprintf(info, "While statements require an int or pointer type to be in their condition, but was given type \"%s\"", conditional_expr->inferred_type->type_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Otherwise we know it's good so we can add it in as a child
	add_child_node(while_stmt_node, conditional_expr);

	//After this point we need to see a right paren
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//Fail if we don't see it
	if(lookahead.tok != R_PAREN){
		print_parse_message(PARSE_ERROR, "Expected right parenthesis after conditional expression",  parser_line_num);
		num_errors++;
		//Create and give back an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//We also need to check for matching
	if(pop_token(grouping_stack).tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected", parser_line_num);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now that we've made it all the way here, we need to see the do keyword
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//Instant fail if not
	if(lookahead.tok != DO){
		print_parse_message(PARSE_ERROR, "Do keyword expected before compound expression in while statement", parser_line_num);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Following this, we need to see a valid compound statement, and then we're done
	generic_ast_node_t* compound_stmt_node = compound_statement(fl);

	//If this is invalid we fail
	if(compound_stmt_node->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid compound statement in while expression", parser_line_num);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Otherwise we'll add it in as a child
	add_child_node(while_stmt_node, compound_stmt_node);
	//Store the current line number
	while_stmt_node->line_number = current_line;

	//And we'll return the root reference
	return while_stmt_node;
}


/**
 * A do-while statement ensures that the body is executes once before the condition is checked. Like all other
 * rules, this function returns a reference to the root node of the subtree that it creates
 *
 * NOTE: By the time we get here, we assume that we've already seen the "do" keyword
 *
 * BNF Rule: <do-while-statement> ::= do <compound-statement> while( <logical-or-expression> );
 */
static generic_ast_node_t* do_while_statement(FILE* fl){
	//For error printing
	char info[ERROR_SIZE];
	//Freeze the current line number
	u_int16_t current_line = parser_line_num;
	//Lookahead token
	Lexer_item lookahead;

	//Let's first create the overall global root node
	generic_ast_node_t* do_while_stmt_node = ast_node_alloc(AST_NODE_CLASS_DO_WHILE_STMT);

	//Remember by the time that we've gotten here, we have already seen the do keyword
	//Let's first find a valid compound statement
	generic_ast_node_t* compound_stmt = compound_statement(fl);

	//If we fail, then we are done here
	if(compound_stmt->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid compound statement given to do-while statement", current_line);
		num_errors++;
		//It's already an error, so just give it back
		return compound_stmt;
	}

	//Otherwise we know that it was valid, so we can add it in as a child of the root
	add_child_node(do_while_stmt_node, compound_stmt);

	//Once we get past the compound statement, we need to now see the while keyword
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we don't see it, instant failure
	if(lookahead.tok != WHILE){
		print_parse_message(PARSE_ERROR, "Expected while keyword after block in do-while statement", parser_line_num);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}
	
	//Once we've made it here, we now need to see a left paren
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	
	//Fail out if we don't see
	if(lookahead.tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Left parenthesis expected after while keyword", parser_line_num);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Push it to the stack for later matching
	push_token(grouping_stack, lookahead);

	//Now we need to see a valid conditional block in here
	generic_ast_node_t* expr_node = logical_or_expression(fl);

	//Fail out if this happens
	if(expr_node->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid expression in while part of do-while statement",  parser_line_num);
		num_errors++;
		//It's already an error so just give this back
		return expr_node;
	}

	/**
	 * The expression of this type must be compatible at the very list with a u_int64
	 */
	symtab_type_record_t* int_type = lookup_type(type_symtab, "u64");

	//If it's not of this type or a compatible type(pointer, smaller int, etc, it is out)
	if(expr_node->inferred_type->type_class != TYPE_CLASS_POINTER && types_compatible(int_type->type, expr_node->inferred_type) == NULL){
		sprintf(info, "Do-while statements require an int or pointer type to be in their condition, but was given type \"%s\"", expr_node->inferred_type->type_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}
	//Otherwise we know it's good so we can add it in as a child
	add_child_node(do_while_stmt_node, expr_node);

	//After this point we need to see a right paren
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//Fail if we don't see it
	if(lookahead.tok != R_PAREN){
		print_parse_message(PARSE_ERROR, "Expected right parenthesis after conditional expression",  parser_line_num);
		num_errors++;
		//Create and give back an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//We also need to check for matching
	if(pop_token(grouping_stack).tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected", parser_line_num);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Finally we need to see a semicolon
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we don't see one, final chance to fail
	if(lookahead.tok != SEMICOLON){
		print_parse_message(PARSE_ERROR, "Semicolon expected at the end of do while statement", parser_line_num);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}
	//Store the line number
	do_while_stmt_node->line_number = current_line;
	
	//Otherwise if we made it here, everything went well
	return do_while_stmt_node;
}


/**
 * A for statement is the classic for loop that you'd expect. Like all other rules, this rule returns
 * a reference to the root of the subtree that it creates
 * 
 * NOTE: By the the time we get here, we assume that we've already seen the "for" keyword
 *
 * BNF Rule: <for-statement> ::= for( {<assignment-expression> | <let-statement>}? ; <logical-or-expression> ; {<assignment-expression>}? ) do <compound-statement>
 */
static generic_ast_node_t* for_statement(FILE* fl){
	//Freeze the current line number
	u_int16_t current_line = parser_line_num; 
	//Lookahead token
	Lexer_item lookahead;

	//We've already seen the for keyword, so let's create the root level node
	generic_ast_node_t* for_stmt_node = ast_node_alloc(AST_NODE_CLASS_FOR_STMT);

	//We now need to first see a left paren
 	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we don't see it, instantly fail out
	if(lookahead.tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Left parenthesis expected after for keyword", parser_line_num);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Push to the stack for later matching
	push_token(grouping_stack, lookahead);

	/**
	 * Important note: The parenthesized area of a for statement represents a new lexical scope
	 * for variables. As such, we will initialize a new variable scope when we get here
	 */
	initialize_variable_scope(variable_symtab);

	//Now we have the option of seeing an assignment expression, a let statement, or nothing
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//We could also see the let keyword for a let_stmt
	if(lookahead.tok == LET){
		//On the contrary, the let statement rule assumes that let has already been consumed, so we won't
		//put it back here, we'll just call the rule
		generic_ast_node_t* let_stmt = let_statement(fl);

		//If it fails, we also fail
		if(let_stmt->CLASS == AST_NODE_CLASS_ERR_NODE){
			print_parse_message(PARSE_ERROR, "Invalid let statement given to for loop", current_line);
			num_errors++;
			//It's already an error, so just give it back
			return let_stmt;
		}

		//Create the wrapper node for CFG creation later on
		generic_ast_node_t* for_loop_cond_node = ast_node_alloc(AST_NODE_CLASS_FOR_LOOP_CONDITION);
		//Add this in as a child
		add_child_node(for_loop_cond_node, let_stmt);

		//Otherwise if we get here it worked, so we'll add it in as a child
		add_child_node(for_stmt_node, for_loop_cond_node);
		
		//Remember -- let statements handle semicolons for us, so we don't need to check

	//Otherwise it had to be a semicolon, so if it isn't we fail
	} else if(lookahead.tok != SEMICOLON){
		//If it isn't a semicolon, then we must have some kind of assignment op here
		//Push the token back
		push_back_token(lookahead);

		//Let the assignment expression handle this
		generic_ast_node_t* asn_expr = assignment_expression(fl);

		//If it fails, we fail too
		if(asn_expr->CLASS == AST_NODE_CLASS_ERR_NODE){
			print_parse_message(PARSE_ERROR, "Invalid assignment expression given to for loop", current_line);
			num_errors++;
			//It's already an error, so just give it back
			return asn_expr;
		}

		//This actually must be an assignment expression, so if it isn't we fail 
		if(asn_expr->CLASS != AST_NODE_CLASS_ASNMNT_EXPR){
			print_parse_message(PARSE_ERROR, "Invalid assignment expression given to for loop", current_line);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Create the wrapper node for CFG creation later on
		generic_ast_node_t* for_loop_cond_node = ast_node_alloc(AST_NODE_CLASS_FOR_LOOP_CONDITION);
		//Add this in as a child
		add_child_node(for_loop_cond_node, asn_expr);

		//Otherwise it worked, so we'll add it in as a child
		add_child_node(for_stmt_node, for_loop_cond_node);

		//We'll refresh the lookahead for the eventual next step
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

		//The assignment expression won't check semicols for us, so we'll do it here
		if(lookahead.tok != SEMICOLON){
			print_parse_message(PARSE_ERROR, "Semicolon expected in for statement declaration", parser_line_num);
			num_errors++;
			//Create and return an error node
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

	//Just add in a blank node as a placeholder
	} else {
		generic_ast_node_t* for_loop_cond_node = ast_node_alloc(AST_NODE_CLASS_FOR_LOOP_CONDITION);
		add_child_node(for_stmt_node, for_loop_cond_node);
	}

	//Now we're in the middle of the for statement. We can optionally see a conditional expression here
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If it's not a semicolon, we need to see a valid conditional expression
	if(lookahead.tok != SEMICOLON){
		//Push whatever it is back
		push_back_token(lookahead);

		//Let this rule handle it
		generic_ast_node_t* expr_node = logical_or_expression(fl);

		//If it fails, we fail too
		if(expr_node->CLASS == AST_NODE_CLASS_ERR_NODE){
			print_parse_message(PARSE_ERROR, "Invalid conditional expression in for loop middle", parser_line_num);
			num_errors++;
			//It's already an error, so just send it up
			return expr_node;
		}

		//Create the wrapper node for CFG creation later on
		generic_ast_node_t* for_loop_cond_node = ast_node_alloc(AST_NODE_CLASS_FOR_LOOP_CONDITION);
		//Add this in as a child
		add_child_node(for_loop_cond_node, expr_node);

		//Otherwise it did work, so we'll add it as a child node
		add_child_node(for_stmt_node, for_loop_cond_node);

		//Now once we get here, we need to see a valid semicolon
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	
		//If it isn't one, we fail out
		if(lookahead.tok != SEMICOLON){
			print_parse_message(PARSE_ERROR, "Semicolon expected after conditional expression in for loop", parser_line_num);
			num_errors++;
			//Return an error node
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}
	//Create a blank node as a placeholder
	} else {
		print_parse_message(PARSE_ERROR, "For loops must have the second condition occupied", parser_line_num);
		num_errors++;
		//Return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Once we make it here, we know that either the inside was blank and we saw a semicolon or it wasn't and we saw a valid conditional 
	
	//As our last step, we can see another conditional expression. If the lookahead isn't a rparen, we must see one
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If it isn't an R_PAREN
	if(lookahead.tok != R_PAREN){
		//Put it back
		push_back_token(lookahead);

		//We now must see a valid conditional
		//Let this rule handle it
		generic_ast_node_t* expr_node = assignment_expression(fl);

		//If it fails, we fail too
		if(expr_node->CLASS == AST_NODE_CLASS_ERR_NODE){
			print_parse_message(PARSE_ERROR, "Invalid conditional expression in for loop", parser_line_num);
			num_errors++;
			//It's already an error, so just send it up
			return expr_node;
		}

		//Create the wrapper node for CFG creation later on
		generic_ast_node_t* for_loop_cond_node = ast_node_alloc(AST_NODE_CLASS_FOR_LOOP_CONDITION);
		//Add this in as a child
		add_child_node(for_loop_cond_node, expr_node);

		//Otherwise it did work, so we'll add it as a child node
		add_child_node(for_stmt_node, for_loop_cond_node);

		//We'll refresh the lookahead for our search here
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	//Create a blank node here as a placeholder
	} else {
		generic_ast_node_t* for_loop_cond_node = ast_node_alloc(AST_NODE_CLASS_FOR_LOOP_CONDITION);
		add_child_node(for_stmt_node, for_loop_cond_node);
	}

	//Now if we make it down here no matter what it must be an R_Paren
	if(lookahead.tok != R_PAREN){
		print_parse_message(PARSE_ERROR, "Right parenthesis expected after for loop declaration", parser_line_num);
		num_errors++;
		//Return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now check for matching
	if(pop_token(grouping_stack).tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected", parser_line_num);
		num_errors++;
		//Return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}
	
	//Now we need to see the do keyword
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If it isn't a do keyword, we fail here
	if(lookahead.tok != DO){
		print_parse_message(PARSE_ERROR, "Do keyword expected after for loop declaration", parser_line_num);
		num_errors++;
		//Return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now that we're all done, we need to see a valid compound statement
	generic_ast_node_t* compound_stmt_node = compound_statement(fl);

	//If it's invalid, we'll fail out here
	if(compound_stmt_node->CLASS == AST_NODE_CLASS_ERR_NODE){
		//No error message, just pass the failure up
		return compound_stmt_node;
	}

	//Otherwise if we make it here, we know that it worked so we'll add it as a child
	add_child_node(for_stmt_node, compound_stmt_node);

	//At the end, we'll finalize the lexical scope
	finalize_variable_scope(variable_symtab);
	//Store the line number
	for_stmt_node->line_number = current_line;

	//It all worked here, so we'll return the root
	return for_stmt_node;
}


/**
 * A compound statement is denoted by the {} braces, and can decay in to 
 * statements and declarations. It also represents the start of a brand new
 * lexical scope for types and variables. Like all rules, this rule returns
 * a reference to the root node that it creates
 *
 * NOTE: We assume that we have NOT consumed the { token by the time we make
 * it here
 *
 * BNF Rule: <compound-statement> ::= {{<declaration>}* {<statement>}* {<definition>}*}
 */
static generic_ast_node_t* compound_statement(FILE* fl){
	//Lookahead token
	Lexer_item lookahead;

	//We must first see a left curly
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	
	//If we don't see one, we fail out
	if(lookahead.tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Left curly brace required at beginning of compound statement", parser_line_num);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Push onto the grouping stack so we can check matching
	push_token(grouping_stack, lookahead);

	//Now if we make it here, we're safe to create the actual node
	generic_ast_node_t* compound_stmt_node = ast_node_alloc(AST_NODE_CLASS_COMPOUND_STMT);
	//Store the line number here
	compound_stmt_node->line_number = parser_line_num;

	//Begin a new lexical scope for types and variables
	initialize_type_scope(type_symtab);
	initialize_variable_scope(variable_symtab);

	//Now we can keep going until we see a closing curly
	//We'll seed the search
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//So long as we don't reach the end
	while(lookahead.tok != R_CURLY){
		//We can choose between a declaration or a statement
		//All these keywords indicate a declaraion
		if(lookahead.tok == DECLARE || lookahead.tok == LET){
			//We'll let the actual rule handle it, so push the token back
			push_back_token(lookahead);

			//We now need to see a valid version
			generic_ast_node_t* declaration_node = declaration(fl);

			//If it's invalid, we pass right through, no error printing
			if(declaration_node->CLASS == AST_NODE_CLASS_ERR_NODE){
				//It's already an error, so just send it back up
				return declaration_node;
			}

			//Otherwise it's worked just fine, so we'll add it in as a child
			add_child_node(compound_stmt_node, declaration_node);

		//Otherwise we can see a definition of some kind. Definitions are compiler-only
		//directives, and as such produce no nodes
		} else if(lookahead.tok == DEFINE || lookahead.tok == ALIAS){
			//Put the token back
			push_back_token(lookahead);

			//Let's see if it worked
			u_int8_t status = definition(fl);

			//If we fail here we'll throw an error
			if(status == FAILURE){
				//Return an error node
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}

			//There is nothing to add into here, again these are 100% compiler directives

		//Otherwise, we need to see a statement of some kind
		} else {
			//Put whatever we saw back
			push_back_token(lookahead);
			
			//We now need to see a valid statement
			generic_ast_node_t* stmt_node = statement(fl);

			//If it's null(which is possible) we just move along
			if(stmt_node == NULL){
				goto loop_end;
			}

			//If it's invalid we'll pass right through, no error printing
			if(stmt_node->CLASS == AST_NODE_CLASS_ERR_NODE){
				//Send it right back
				return stmt_node;
			}

			//Otherwise, we'll add it as a child node
			add_child_node(compound_stmt_node, stmt_node);
		}

	loop_end:
		//Whatever happened, once we get here we need to refresh the lookahead
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	}

	//Once we've escaped out of the while loop, we know that the token we currently have
	//is an R_CURLY
	//We still must check for matching
	if(pop_token(grouping_stack).tok != L_CURLY){
		printf("%d\n", lookahead.tok);
		print_parse_message(PARSE_ERROR, "Unmatched curly braces detected", parser_line_num);
		num_errors++;
		//Return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Otherwise, we've reached the end of the new lexical scope that we made. As such, we'll
	//"finalize" both of these scopes
	finalize_type_scope(type_symtab);
	finalize_variable_scope(variable_symtab);
	//Add in the line number
	compound_stmt_node->line_number = parser_line_num;

	//And we're all done, so we'll return the reference to the root node
	return compound_stmt_node;
}


/**
 * Assembly inline statements allow the programmer to write assembly
 * directly into a file. This assembly will be inserted, in the exact logical control 
 * flow where it came from, and will not be altered or analyzed by oc.
 *
 * Recall: By the time that we reach this, we'll have already seen the asm
 * keyword
 *
 * BNF Rule: <assembly-statement> ::= #asm{{assembly-statement}+};
 */
static generic_ast_node_t* assembly_inline_statement(FILE* fl){
	//The next token
	Lexer_item lookahead;

	//We must first see an opening curly
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we don't see one, we fail
	if(lookahead.tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Assembly insertion statements must be wrapped in curly braces({})", parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Let's warn the user. Assembly inline statements are a great way to shoot yourself in the foot
	print_parse_message(INFO, "Assembly inline statements are not analyzed by OC. Whatever is written will be executed verbatim. Please double check your assembly statements.", parser_line_num);

	//Otherwise we're presumably good, so we can start hunting for assembly statements
	generic_ast_node_t* assembly_ast_node_t = ast_node_alloc(AST_NODE_CLASS_ASM_INLINE_STMT);
	//Store this too
	assembly_ast_node_t->line_number = parser_line_num;

	//For quick reference, grab out the assembly node in here
	asm_inline_stmt_ast_node_t* asm_node_ref = (asm_inline_stmt_ast_node_t*)(assembly_ast_node_t->node);

	//We keep going here as long as we don't see the closing curly brace
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//So long as we don't see this
	while(lookahead.tok != R_CURLY){
		//Put it back
		push_back_token(lookahead);

		//We'll now need to consume an assembly statement
		lookahead = get_next_assembly_statement(fl, &parser_line_num);

		//If it's an error, we'll fail out here
		if(lookahead.tok == ERROR){
			print_parse_message(PARSE_ERROR, "Unable to parse assembly statement. Did you enclose the whole block in curly braces({})?", parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Otherwise it worked, so we'll need to add the statement in here
		//Let's check -- we may be overrunning our allocate bounds
		if(asm_node_ref->length + lookahead.char_count + 1 >= asm_node_ref->max_length){
			//We'll realloc here and update the max length by doubling it
			asm_node_ref->max_length *= 2;

			//Realloc as needed to keep enough space
			asm_node_ref->asm_line_statements = realloc(asm_node_ref->asm_line_statements, asm_node_ref->max_length);
		}

		//Now we can add whatever the assembly statement that we had before is in
		strcat(asm_node_ref->asm_line_statements, lookahead.lexeme);

		//For readability
		strcat(asm_node_ref->asm_line_statements, "\n");

		//Update the length too
		asm_node_ref->length += lookahead.char_count + 1;

		//Now we'll refresh the lookahead token
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	}

	//Now we just need to see one last thing -- the closing semicolon
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	if(lookahead.tok != SEMICOLON){
		print_parse_message(PARSE_ERROR, "Expected semicolon after assembly statement", parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Once we escape out here, we've seen the whole thing, so we're done
	return assembly_ast_node_t;
}


/**
 * A defer statement allows users to defer execution until after a function occurs
 *
 * Remember: By the time that we get here, we will have already seen the defer keyword
 *
 * TODO UPDATE THIS
 *
 * <defer-statement> ::= defer {<logical-or-expression> | <assembly-inline-statement>};
 */
static generic_ast_node_t* defer_statement(FILE* fl){
	//For searching
	Lexer_item lookahead;

	//Now if we see that this is NULL, we'll allocate here
	if(deferred_stmts_node == NULL){
		deferred_stmts_node = ast_node_alloc(AST_NODE_CLASS_DEFER_STMT);
	}

	//Let's see what we have here
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//We are trying to defer an assembly statement
	if(lookahead.tok == ASM){
		//Let's handle our assembly inline
		generic_ast_node_t* asm_inline = assembly_inline_statement(fl);

		//If it failed, we're done here
		if(asm_inline->CLASS == AST_NODE_CLASS_ERR_NODE){
			print_parse_message(PARSE_ERROR, "Bad assembly inline given for deferral", parser_line_num);
			num_errors++;
			return asm_inline;
		}

		//We always display this warning
		print_parse_message(INFO, "Deferring an assembly statement will likely crash your program if extreme caution is not used.", parser_line_num);

		//Otherwise it's fine. Also recall -- asm statements parse semicolons for us
		asm_inline->line_number = parser_line_num;
		//Mark this as deferred
		asm_inline->is_deferred = 1;

		//Add it in as a child
		add_child_node(deferred_stmts_node, asm_inline);

	//Otherwise we have a regular logical or expression here
	} else {
		//Put it back
		push_back_token(lookahead);
		//We must first see a valid expression statement
		generic_ast_node_t* expr_node = logical_or_expression(fl);

		//We have a bad expression, fail out here
		if(expr_node->CLASS == AST_NODE_CLASS_ERR_NODE){
			print_parse_message(PARSE_ERROR, "Invalid statement given for deferral", parser_line_num);
			num_errors++;
			//It's already an error, just send it up
			return expr_node;
		}
	
		//Grab the next token
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

		//If it isn't a semicolon, we fail out
		if(lookahead.tok != SEMICOLON){
			print_parse_message(PARSE_ERROR, "Defer statement must be terminated with a semicolon", parser_line_num);
			num_errors++;
			//Error out
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Store the line number
		expr_node->line_number = parser_line_num;

		//Mark that this is deferred
		expr_node->is_deferred = 1;

		//Add this in as a child node to the overall deferred statements node
		add_child_node(deferred_stmts_node, expr_node);
	}

	//We return nothing here -- there is no need
	return NULL;
}


/**
 * An idle statement simply inserts one nop statements. This can be
 * used by the programmer to stall a program
 *
 * REMEMBER: We have already seen the stall keyword by the time that we get here
 */
static generic_ast_node_t* idle_statement(FILE* fl){
	//Lookahead token
	Lexer_item lookahead;

	//We just need to see a semicolon now
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If it isn't a semicolon, we error out
	if(lookahead.tok != SEMICOLON){
		print_parse_message(PARSE_ERROR, "Semicolon required after idle keyword", parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Create and populate the node
	generic_ast_node_t* idle_statement = ast_node_alloc(AST_NODE_CLASS_IDLE_STMT);
	idle_statement->line_number = parser_line_num;

	//We'll create and return an idle statement
	return idle_statement;
}


/**
 * A statement is a kind of multiplexing rule that just determines where we need to go to. Like all rules in the parser,
 * this function returns a reference to the the root node that it creates, even though that actual root node is created 
 * further down the chain
 *
 * BNF Rule: <statement> ::= <labeled-statement> 
 * 						   | <expression-statement> 
 * 						   | <compound-statement> 
 * 						   | <if-statement> 
 * 						   | <switch-statement> 
 * 						   | <for-statement> 
 * 						   | <do-while-statement> 
 * 						   | <while-statement> 
 * 						   | <branch-statement>
 * 						   | <assembly-statement>
 * 						   | <defer-statement>
 * 						   | <idle-statement>
 */
static generic_ast_node_t* statement(FILE* fl){
	//Lookahead token
	Lexer_item lookahead;

	//Let's grab the next item and see what we have here
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we see a label ident, we know we're seeing a labeled statement
	if(lookahead.tok == LABEL_IDENT){
		//This rule relies on these tokens, so we'll push them back
		push_back_token(lookahead);
	
		//Just return whatever the rule gives us
		return labeled_statement(fl);
	
	//If we see an L_CURLY, we are seeing a compound statement
	} else if(lookahead.tok == L_CURLY){
		//The rule relies on it, so put it back
		push_back_token(lookahead);

		//Return whatever the rule gives us
		return compound_statement(fl);
	
	//If we see for, we are seeing a for statement
	} else if(lookahead.tok == FOR){
		//This rule relies on for already being consumed, so we won't put it back
		return for_statement(fl);

	//If we see this, we are seeing a defer statment
	} else if(lookahead.tok == DEFER){
		//This rule relies on the defer keyword already being consumed, so we won't put it back
		return defer_statement(fl);

	//While statement
	} else if(lookahead.tok == WHILE){
		//This rule relies on while already being consumed, so we won't put it back
		return while_statement(fl);

	//An idle(nop) statement
	} else if(lookahead.tok == IDLE){
		//This rule just gives back an idle statement
		return idle_statement(fl);

	//Do while statement
	} else if(lookahead.tok == DO){
		//This rule relies on do already being consumed, so we won't put it back
		return do_while_statement(fl);

	//Switch statement
	} else if(lookahead.tok == SWITCH){
		//This rule relies on switch already being consumed, so we won't put it back
		return switch_statement(fl);

	//If statement
	} else if(lookahead.tok == IF){
		//This rule relies on if already being consumed, so we won't put it back
		return if_statement(fl);

	//Some kind of branch statement
	} else if(lookahead.tok == JUMP || lookahead.tok == BREAK || lookahead.tok == CONTINUE
			|| lookahead.tok == RET){
		//The branch rule needs these, so we'll put them back
		push_back_token(lookahead);
		//return whatever this gives us
		return branch_statement(fl);
	//We have an asm statement here--this really acts like a compiler directive, but we still
	//need to consider it
	} else if(lookahead.tok == ASM){
		return assembly_inline_statement(fl);
	//Common mistake, but this isn't allowed in Ollie
	} else if(lookahead.tok == REPLACE){
		print_parse_message(PARSE_ERROR, "Replace statements have global effects, and therefore must be declared in the global scope", parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	} else {
		//Otherwise, this is some kind of expression statement. We'll put the token back and
		//return that
		push_back_token(lookahead);
		return expression_statement(fl);
	}
}


/**
 * This special modification of the statement rule prohibits us seeing any switch, case or
 * default statements. This statement type is intended to be used inside of swtich statements
 */
static generic_ast_node_t* statement_in_block(FILE* fl){
	//Lookahead token
	Lexer_item lookahead;

	//Let's grab the next item and see what we have here
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we see a label ident, we know we're seeing a labeled statement
	if(lookahead.tok == LABEL_IDENT){
		//This rule relies on these tokens, so we'll push them back
		push_back_token(lookahead);
	
		//Just return whatever the rule gives us
		return labeled_statement(fl);
	
	//If we see an L_CURLY, we are seeing a compound statement
	} else if(lookahead.tok == L_CURLY){
		//The rule relies on it, so put it back
		push_back_token(lookahead);

		//Return whatever the rule gives us
		return compound_statement(fl);
	
	//If we see for, we are seeing a for statement
	} else if(lookahead.tok == FOR){
		//This rule relies on for already being consumed, so we won't put it back
		return for_statement(fl);

	//If we see this, we are seeing a defer statment
	} else if(lookahead.tok == DEFER){
		//This rule relies on the defer keyword already being consumed, so we won't put it back
		return defer_statement(fl);
	
	//If we see this, we have an idle statement
	} else if(lookahead.tok == IDLE){
		return idle_statement(fl);

	//While statement
	} else if(lookahead.tok == WHILE){
		//This rule relies on while already being consumed, so we won't put it back
		return while_statement(fl);

	//Do while statement
	} else if(lookahead.tok == DO){
		//This rule relies on do already being consumed, so we won't put it back
		return do_while_statement(fl);

	//Switch statement -- not allowed here
	} else if(lookahead.tok == SWITCH){
		print_parse_message(PARSE_ERROR, "Ollie language does not allow for nested switch statements", parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);

	//Nested case statements -- should not happen
	} else if(lookahead.tok == CASE || lookahead.tok == DEFAULT){
		print_parse_message(PARSE_ERROR, "Ollie language does not allow for nested case or default statements", parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	
	//If statement
	} else if(lookahead.tok == IF){
		//This rule relies on if already being consumed, so we won't put it back
		return if_statement(fl);

	//Some kind of branch statement
	} else if(lookahead.tok == JUMP || lookahead.tok == BREAK || lookahead.tok == CONTINUE
			|| lookahead.tok == RET){
		//The branch rule needs these, so we'll put them back
		push_back_token(lookahead);
		//return whatever this gives us
		return branch_statement(fl);
	//Let statement
	} else if(lookahead.tok == LET){
		return let_statement(fl);
	} else if(lookahead.tok == DECLARE){
		return declare_statement(fl);
	} else {
		//Otherwise, this is some kind of expression statement. We'll put the token back and
		//return that
		push_back_token(lookahead);
		return expression_statement(fl);
	}
}


/**
 * Handle a default statement. A default statement cannot terminate until we see a "case" or "}"
 *
 * NOTE: We assume that we have already seen and consumed the first case token here
 */
static generic_ast_node_t* default_statement(FILE* fl){
	//Lookaehad token
	Lexer_item lookahead;
	//Freeze the line number
	u_int16_t current_line = parser_line_num;

	//So we've found a switch statement, but have we already found a default clause? Let's see to find out
	if(found_default_clause == 1){
		print_parse_message(PARSE_ERROR, "Duplicate \"default\" clause found in switch statements. Only one default per statement is allowed", parser_line_num);
		num_errors++;
		//Error out
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//If we see default, we can just make the default node
	generic_ast_node_t* default_stmt = ast_node_alloc(AST_NODE_CLASS_DEFAULT_STMT);

	//All that we need to see now is a colon
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we don't see one, we need to scrap it
	if(lookahead.tok != COLON){
		print_parse_message(PARSE_ERROR, "Colon required after default statement", current_line);
		num_errors++;
		//Error node return
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Seed the search
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//Now we need to go through and process the statement
	while(lookahead.tok != CASE && lookahead.tok != R_CURLY){
		//If for some reason it's another default statement, we have a duplicate
		if(lookahead.tok == DEFAULT){
			print_parse_message(PARSE_ERROR, "Duplicate \"default\" clause found in switch statements. Only one default per statement is allowed", parser_line_num);
			num_errors++;
			//Error out
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Push it back
		push_back_token(lookahead);

		//Handle whatever it is that we have in here
		generic_ast_node_t* stmt = statement_in_block(fl);

		//If it's an error send it up
		if(stmt->CLASS == AST_NODE_CLASS_ERR_NODE){
			return stmt;
		}

		//Now that it's good, add this into the statement umbrella
		add_child_node(default_stmt, stmt);

		//Refresh the lookahead
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	}

	//Push it back, we're done here
	push_back_token(lookahead);

	//Mark that we've actually found a default clause
	found_default_clause = 1;

	//Otherwise it all worked, so we'll just return
	return default_stmt;
}


/**
 * Handle a case statement. A case statement does not terminate until we see another or default statement or the closing
 * curly of a switch statement
 *
 * NOTE: We assume that we have already seen and consumed the first case token here
 */
static generic_ast_node_t* case_statement(FILE* fl){
	//For error printing
	char info[ERROR_SIZE];

	//Freeze the current line number
	u_int16_t current_line = parser_line_num;
	//Lookahead token
	Lexer_item lookahead;
	
	//Remember that we've already seen the first "case" keyword here, so now we need
	//to consume whatever comes after it(constant or enum value)
	
	//Create the node
	generic_ast_node_t* case_stmt = ast_node_alloc(AST_NODE_CLASS_CASE_STMT);
	
	//Let's now lookahead and see if we have a valid constant or not
	lookahead = get_next_token(fl, &parser_line_num, SEARCHING_FOR_CONSTANT);

	//If we have some kind of identifier -- it must be an enum member
	if(lookahead.tok == IDENT){
		//Put it back
		push_back_token(lookahead);

		//Let the subrule handle it
		generic_ast_node_t* enum_ident_node = identifier(fl);

		//If it's invalid fail out
		if(enum_ident_node->CLASS == AST_NODE_CLASS_ERR_NODE){
			return enum_ident_node;
		}

		//Extract the name
		char* name = enum_ident_node->identifier;

		//If it's an identifier, then it has to be an enum
		symtab_variable_record_t* enum_record = lookup_variable(variable_symtab, name);

		//If we somehow couldn't find it
		if(enum_record == NULL){
			sprintf(info, "Identifier \"%s\" has never been declared", name);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//If we could find it, but it isn't an enum
		if(enum_record->is_enumeration_member == 0){
			sprintf(info, "Identifier \"%s\" does not belong to an enum, and as such cannot be used in a case statement", name);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);	
		}

		//Otherwise we know that it is good, but is it the right type
		//Are the types here compatible?
		case_stmt->inferred_type = types_compatible(current_switch_statement_type, enum_ident_node->inferred_type);

		//If this fails, they're incompatible
		if(case_stmt->inferred_type == NULL){
			sprintf(info, "Switch statement switches on type \"%s\", but case statement has incompatible type \"%s\"", 
						  current_switch_statement_type->type_name, enum_ident_node->inferred_type->type_name);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Store this for later processing
		enum_ident_node->variable = enum_record;

		//Grab the value of this case statement
		case_stmt->case_statement_value = enum_ident_node->variable->enum_member_value;

		//We already have the value -- so this doesn't need to be a child node

		//Is the lookahead a constant?
	} else if(lookahead.tok == INT_CONST || lookahead.tok == INT_CONST_FORCE_U || lookahead.tok == HEX_CONST
			  || lookahead.tok == CHAR_CONST || lookahead.tok == LONG_CONST || lookahead.tok == LONG_CONST_FORCE_U){
		//Put it back
		push_back_token(lookahead);
	
		//We are now required to see a valid constant
		generic_ast_node_t* const_node = constant(fl, SEARCHING_FOR_CONSTANT);

		//If this fails, the whole thing is over
		if(const_node->CLASS == AST_NODE_CLASS_ERR_NODE){
			print_parse_message(PARSE_ERROR, "Invalid constant found in switch statment", current_line);
			num_errors++;
			//It's already an error, so we'll just give it back
			return const_node;
		}

		//If we have an integer constant here, we need to make sure that it is not negative. Negative values
		//would mess with the jump table logic. Ollie langauge does not support GCC-style "switch-to-if" conversions
		//if the user does this

		//Grab a reference to the constant node
		constant_ast_node_t* const_inner_node = (constant_ast_node_t*)(const_node->node);

		//If it's an int, make sure it isn't negative
		if(const_inner_node->constant_type == INT_CONST || const_inner_node->constant_type == HEX_CONST
		   || const_inner_node->constant_type == INT_CONST_FORCE_U){
			//Fail case here
			if(const_inner_node->int_val < 0){
				print_parse_message(PARSE_ERROR, "Due to ollie mandating the use of a jump table, negative values may not be used in case statements.", current_line);
				num_errors++;
				//It's already an error, so we'll just give it back
				return const_node;
			}
			
			//Assign the value here
			case_stmt->case_statement_value = const_inner_node->int_val;

		//Same thing here as well
		} else if(const_inner_node->constant_type == LONG_CONST || const_inner_node->constant_type == LONG_CONST_FORCE_U){
			//Fail case here
			if(const_inner_node->long_val < 0){
				print_parse_message(PARSE_ERROR, "Due to ollie mandating the use of a jump table, negative values may not be used in case statements.", current_line);
				num_errors++;
				//It's already an error, so we'll just give it back
				return const_node;
			}
		} else if(const_inner_node->constant_type == CHAR_CONST){
			//Just assign the char value here
			case_stmt->case_statement_value = const_inner_node->char_val;
		}

		//Otherwise we know that it is good, but is it the right type
		//Are the types here compatible?
		case_stmt->inferred_type = types_compatible(current_switch_statement_type, const_node->inferred_type);

		//If this fails, they're incompatible
		if(case_stmt->inferred_type == NULL){
			sprintf(info, "Switch statement switches on type \"%s\", but case statement has incompatible type \"%s\"", 
						  current_switch_statement_type->type_name, const_node->inferred_type->type_name);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//We already have the value -- so this doesn't need to be a child node

	} else {
		print_parse_message(PARSE_ERROR, "Enum member or constant required as argument to case statement", current_line);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}


	//One last thing to check -- we need a colon
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we don't see one, we need to scrap it
	if(lookahead.tok != COLON){
		print_parse_message(PARSE_ERROR, "Colon required after case statement", current_line);
		num_errors++;
		//Error node return
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now we get to the main part of our case statement. We can keep seeing extra tokens so long as they are not 
	//"case", "default" or "}"(indicates end of switch statement)
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//We have already seen a break for this case statement
	u_int8_t seen_break = FALSE;

	//So long as we don't have the aforementioned keywords
	while(lookahead.tok != CASE && lookahead.tok != DEFAULT && lookahead.tok != R_CURLY){
		//Put it back
		push_back_token(lookahead);

		//We will use our special statement rule to handle this
		generic_ast_node_t* stmt_node = statement_in_block(fl);

		//If this fails, we error out
		if(stmt_node->CLASS == AST_NODE_CLASS_ERR_NODE){
			return stmt_node;
		}

		//If we have a break statement, we want to make sure that we only have one of these
		if(stmt_node->CLASS == AST_NODE_CLASS_BREAK_STMT){
			//Check here for violations
			if(seen_break == TRUE){
				print_parse_message(PARSE_ERROR, "More than one \"break\" or \"break when\" statements inside of case statement", stmt_node->line_number);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}

			//We've definitely seen one now
			seen_break = TRUE;
		}

		//Otherwise we add this in as a child under the case statement umbrella
		add_child_node(case_stmt, stmt_node);

		//And refresh the lookahead
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	}

	//Push it back, we're done here
	push_back_token(lookahead);

	//Finally give this back
	return case_stmt;
}


/**
 * A declare statement is always the child of an overall declaration statement, so it will
 * be added as the child of the given parent node. A declare statement also performs all
 * needed type/repetition checks. Like all rules, this function returns a reference to the root
 * node that it's created.
 * 
 * NOTE: We have already seen and consume the "declare" keyword by the time that we get here
 *
 * BNF Rule: <declare-statement> ::= declare {register | static}? {mut}? <identifier> : <type-specifier>;
 */
static generic_ast_node_t* declare_statement(FILE* fl){
	//For error printing
	char info[LARGE_ERROR_SIZE];
	//Freeze the current line number
	u_int16_t current_line = parser_line_num;
	//Lookahead token
	Lexer_item lookahead;
	//Is it mutable?
	u_int8_t is_mutable = 0;
	//The storage class, normal by default
	STORAGE_CLASS_T storage_class = STORAGE_CLASS_NORMAL;

	//Let's first declare the root node
	generic_ast_node_t* decl_node = ast_node_alloc(AST_NODE_CLASS_DECL_STMT);

	//Let's see if we have a storage class
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we see a specifier here, we'll record it
	if(lookahead.tok == REGISTER){
		storage_class = STORAGE_CLASS_REGISTER;
		//Refresh token
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	} else if(lookahead.tok == STATIC){
		storage_class = STORAGE_CLASS_STATIC;
		//Refresh token
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	}

	//Let's now check to see if it's mutable or not
	if(lookahead.tok == MUT){
		is_mutable = 1;
	} else {
		//Push the token back
		push_back_token(lookahead);
	}

	//The last thing before we perform checks is for us to see a valid identifier
	generic_ast_node_t* ident_node = identifier(fl);

	if(ident_node->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid identifier given in declaration", parser_line_num);
		num_errors++;
		//It's already an error, just give it back
		return ident_node;
	}

	//Let's get a pointer to the name for convenience
	char* name = ident_node->identifier;

	//Array bounds checking real quick
	if(strlen(name) > MAX_TYPE_NAME_LENGTH){
		sprintf(info, "Variable names may only be at most 200 characters long, was given: %s", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		return FAILURE;
	}

	//Now we will check for duplicates. Duplicate variable names in different scopes are ok, but variables in
	//the same scope may not share names. This is also true regarding functions and types globally
	//Check that it isn't some duplicated function name
	symtab_function_record_t* found_func = lookup_function(function_symtab, name);

	//Fail out here
	if(found_func != NULL){
		sprintf(info, "Attempt to redefine function \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the function declaration
		print_function_name(found_func);
		num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Finally check that it isn't a duplicated type name
	symtab_type_record_t* found_type = lookup_type(type_symtab, name);

	//Fail out here
	if(found_type!= NULL){
		sprintf(info, "Attempt to redefine type \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_type_name(found_type);
		num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Check that it isn't some duplicated variable name. We will only check in the
	//local scope for this one
	symtab_variable_record_t* found_var = lookup_variable_local_scope(variable_symtab, name);

	//Fail out here
	if(found_var != NULL){
		sprintf(info, "Attempt to redefine variable \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_variable_name(found_var);
		num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Let's see if we've already named a constant this
	symtab_constant_record_t* found_const = lookup_constant(constant_symtab, name);

	//Fail out if this isn't null
	if(found_const != NULL){
		sprintf(info, "Attempt to redefine constant \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_constant_name(found_const);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	
	//Now we need to see a colon
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	if(lookahead.tok != COLON){
		print_parse_message(PARSE_ERROR, "Colon required between identifier and type specifier in declare statement", parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}
	
	//Now we are required to see a valid type specifier
	generic_ast_node_t* type_spec_node = type_specifier(fl);

	//If this fails, the whole thing is bunk
	if(type_spec_node->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid type specifier given in declaration", parser_line_num);
		//It's already an error, so we'll just send it back up
		return type_spec_node;
	}

	//One thing here, we aren't allowed to see void
	if(strcmp(type_spec_node->inferred_type->type_name, "void") == 0){
		print_parse_message(PARSE_ERROR, "\"void\" type is only valid for function returns, not variable declarations", parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//The last thing that we are required to see before final assembly is a semicolon
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	if(lookahead.tok != SEMICOLON){
		print_parse_message(PARSE_ERROR, "Semicolon required at the end of declaration statement", parser_line_num);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now that we've made it down here, we know that we have valid syntax and no duplicates. We can
	//now create the variable record for this function
	//Initialize the record
	symtab_variable_record_t* declared_var = create_variable_record(name, storage_class);
	//Store its constant status
	declared_var->is_mutable = is_mutable;
	//Store the type--make sure that we strip any aliasing off of it first
	declared_var->type = dealias_type(type_spec_node->inferred_type);
	//It was not initialized
	declared_var->initialized = 0;
	//It was declared
	declared_var->declare_or_let = 0;
	//The line_number
	declared_var->line_number = current_line;
	//Store what module this was defined in
	strncpy(declared_var->module_defined_in, current_file_token, MAX_IDENT_LENGTH);
	//Now that we're all good, we can add it into the symbol table
	insert_variable(variable_symtab, declared_var);

	//Also store this record with the root node
	decl_node->variable = declared_var;
	//Store the line number
	decl_node->line_number = current_line;

	//We no longer need the ident or type specifier nodes

	//All went well so we can return this
	return decl_node;
}


/**
 * A let statement is always the child of an overall declaration statement. Like a declare statement, it also
 * performs type checking and inference and all needed symbol table manipulation
 *
 * NOTE: By the time we get here, we've already consumed the let keyword
 *
 * BNF Rule: <let-statement> ::= let {register | static}? {mut}? <identifier> : <type-specifier> := <conditional-expression>;
 */
static generic_ast_node_t* let_statement(FILE* fl){
	//For error printing
	char info[LARGE_ERROR_SIZE];
	//The line number
	u_int16_t current_line = parser_line_num;
	//Lookahead token
	Lexer_item lookahead;
	//Is it mutable?
	u_int8_t is_mutable = 0;
	//The storage class, normal by default
	STORAGE_CLASS_T storage_class = STORAGE_CLASS_NORMAL;

	//Let's first declare the root node
	generic_ast_node_t* let_stmt_node = ast_node_alloc(AST_NODE_CLASS_LET_STMT);

	//Grab the next token -- we could potentially see a storage class specifier
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we see a specifier here, we'll record it
	if(lookahead.tok == REGISTER){
		storage_class = STORAGE_CLASS_REGISTER;
		//Refresh token
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	} else if(lookahead.tok == STATIC){
		storage_class = STORAGE_CLASS_STATIC;
		//Refresh token
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	}

	//Let's now check and see if this is mutable
	if(lookahead.tok == MUT){
		is_mutable = 1;
	} else {
		//Otherwise push this back
		push_back_token(lookahead);
	}

	//The last thing before we perform checks is for us to see a valid identifier
	generic_ast_node_t* ident_node = identifier(fl);

	if(ident_node->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid identifier given in let statement", parser_line_num);
		num_errors++;
		//It's already an error, just give it back
		return ident_node;
	}

	//Let's get a pointer to the name for convenience
	char* name = ident_node->identifier;

	//Array bounds checking real quick
	if(strlen(name) > MAX_TYPE_NAME_LENGTH){
		sprintf(info, "Variable names may only be at most 200 characters long, was given: %s", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		return FAILURE;
	}

	//Now we will check for duplicates. Duplicate variable names in different scopes are ok, but variables in
	//the same scope may not share names. This is also true regarding functions and types globally
	//Check that it isn't some duplicated function name
	symtab_function_record_t* found_func = lookup_function(function_symtab, name);

	//Fail out here
	if(found_func != NULL){
		sprintf(info, "Attempt to redefine function \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the function declaration
		print_function_name(found_func);
		num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Finally check that it isn't a duplicated type name
	symtab_type_record_t* found_type = lookup_type(type_symtab, name);

	//Fail out here
	if(found_type!= NULL){
		sprintf(info, "Attempt to redefine type \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_type_name(found_type);
		num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Check that it isn't some duplicated variable name. We will only check in the
	//local scope for this one
	symtab_variable_record_t* found_var = lookup_variable_local_scope(variable_symtab, name);

	//Fail out here
	if(found_var != NULL){
		sprintf(info, "Attempt to redefine variable \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_variable_name(found_var); num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Let's see if we've already named a constant this
	symtab_constant_record_t* found_const = lookup_constant(constant_symtab, name);

	//Fail out if this isn't null
	if(found_const != NULL){
		sprintf(info, "Attempt to redefine constant \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_constant_name(found_const);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now we need to see a colon
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	if(lookahead.tok != COLON){
		print_parse_message(PARSE_ERROR, "Expected colon between identifier and type specifier in let statement", parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now we are required to see a valid type specifier
	generic_ast_node_t* type_spec_node = type_specifier(fl);

	//If this fails, the whole thing is bunk
	if(type_spec_node->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid type specifier given in let statement", parser_line_num);
		//It's already an error, so we'll just send it back up
		return type_spec_node;
	}
	
	//One thing here, we aren't allowed to see void
	if(strcmp(type_spec_node->inferred_type->type_name, "void") == 0){
		print_parse_message(PARSE_ERROR, "\"void\" type is only valid for function returns, not variable declarations", parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}


	//Now we know that it wasn't a duplicate, so we must see a valid assignment operator
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	if(lookahead.tok != COLONEQ){
		print_parse_message(PARSE_ERROR, "Assignment operator(:=) required after identifier in let statement", parser_line_num);
		num_errors++;
		//Create and return an error
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Otherwise we saw it, so now we need to see a valid conditional expression
	generic_ast_node_t* expr_node = logical_or_expression(fl);

	//We now need to complete type checking. Is what we're assigning to the new variable
	//compatible with what we're given by the logical or expression here?

	//If it fails, we fail out
	if(expr_node->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid conditional expression given as intializer", parser_line_num);
		num_errors++;
		//It's already an error so just give it back
		return expr_node;
	}

	//Otherwise it worked, so we'll add it in as a child
	add_child_node(let_stmt_node, expr_node);

	//The last thing that we are required to see before final assembly is a semicolon
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//Last possible tripping point
	if(lookahead.tok != SEMICOLON){
		print_parse_message(PARSE_ERROR, "Semicolon required at the end of let statement", parser_line_num);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Extract the two types here
	generic_type_t* left_hand_type = type_spec_node->inferred_type;
	generic_type_t* right_hand_type = expr_node->inferred_type;

	generic_type_t* return_type = types_compatible(left_hand_type, right_hand_type);

	//If the return type of the logical or expression is an address, is it an address of a mutable variable?
	if(expr_node->inferred_type->type_class == TYPE_CLASS_POINTER){
		if(expr_node->variable->is_mutable == 0 && is_mutable == 1){
			print_parse_message(PARSE_ERROR, "Mutable references to immutable variables are forbidden", parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}
	}

	//Will be null if we have a failure
	if(return_type == NULL){
		sprintf(info, "Attempt to assign expression of type %s to variable of type %s", right_hand_type->type_name, left_hand_type->type_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Store this just in case--most likely won't use
	let_stmt_node->inferred_type = return_type;

	//Now that we've made it down here, we know that we have valid syntax and no duplicates. We can
	//now create the variable record for this function
	//Initialize the record
	symtab_variable_record_t* declared_var = create_variable_record(name, storage_class);
	//Store it's mutability status
	declared_var->is_mutable = is_mutable;
	//Store the type
	declared_var->type = type_spec_node->inferred_type;
	//Is it mutable
	declared_var->is_mutable = is_mutable;
	//It was initialized
	declared_var->initialized = 1;
	//It was "letted" 
	declared_var->declare_or_let = 1;
	//Save the line num
	declared_var->line_number = current_line;
	//Insert what module this came from
	strncpy(declared_var->module_defined_in, current_file_token, MAX_IDENT_LENGTH);

	//Now that we're all good, we can add it into the symbol table
	insert_variable(variable_symtab, declared_var);
	
	//Add the reference into the root node
	let_stmt_node->variable = declared_var;
	//Store the line number
	let_stmt_node->line_number = current_line;

	//Once we get here, the ident nodes and type specifiers are useless

	//Give back the let statement node here
	return let_stmt_node;
}


/**
 * An alias statement allows us to redefine any currently defined type as some other type. It is probably the
 * simplest of any of these rules, but it still performs all type checking and symbol table manipulation. This is a compiler
 * only directive, so no node is return. Simply success or failure is given back
 *
 * NOTE: By the time we make it here, we have already seeen the alias keyword
 *
 * BNF Rule: <alias-statement> ::= alias <type-specifier> as <identifier>;
 */
static u_int8_t alias_statement(FILE* fl){
	//For error printing
	char info[LARGE_ERROR_SIZE];
	//Store the ident name locally
	char ident_name[MAX_TYPE_NAME_LENGTH];
	//Our lookahead token
	Lexer_item lookahead;

	//We need to first see a valid type specifier
	generic_ast_node_t* type_spec_node = type_specifier(fl);

	//If it is bad, we'll bail out
	if(type_spec_node->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid type specifier given to alias statement", parser_line_num);
		num_errors++;
		//Fail out
		return FAILURE;
	}

	//Let's grab this now
	generic_type_t* type = type_spec_node->inferred_type;

	//Once we have the reference, the actual node is useless so we'll free it

	//We now need to see the as keyword
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we don't see it we're out
	if(lookahead.tok != AS){
		print_parse_message(PARSE_ERROR, "As keyword expected in alias statement", parser_line_num);
		num_errors++;
		//Fail out
		return FAILURE;
	}

	//Otherwise we've made it, so now we need to see a valid identifier
	generic_ast_node_t* ident_node = identifier(fl);

	//If it's bad, we're also done here
	if(ident_node->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid identifier given to alias statement", parser_line_num);
		num_errors++;
		return FAILURE;
	}

	//Array bounds checking real quick
	if(strlen(ident_node->identifier) > MAX_TYPE_NAME_LENGTH){
		sprintf(info, "Type names may only be at most 200 characters long, was given: %s", (ident_node->identifier));
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		return FAILURE;
	}

	//Let's extract the name
	strcpy(ident_name, ident_node->identifier);

	//Once we have the ident name, we no longer need the ident node

	//Let's do our last syntax check--the semicolon
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we don't see a semicolon we're out
	if(lookahead.tok != SEMICOLON){
		print_parse_message(PARSE_ERROR, "Semicolon expected at the end of alias statement",  parser_line_num);
		num_errors++;
		//Fail out here
		return FAILURE;
	}

	//Check that it isn't some duplicated function name
	symtab_function_record_t* found_func = lookup_function(function_symtab, ident_name);

	//Fail out here
	if(found_func != NULL){
		sprintf(info, "Attempt to redefine function \"%s\". First defined here:", ident_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the function declaration
		print_function_name(found_func);
		num_errors++;
		//Fail out
		return FAILURE;
	}

	//Check that it isn't some duplicated variable name
	symtab_variable_record_t* found_var = lookup_variable(variable_symtab, ident_name);

	//Fail out here
	if(found_var != NULL){
		sprintf(info, "Attempt to redefine variable \"%s\". First defined here:", ident_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_variable_name(found_var);
		num_errors++;
		//Fail out
		return FAILURE;
	}

	//Finally check that it isn't a duplicated type name
	symtab_type_record_t* found_type = lookup_type(type_symtab, ident_name);

	//Fail out here
	if(found_type!= NULL){
		sprintf(info, "Attempt to redefine type \"%s\". First defined here:", ident_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_type_name(found_type);
		num_errors++;
		//Fail out
		return FAILURE;
	}

	//If we get here, we know that it actually worked, so we can create the alias
	generic_type_t* aliased_type = create_aliased_type(ident_name, type, parser_line_num);

	//Let's now create the aliased record
	symtab_type_record_t* aliased_record = create_type_record(aliased_type);

	//We'll store it in the symbol table
	insert_type(type_symtab, aliased_record);

	//We succeeded so
	return SUCCESS;
}


/**
 * A definition is an alias or define statement. Since these statements are compiler directives
 * in and of themselves, we will not return any nodes. Instead, we return 0 or 1 based on success 
 * or failure
 *
 * NOTE: We assume that there is a define or alias token for us to use to switch based on
 */
static u_int8_t definition(FILE* fl){
	//Lookahead token
	Lexer_item lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//Switch based on what we have
	if(lookahead.tok == DEFINE){
		//We can now see construct or enum
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

		//Switch based on what we have here
		if(lookahead.tok == CONSTRUCT){
			return construct_definer(fl);
		} else if(lookahead.tok == ENUM){
			return enum_definer(fl);
		//Some weird error here
		} else {
			print_parse_message(PARSE_ERROR, "Expected construct or enum keywords after define statement, saw neither", parser_line_num);
			num_errors++;
			return FAILURE;
		}

	} else if(lookahead.tok == ALIAS){
		return alias_statement(fl);
	//Some weird error here
	} else {
		print_parse_message(PARSE_ERROR, "Definition expected define or alias keywords, found neither", parser_line_num);
		num_errors++;
		return FAILURE;
	}
}


/**
 * A declaration is a pass through rule that does not itself initialize a node. Instead, it will pass down to
 * the appropriate rule here and let them initialize the rule. Like all rules in a system, the declaration returns
 * a reference to the root node that it created
 *
 * <declaration> ::= <declare-statement> 
 * 				   | <let-statement> 
 */
static generic_ast_node_t* declaration(FILE* fl){
	//For error printing
	char info[LARGE_ERROR_SIZE];
	//Lookahead token
	Lexer_item lookahead;

	//We will multiplex based on what we see with the lookahead
	//This rule also consumes the first token that it sees, so all downstream
	//rules must account for that
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//We have a declare statement
	if(lookahead.tok == DECLARE){
		return declare_statement(fl);
	//We have a let statement
	} else if(lookahead.tok == LET){
		return let_statement(fl);
	//Otherwise we have some weird error here
	} else {
		sprintf(info, "Saw \"%s\" when let or declare was expected", lookahead.lexeme);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		//Return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}
}


/**
 * We will completely duplicate a deferred statement here. Since all deferred statements
 * are logical expressions, we will perform a deep copy to create an entirely new
 * chain of deferred statements
 */
static generic_ast_node_t* duplicate_subtree(const generic_ast_node_t* duplicatee){
	//Base case here -- although in theory we shouldn't make it here
	if(duplicatee == NULL){
		return NULL;
	}

	//Duplicate the node here
	generic_ast_node_t* duplicated_root = duplicate_node(duplicatee);

	//Now for each child in the node, we duplicate it and add it in as a child
	generic_ast_node_t* child_cursor = duplicatee->first_child;

	//The duplicated child
	generic_ast_node_t* duplicated_child = NULL;

	//So long as we aren't null
	while(child_cursor != NULL){
		//Recursive call
		duplicated_child = duplicate_subtree(child_cursor);

		//Add the duplicate child into the node
		add_child_node(duplicated_root, duplicated_child);

		//Advance the cursor
		child_cursor = child_cursor->next_sibling;
	}

	//Return the duplicate root
	return duplicated_root;
}


/**
 * We need to go through and check all of the jump statements that we have in the function. If any
 * one of these jump statements is trying to jump to a label that does not exist, then we need to fail out
 */
static int8_t check_jump_labels(symtab_function_record_t* func_record){
	//For error printing
	char info[ERROR_SIZE];
	//Grab a reference to our current jump statement
	generic_ast_node_t* current_jump_statement;

	//So long as there are jump statements in the queue
	while(queue_is_empty(current_function_jump_statements) == HEAP_QUEUE_NOT_EMPTY){
		//Grab the jump statement
		current_jump_statement = dequeue(current_function_jump_statements);

		//Grab the label ident node
		generic_ast_node_t* label_ident_node = current_jump_statement->first_child;

		//Let's grab out the name for convenience
		char* name = label_ident_node->identifier;

		//We now need to lookup the name in here. We use a special function that allows
		//us to look deeper into the scopes 
		symtab_variable_record_t* label = lookup_variable_lower_scope(variable_symtab, name);

		//If we didn't find it, we fail out
		if(label == NULL){
			sprintf(info, "Attempt to jump to nonexistent label \"%s\".", name);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			//Fail out
			return FAILURE;
		}

		//We can also have a case where this is not null, but it isn't in the correct function scope(also bad)
		if(strcmp(current_function->func_name, label->function_declared_in->func_name) != 0){
			sprintf(info, "Label \"%s\" was declared in function \"%s\". You cannot jump outside of a function" , name, label->function_declared_in->func_name);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			return FAILURE;
		}

		//Otherwise it worked, so we'll add this as the function's jumping to label
		current_jump_statement->variable = label;
	}

	//If we get here then they all worked
	return SUCCESS;
}


/**
 * Handle the case where we declare a function. A function will always be one of the children of a declaration
 * partition
 *
 * NOTE: We have already consumed the FUNC keyword by the time we arrive here, so we will not look for it in this function
 *
 * BNF Rule: <function-definition> ::= func {:static}? <identifer> ({<parameter-list>}?) -> <type-specifier>{; | <compound-statement>}
 *
 * REMEMBER: By the time we get here, we've already seen the func keyword
 */
static generic_ast_node_t* function_definition(FILE* fl){
	//This will be used for error printing
	char info[LARGE_ERROR_SIZE];
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	//Lookahead token
	Lexer_item lookahead;
	//Are we defining something that's already been defined implicitly?
	u_int8_t defining_prev_implicit = 0;
	//Is it the main function?
	u_int8_t is_main_function = 0;

	//What is the function's storage class? Normal by default
	STORAGE_CLASS_T storage_class = STORAGE_CLASS_REGISTER;

	//We need a stack for storing jump statements. We need to check these later because if
	//we check them as we go, we don't get full jump functionality
	current_function_jump_statements = heap_queue_alloc();

	//We also have the AST function node, this will be intialized immediately
	//It also requires a symtab record of the function, but this will be assigned
	//later once we have it
	generic_ast_node_t* function_node = ast_node_alloc(AST_NODE_CLASS_FUNC_DEF);

	//REMEMBER: by the time we get here, we've already seen and consumed "FUNC"
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	
	//We've seen the option for a function specifier. 
	if(lookahead.tok == COLON){
		//We need to see the optional static keyword here
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

		//This is our fail case here
		if(lookahead.tok != STATIC){
			print_parse_message(PARSE_ERROR, "Static keyword expected after colon in function definition", parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Otherwise, we know that we have a static storage class
		storage_class = STORAGE_CLASS_STATIC;
	//Otherwise it's just the normal storage class
	} else {
		//Otherwise put the token back in the stream
		push_back_token(lookahead);
		//Normal storage class
		storage_class = STORAGE_CLASS_NORMAL;
	}

	//Now we must see a valid identifier as the name
	generic_ast_node_t* ident_node = identifier(fl);

	//If we have a failure here, we're done for
	if(ident_node->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid name given as function name", current_line);
		num_errors++;
		//It's already an error, so just give it back
		return ident_node;
	}

	//Otherwise, we could still have a failure here if this is any kind of duplicate
	//Grab a reference for convenience
	char* function_name = ident_node->identifier;

	//Array bounds checking real quick
	if(strlen(function_name) > MAX_TYPE_NAME_LENGTH){
		sprintf(info, "Function names may only be at most 200 characters long, was given: %s", function_name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Let's now do all of our checks for duplication before we go any further. This can
	//save us time if it ends up being bad
	
	//Now we must perform all of our symtable checks. Parameters may not share names with types, functions or variables
	symtab_function_record_t* function_record = lookup_function(function_symtab, function_name); 

	//Fail out if found and it's already been defined
	if(function_record != NULL && function_record->defined == 1){
		sprintf(info, "A function with name \"%s\" has already been defined. First defined here:", function_record->func_name);
		print_parse_message(PARSE_ERROR, info, current_line);
		print_function_name(function_record);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);

	//This is our interesting case. The function has been defined implicitly, and now we're trying to define it
	//explicitly. We don't need to do any other checks if this is the case
	} else if(function_record != NULL && function_record->defined == 0){
		//Flag this
		defining_prev_implicit = 1;
		//Set this as well
		current_function = function_record;

	//Otherwise we're defining fresh, so all of these checks need to happen
	} else {
		//Check for duplicated variables
		symtab_variable_record_t* found_variable = lookup_variable(variable_symtab, function_name); 

		//Fail out if duplicate is found
		if(found_variable != NULL){
			sprintf(info, "A variable with name \"%s\" has already been defined. First defined here:", found_variable->var_name);
			print_parse_message(PARSE_ERROR, info, current_line);
			print_variable_name(found_variable);
			num_errors++;
			//Create and return an error node
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Check for duplicated type names
		symtab_type_record_t* found_type = lookup_type(type_symtab, function_name); 

		//Fail out if duplicate has been found
		if(found_type != NULL){
			sprintf(info, "A type with name \"%s\" has already been defined. First defined here:", found_type->type->type_name);
			print_parse_message(PARSE_ERROR, info, current_line);
			print_type_name(found_type);
			num_errors++;
			//Create and return an error node
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Let's see if we've already named a constant this
		symtab_constant_record_t* found_const = lookup_constant(constant_symtab, function_name);

		//Fail out if this isn't null
		if(found_const != NULL){
			sprintf(info, "Attempt to redefine constant \"%s\". First defined here:", function_name);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			//Also print out the original declaration
			print_constant_name(found_const);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Now that we know it's fine, we can first create the record. There is still more to add in here, but we can at least start it
		function_record = create_function_record(function_name, storage_class);
		//Associate this with the function node
		function_node->func_record = function_record;
		//Set first thing
		function_record->number_of_params = 0;
		function_record->line_number = current_line;
		//Create the call graph node
		function_record->call_graph_node = create_call_graph_node(function_record);
		//By default, this function has never been called
		function_record->called = 0;
		//Copy the module it was defined in
		strncpy(function_record->module_defined_in, current_file_token, MAX_IDENT_LENGTH);

		//We'll put the function into the symbol table
		//since we now know that everything worked
		insert_function(function_symtab, function_record);

		//We'll also flag that this is the current function
		current_function = function_record;

		/**
		 * If this is the main function, we will record it as having been called by the operating 
		 * system
		 */
		if(strcmp("main", function_name) == 0){
			//By default, this function has been called
			function_record->called = 1;
			//It is the main function
			is_main_function = 1;
			//And furthermore, it was called by the os
			call_function(os, function_record->call_graph_node);
		}
	}

	//Now we need to see a valid parentheis
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we didn't find it, no point in going further
	if(lookahead.tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Left parenthesis expected before parameter list", current_line);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Otherwise, we'll push this onto the list to check for later
	push_token(grouping_stack, lookahead);

	//We initialize this scope automatically, even if there is no param list.
	//It will just be empty if this is the case, no big issue
	initialize_variable_scope(variable_symtab);

	//Now we must ensure that we see a valid parameter list. It is important to note that
	//parameter lists can be empty, but whatever we have here we'll have to add in
	//Parameter list parent is the function node
	generic_ast_node_t* param_list_node = parameter_list(fl);

	//We have a bad parameter list
	if(param_list_node->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid parameter list given in function declaration", current_line);
		num_errors++;
		//It's already an error, so just send it back up
		return param_list_node;
	}

	//We'll hold off on addind it as a child until we see valid closing parenthesis
	
	//Now we need to see a valid closing parenthesis
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we don't have an R_Paren that's an issue
	if(lookahead.tok != R_PAREN){
		print_parse_message(PARSE_ERROR, "Right parenthesis expected after parameter list", current_line);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}
	
	//If this happens, then we have some unmatched parenthesis
	if(pop_token(grouping_stack).tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Unmatched parenthesis found", current_line);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Once we make it here, we know that we have a valid param list and valid parenthesis. We can
	//now parse the param_list and store records to it	
	//Let's first add the param list in as a child
	
	//Let's now iterate over the parameter list and add the parameter records into the function 
	//record for ease of access later
	generic_ast_node_t* param_list_cursor = param_list_node->first_child;

	//If we are defining a previously implicit function, we'll need to check the types & order
	if(defining_prev_implicit == 1){
		//How many params do we have
		u_int8_t param_count = 0;
		//The internal function record param
		symtab_variable_record_t* func_param;

		//So long as this isn't null
		while(param_list_cursor != NULL){
			//If at any point this is more than the number of parameters this function is meant to have,
			//we bail
			if(param_count > function_record->number_of_params){
				sprintf(info, "Function \"%s\" was defined implicitly to only have %d parameters. First defined here:", function_record->func_name, function_record->number_of_params);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				//Print the function out too
				print_function_name(function_record);
				num_errors++;
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}

			//Grab this out for reference
			func_param = function_record->func_params[param_count].associate_var;
			//The variable record for this param node
			symtab_variable_record_t* param_rec = param_list_cursor->variable;

			//Let's now compare the types here
			if(strcmp(param_rec->type->type_name, func_param->type->type_name) != 0){
				sprintf(info, "Function \"%s\" was defined with parameter %d of type \"%s\", this may not be changed.", function_name, param_count, func_param->type->type_name);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				print_function_name(function_record);
				num_errors++;
			}

			//Otherwise it's fine, so we'll overwrite the entire thing in the record
			function_record->func_params[param_count].associate_var = param_rec;

			//Advance this
			param_list_cursor = param_list_cursor->next_sibling;
			//One more param
			param_count++;
		}

	//Otherwise we are defining from scratch here
	} else {
		//So long as this is not null
		while(param_list_cursor != NULL){
			//For dev use--sanity check
			if(param_list_cursor->CLASS != AST_NODE_CLASS_PARAM_DECL){
				print_parse_message(PARSE_ERROR, "Fatal internal compiler error. Expected declaration node in parameter list", parser_line_num);
				num_errors++;
				//Return an error node
				return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
			}

			//The variable record for this param node
			symtab_variable_record_t* param_rec = param_list_cursor->variable;

			//We'll add it in as a reference to the function
			function_record->func_params[function_record->number_of_params].associate_var = param_rec;
			//Increment the parameter count
			(function_record->number_of_params)++;

			//Set the associated function record
			param_rec->parent_function = function_record;

			//Push the cursor up by 1
			param_list_cursor = param_list_cursor->next_sibling;
		}
	}

	//Once we get down here, the entire parameter list has been stored properly

	//Semantics here, we now must see a valid arrow symbol
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If it isn't an arrow, we're out of here
	if(lookahead.tok != ARROW){
		print_parse_message(PARSE_ERROR, "Arrow(->) required after parameter-list in function", parser_line_num);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now if we get here, we must see a valid type specifier
	//The type specifier rule already does existence checking for us
	generic_ast_node_t* return_type_node = type_specifier(fl);

	//If we failed, bail out
	if(return_type_node->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid return type given to function. All functions, even void ones, must have an explicit return type", parser_line_num);
		num_errors++;
		//It's already an error, just send it back
		return return_type_node;
	}

	//Grab the type record. A reference to this will be stored in the function symbol table. Make sure
	//that we first dealias it
	generic_type_t* type = dealias_type(return_type_node->inferred_type);

	//SPECIAL CASE : The main function must return a type of s_int32
	if(is_main_function == 1){
		//If it's not a basic type we fail
		if(type->type_class != TYPE_CLASS_BASIC){
			print_parse_message(PARSE_ERROR, "The main function must return a type of i32.", parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Now we know that it is a basic type, but is it an s_int32?
		if(type->basic_type->basic_type != S_INT32){
			print_parse_message(PARSE_ERROR, "The main function must return a type of i32.", parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}
		//Otherwise it's fine
	}

	//If we're defining a function that was previously implicit, the types have to match exactly
	if(defining_prev_implicit == 1){
		if(strcmp(type->type_name, function_record->return_type->type_name) != 0){
			sprintf(info, "Function \"%s\" was defined implicitly with a return type of \"%s\", this may not be altered. First defined here:", function_name, function_record->return_type->type_name);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			print_function_name(function_record);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}
	}

	//Store the return type
	function_record->return_type = type;

	//Once we have the type, most nodes that we have here are useless

	//Now we have a fork in the road here. We can either define the function implicitly here
	//or we can do a full definition
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If it's a semicolon, we're done
	if(lookahead.tok == SEMICOLON){
		//The main function may not be defined implicitly
		if(is_main_function == 1){
			print_parse_message(PARSE_ERROR, "The main function may not be defined implicitly. Implicit definition here:", parser_line_num);
			print_function_name(function_record);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//If we're for some reason defining a previous implicit function
		if(defining_prev_implicit == 1){
			sprintf(info, "Function \"%s\" was already defined implicitly here:", function_name);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			print_function_name(function_record);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Otherwise it should be ok

		//If this is the case, then we essentially have a compiler directive here. We'll return NULL

		//Finalize the variable scope
		finalize_variable_scope(variable_symtab);
		
		//This function was not defined
		function_record->defined = 0;
		
		//Return NULL here
		return NULL;

	} else {
		//Put it back
		push_back_token(lookahead);

		//Some housekeeping, if there were previously deferred statements, we want them out
		deferred_stmts_node = NULL;

		//We are finally required to see a valid compound statement
		generic_ast_node_t* compound_stmt_node = compound_statement(fl);

		//If this fails we'll just pass it through
		if(compound_stmt_node->CLASS == AST_NODE_CLASS_ERR_NODE){
			return compound_stmt_node;
		}
	
		//This function was defined
		function_record->defined = 1;

		//Where was this function defined
		function_record->line_number = current_line;

		//If this function is a void return type, we need to manually insert
		//a ret statement at the very end, if there isn't one already
		if(strcmp(type->type_name, "void") == 0){
			//Let's drill down to the very end
			generic_ast_node_t* cursor = compound_stmt_node->first_child;

			//So long as we don't see ret statements here, we keep going
			while(cursor->next_sibling != NULL && cursor->CLASS != AST_NODE_CLASS_RET_STMT){
				//Advance
				cursor = cursor->next_sibling;
			}

			//Once we get here, we need to check and see if we have a ret stmt
			if(cursor->CLASS != AST_NODE_CLASS_RET_STMT){
				//Allocate a ret node - leave it blank
				generic_ast_node_t* ret_node = ast_node_alloc(AST_NODE_CLASS_RET_STMT);

				//We'll add in all of our deferred statements here too, if we have them
				if(deferred_stmts_node != NULL){
					//Make a clone of it
					generic_ast_node_t* deferred_stmts = duplicate_subtree(deferred_stmts_node);

					//Link the ret node in
					deferred_stmts->next_sibling = ret_node;

					//Add this as the child to the compound stmt node
					add_child_node(compound_stmt_node, deferred_stmts);
				} else {
					//No deferred statements here
					add_child_node(compound_stmt_node, ast_node_alloc(AST_NODE_CLASS_RET_STMT));
				}
			}
		}

		//If we get here we know that it worked, so we'll add it in as a child
		add_child_node(function_node, compound_stmt_node);
		
		//We now need to check and see if our jump statements are actually valid
		if(check_jump_labels(function_record) == FAILURE){
			//If this fails, we fail out here too
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Finalize the variable scope for the parameter list
		finalize_variable_scope(variable_symtab);

		//We're done with this, so destroy it
		heap_queue_dealloc(current_function_jump_statements);

		//Store the line number
		function_node->line_number = current_line;

		//All good so we can get out
		return function_node;
	}
}


/**
 * Handle a replace statement. A replace statement allows the programmer to eliminate any/all
 * magic numbers in the program. A replace statement is the only kind of statement that 
 *
 * Example:
 * #replace MY_INT with 2;
 */
static u_int8_t replace_statement(FILE* fl){
	//For error printing
	char info[1500];
	//Lookahead token
	Lexer_item lookahead;

	//We've already seen the with statement, now we need to see an
	//identifier
	generic_ast_node_t* ident_node = identifier(fl);

	//If we failed, we're done here
	if(ident_node->CLASS == AST_NODE_CLASS_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Invalid identifier given to replace statement", parser_line_num);
		num_errors++;
		return FAILURE;
	}
	
	//Now that we have the ident, we need to make sure that it's not a duplicate
	//Let's get a pointer to the name for convenience
	char* name = ident_node->identifier;

	//Array bounds checking real quick
	if(strlen(name) > MAX_TYPE_NAME_LENGTH){
		sprintf(info, "Variable names may only be at most 200 characters long, was given: %s", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		return FAILURE;
	}

	//Now we will check for duplicates. Duplicate variable names in different scopes are ok, but variables in
	//the same scope may not share names. This is also true regarding functions and types globally
	//Check that it isn't some duplicated function name
	symtab_function_record_t* found_func = lookup_function(function_symtab, name);

	//Fail out here
	if(found_func != NULL){
		sprintf(info, "Attempt to redefine function \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the function declaration
		print_function_name(found_func);
		num_errors++;
		return FAILURE;
	}

	//Finally check that it isn't a duplicated type name
	symtab_type_record_t* found_type = lookup_type(type_symtab, name);

	//Fail out here
	if(found_type!= NULL){
		sprintf(info, "Attempt to redefine type \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_type_name(found_type);
		num_errors++;
		return FAILURE;
	}

	//Check that it isn't some duplicated variable name. We will only check in the
	//local scope for this one
	symtab_variable_record_t* found_var = lookup_variable_local_scope(variable_symtab, name);

	//Fail out here
	if(found_var != NULL){
		sprintf(info, "Attempt to redefine variable \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_variable_name(found_var); num_errors++;
		return FAILURE;
	}

	//Let's see if we've already named a constant this
	symtab_constant_record_t* found_const = lookup_constant(constant_symtab, name);

	//Fail out if this isn't null
	if(found_const != NULL){
		sprintf(info, "Attempt to redefine constant \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_constant_name(found_const);
		num_errors++;
		return FAILURE;
	}

	//We now need to see the with keyword
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we don't see it, then we're done here
	if(lookahead.tok != WITH){
		print_parse_message(PARSE_ERROR, "With keyword required in replace statement", parser_line_num);
		num_errors++;
		return FAILURE;
	}

	//Otherwise it worked, so now we need to see a constant of some kind
	generic_ast_node_t* constant_node = constant(fl, SEARCHING_FOR_CONSTANT);

	//If this fails, then we are done
	if(constant_node->CLASS == AST_NODE_CLASS_ERR_NODE){
		//Just return 0, printing already happened
		return FAILURE;
	}

	//One last thing, we need to see a semicolon
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we don't see this, we're done
	if(lookahead.tok != SEMICOLON){
		print_parse_message(PARSE_ERROR, "Semicolon required after replace statement", parser_line_num);
		num_errors++;
		return FAILURE;
	}

	//Now we're ready for assembly and insertion
	symtab_constant_record_t* created_const = create_constant_record(name);

	//Once we've created it, we'll pack it with values
	created_const->constant_node = constant_node;
	created_const->line_number = parser_line_num;
	//Store what module this was defined in
	strncpy(created_const->module_defined_in, current_file_token, MAX_IDENT_LENGTH);

	//Insert the record into the symtab
	insert_constant(constant_symtab, created_const);

	//And we're all set, return success(1)
	return SUCCESS;
}


/**
 * Here we can either have a function definition or a declaration
 *
 * Like all other functions, this function returns a pointer to the 
 * root of the subtree it creates. Since there is no concrete node here,
 * this function is really just a multiplexing rule
 *
 * <declaration-partition>::= <function-definition>
 *                        	| <declaration>
 *                        	| <definition>
 *                        	| <replace-statement>
 */
static generic_ast_node_t* declaration_partition(FILE* fl){
	//Lookahead token
	Lexer_item lookahead;

	//Grab the next token
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	if(lookahead.tok == ERROR){
		print_parse_message(PARSE_ERROR, "Fatal error. Found error token\n", lookahead.line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}


	//We know that we have a function here
	//We consume the function token here, NOT in the function rule
	if(lookahead.tok == FN){
		//We'll just let the function definition rule handle this. If it fails, 
		//that will be caught above
		return function_definition(fl);
	
	//We'll let the definition rule handle this
	} else if(lookahead.tok == DEFINE || lookahead.tok == ALIAS){
		//Put whatever we saw back
		push_back_token(lookahead);

		//Call definition
		u_int8_t status = definition(fl);

		//If it's bad, we'll return an error node
		if(status == FAILURE){
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Otherwise we'll just return null, the caller will know what to do with it
		return NULL;

	//We'll let the replace rule handle this
	} else if(lookahead.tok == REPLACE){
		//We don't need to put it back
		u_int8_t status = replace_statement(fl);

		//If it's bad, we'll return an error node
		if(status == FAILURE){
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

		//Otherwise we'll just return null, the caller will know what to do with it
		return NULL;

	//This is an error. The #dependencies directive must be the very first thing in a 
	//file
	} else if(lookahead.tok == DEPENDENCIES){
		print_parse_message(PARSE_ERROR, "The #dependencies section must be the very first thing in a file", parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	//This is out of place if we find it
	} else if(lookahead.tok == REQUIRE){
		print_parse_message(PARSE_ERROR, "Any require statements must be nested in a top level #dependencies block", parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);

	//Otherwise it must be a declaration
	} else {
		//Put the token back
		push_back_token(lookahead);

		//We'll simply return whatever the product of the declaration function is
		return declaration(fl);
	}
}


/**
 * Here is our entry point. Like all functions, this returns
 * a reference to the root of the subtree it creates
 *
 * BNF Rule: <program>::= {<declaration-partition>}*
 */
static generic_ast_node_t* program(FILE* fl){
	//Freeze the line number
	Lexer_item lookahead;

	//If prog is null we make it here
	if(prog == NULL){
		//Create the ROOT of the tree
		prog = ast_node_alloc(AST_NODE_CLASS_PROG);
	}

	//We shouldn't even get here if this doesn't exist, but for our
	//purposes we need to skip through the #file FILE_NAME; top-level
	//declaration
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we don't see this we fail
	if(lookahead.tok != FILE_TOK){
		print_parse_message(PARSE_ERROR, "Top-level \"#file FILE_TOKEN;\" declaration required.", parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Now we need to see the file's identifier
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	
	//If it isn't an identifier we fail
	if(lookahead.tok != IDENT){
		print_parse_message(PARSE_ERROR, "Top-level \"#file FILE_TOKEN;\" declaration required.", parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//One last thing -- need to see the semicolon
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	
	//If it isn't an identifier we fail
	if(lookahead.tok != SEMICOLON){
		print_parse_message(PARSE_ERROR, "Semicolon required after top-level declaration", parser_line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
	}

	//Let's lookahead to see what we have
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we've actually found the comptime section, we'll
	//go through it until we don't have it anymore. The preprocessor
	//will have already consumed these tokens, so we need to get past them
	if(lookahead.tok == DEPENDENCIES){
		//Just run through here until we see the end of the comptime section
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

		//So long as we don't hit the end or the end of the region, keep going
		while(lookahead.tok != DEPENDENCIES && lookahead.tok != DONE){
			//Refresh the token
			lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
		}

		//If we get to the DONE, we error out
		if(lookahead.tok == DONE){
			print_parse_message(PARSE_ERROR, "Unmatched #dependencies region detected", parser_line_num);
			num_errors++;
			return ast_node_alloc(AST_NODE_CLASS_ERR_NODE);
		}

	} else {
		//Put it back
		push_back_token(lookahead);
	}
	
	//As long as we aren't done
	while((lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT)).tok != DONE){
		//Put the token back
		push_back_token(lookahead);

		//Call declaration partition
		generic_ast_node_t* current = declaration_partition(fl);

		//If it was NULL, we had a define or alias statement or implicit function declaration, so we'll move along
		if(current == NULL){
			continue;
		}

		//It failed, we'll bail right out if this is the case
		if(current->CLASS == AST_NODE_CLASS_ERR_NODE){
			//Just return the erroneous node
			return current;
		}
		
		//Otherwise, we'll add this as a child of the root
		add_child_node(prog, current);
		//And then we'll keep right along
	}

	//Line number is 0
	prog->line_number = 0;

	//Return the root of the tree
	return prog;
}


/**
 * Entry point for our parser. Everything beyond this point will be called in a recursive-descent fashion through
 * static methods
*/
front_end_results_package_t parse(FILE* fl, char* file_token){
	//We always reset the entire thing
	reset_file(fl);

	//Set the number of errors here
	num_errors = 0;
	num_warnings = 0;

	//Initialize all of our symtabs
	if(function_symtab == NULL && type_symtab == NULL && variable_symtab == NULL && constant_symtab == NULL){
		function_symtab = function_symtab_alloc();
		variable_symtab = variable_symtab_alloc();
		type_symtab = type_symtab_alloc();
		constant_symtab = constants_symtab_alloc(); 
	}

	//Initialize the OS call graph. This is because the OS always calls the main function
	if(os == NULL){
		os = calloc(1, sizeof(call_graph_node_t));
	}

	//Assign the file token
	current_file_token = file_token;

	//For the type and variable symtabs, their scope needs to be initialized before
	//anything else happens
	
	//Initialize the variable scope
	initialize_variable_scope(variable_symtab);
	//Global variable scope here
	initialize_type_scope(type_symtab);
	//Functions only have one scope, need no initialization

	//Add all basic types into the type symtab
	add_all_basic_types(type_symtab);

	//Also create a stack for our matching uses(curlies, parens, etc.)
	if(grouping_stack == NULL){
		grouping_stack = lex_stack_alloc();
	}

	//Global entry/run point, will give us a tree with
	//the root being here
	prog = program(fl);

	//Check for any unused functions
	check_for_unused_functions(function_symtab, &num_warnings);
	//Check for any bad variable declarations
	check_for_var_errors(variable_symtab, &num_warnings);

	//Initialize our results package here
	front_end_results_package_t results;

	//Package up everything that we need
	results.function_symtab = function_symtab;
	results.variable_symtab = variable_symtab;
	results.type_symtab = type_symtab;
	results.constant_symtab = constant_symtab;
	results.grouping_stack = grouping_stack;
	//AST root
	results.root = prog;
	//Call graph OS root
	results.os = os;
	//Record how many errors that we had
	results.num_errors = num_errors;
	results.num_warnings = num_warnings;
	//How many lines did we process?
	results.lines_processed = parser_line_num;

	return results;
}
