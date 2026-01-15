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
#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "parser.h"
#include "../utils/stack/lexstack.h"
#include "../utils/stack/nesting_stack.h"
#include "../utils/queue/heap_queue.h"
#include "../utils/stack/lightstack.h"
#include "../utils/constants.h"

//Define a generic error array global variable
char info[ERROR_SIZE];

//For printing all of our type names
char type_name_buf[MAX_IDENT_LENGTH];
char type_name_buf2[MAX_IDENT_LENGTH];

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
//The queue that holds all of our jump statements for a given function
static heap_queue_t current_function_jump_statements;

//Our stack for storing variables, etc
static lex_stack_t grouping_stack;

//Generic types here for us to repeatedly reference
static generic_type_t* immut_char = NULL;
static generic_type_t* immut_u8 = NULL;
static generic_type_t* immut_i8 = NULL;
static generic_type_t* immut_u16 = NULL;
static generic_type_t* immut_i16 = NULL;
static generic_type_t* immut_u32 = NULL;
static generic_type_t* immut_i32 = NULL;
static generic_type_t* immut_u64 = NULL;
static generic_type_t* immut_i64 = NULL;
static generic_type_t* immut_f32 = NULL;
static generic_type_t* immut_f64 = NULL;
static generic_type_t* mut_void = NULL;
static generic_type_t* immut_void = NULL;
static generic_type_t* immut_char_ptr = NULL;

//THe specialized nesting stack that we'll use to keep track of what kind of control structure we're in(loop, switch, defer, etc)
static nesting_stack_t nesting_stack;

//The number of errors
static u_int32_t num_errors;
//The number of warnings
static u_int32_t num_warnings;

//The current parser line number
static u_int32_t parser_line_num = 1;

//The overall node that holds all deferred statements for a function
generic_ast_node_t* deferred_stmts_node = NULL;

//Are we enabling debug printing? By default no
u_int8_t enable_debug_printing = FALSE;

//Did we find a main function? By default no
u_int8_t found_main_function = FALSE;

//The current file name
static char* current_file_name = NULL;

//Function prototypes are predeclared here as needed to avoid excessive restructuring of program
static generic_ast_node_t* cast_expression(FILE* fl, side_type_t side);
//What type are we given?
static generic_type_t* type_specifier(FILE* fl);
static symtab_type_record_t* type_name(FILE* fl, mutability_type_t mutability);
static u_int8_t alias_statement(FILE* fl);
static generic_ast_node_t* assignment_expression(FILE* fl);
static generic_ast_node_t* unary_expression(FILE* fl, side_type_t side);
static generic_ast_node_t* declaration(FILE* fl, u_int8_t is_global);
static generic_ast_node_t* compound_statement(FILE* fl);
static generic_ast_node_t* statement(FILE* fl);
static generic_ast_node_t* let_statement(FILE* fl, u_int8_t is_global);
static generic_ast_node_t* logical_or_expression(FILE* fl, side_type_t side);
static generic_ast_node_t* case_statement(FILE* fl, generic_ast_node_t* switch_stmt_node, int32_t* values, int32_t* current_case_value);
static generic_ast_node_t* default_statement(FILE* fl);
static generic_ast_node_t* declare_statement(FILE* fl, u_int8_t is_global);
static generic_ast_node_t* defer_statement(FILE* fl);
static generic_ast_node_t* idle_statement(FILE* fl);
static generic_ast_node_t* ternary_expression(FILE* fl, side_type_t side);
static generic_ast_node_t* initializer(FILE* fl, side_type_t side);
static generic_ast_node_t* function_predeclaration(FILE* fl);
//Definition is a special compiler-directive, it's executed here, and as such does not produce any nodes
static u_int8_t definition(FILE* fl);
static generic_type_t* validate_intializer_types(generic_type_t* target_type, generic_ast_node_t* initializer_node, u_int8_t is_global);


/**
 * Simply prints a parse message in a nice formatted way
*/
void print_parse_message(parse_message_type_t message_type, char* info, u_int32_t line_num){
	//Build and populate the message
	parse_message_t parse_message;
	parse_message.message = message_type;
	parse_message.info = info;
	parse_message.line_num = line_num;

	//Now print it
	//Mapped by index to the enum values
	char* type[] = {"WARNING", "ERROR", "INFO"};

	//Print this out on a single line
	fprintf(stdout, "\n[FILE: %s] --> [LINE %d | COMPILER %s]: %s\n", current_file_name, parse_message.line_num, type[parse_message.message], parse_message.info);
}


/**
 * Is a given type an enum type - accounting for all aliasing
 */
static inline u_int8_t is_enum_type(generic_type_t* type){
	//Dealias if need be
	type = dealias_type(type);

	return type->type_class == TYPE_CLASS_ENUMERATED ? TRUE : FALSE;
}


/**
 * Does an enum list contain a given value for a member?
 */
static inline u_int8_t does_enum_contain_integer_member(generic_type_t* enum_type, int32_t enum_member){
	//Extract the member table
	dynamic_array_t* member_table = &(enum_type->internal_types.enumeration_table);

	//Run through the enum type's table
	for(u_int16_t i = 0; i < member_table->current_index; i++){
		symtab_variable_record_t* member = dynamic_array_get_at((member_table), i);

		//Match found - we can get out
		if(member->enum_member_value == enum_member){
			return TRUE;
		}
	}

	//If we get down here we don't have it
	return FALSE;
}

/**
 * Perform any needed constant coercion that is being done for an assignment. This includes converting pointers to 64-bit
 * integers for constant coercion
 */
static inline void perform_constant_assignment_coercion(generic_ast_node_t* constant_node, generic_type_t* final_type){
	//If we have a pointer, we'll just make this into an i64
	if(final_type->type_class == TYPE_CLASS_POINTER){
		//Set the final type here
		constant_node->inferred_type = immut_i64;
	} else {
		//Set the final type here
		constant_node->inferred_type = final_type;
	}

	//If we have a basic constant type like this, we need to perform coercion
	switch(constant_node->constant_type){
		case CHAR_CONST:
		case BYTE_CONST:
		case BYTE_CONST_FORCE_U:
		case SHORT_CONST:
		case SHORT_CONST_FORCE_U:
		case INT_CONST:
		case INT_CONST_FORCE_U:
		case LONG_CONST:
		case LONG_CONST_FORCE_U:
		case FLOAT_CONST:
		case DOUBLE_CONST:
			coerce_constant(constant_node);
			break;
		//Otherwise do nothing
		default:
			break;
	}
}


/**
 * Determine whether or not a variable is able to be assigned to
 */
static inline u_int8_t can_variable_be_assigned_to(symtab_variable_record_t* variable){
	//Extract the type - it contains the mutability information
	generic_type_t* type = variable->type_defined_as;

	//If this hasn't been initialized then yes
	//we can assign to it
	if(variable->initialized == FALSE){
		return TRUE;
	}

	//Otherwise, let's see if the type allows us to be mutated 
	//If so - then we're fine. If not, then we fail
	if(type->mutability == MUTABLE){
		return TRUE;
	} else {
		return FALSE;
	}
}


/**
 * Insert a value into a list in sorted order(least to greatest).
 * We assume that the list is inherently large enought to do this.
 * This is meant primarily for case statement handling. It also validates
 * the uniqueness constraint of the list given in
 */
static int32_t sorted_list_insert_unique(int32_t* list, int32_t* max_index, int32_t value){
	//We will need this outside of the loop's scope
	int32_t i;

	//Run through everything in the list
	for(i = 0; i < *max_index; i++){
		//Once we've found it, we can get out
		if(value < list[i]){
			break;
		}

		//This invalidates the uniqueness constraint
		//so we need to fail out
		if(value == list[i]){
			return FALSE;
		}
	}

	//Bump this up now, we're going to have one more element
	*max_index += 1;

	//Shift everything in the list to the right to
	//make room
	for(int32_t j = *max_index - 1; j > i; j--){
		//Shift over by 1 each time
		list[j] = list[j - 1];
	}

	//And finally, put in our guy
	list[i] = value;

	//This worked
	return TRUE;
}


/**
 * Determine whether or not a duplicate function exists with the given
 * name
 *
 * Returns TRUE if successful, FALSE if not. This function handles
 * all error printing
 */
static inline u_int8_t do_duplicate_functions_exist(char* name){
	//Look it up
	symtab_function_record_t* found = lookup_function(function_symtab, name);

	//Fail out here
	if(found != NULL){
		sprintf(info, "Attempt to redefine function \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the function declaration
		print_function_name(found);
		num_errors++;

		//Return TRUE here, they do exist
		return TRUE;
	}

	//Otherwise just return FALSE
	return FALSE;
}


/**
 * Determine whether or not a duplicate variable exists with the given
 * name
 *
 * Returns TRUE if successful, FALSE if not. This function handles
 * all error printing
 */
static inline u_int8_t do_duplicate_variables_exist(char* name){
	//Look it up
	symtab_variable_record_t* found = lookup_variable(variable_symtab, name);

	//Means that we have a duplicate
	if(found != NULL){
		sprintf(info, "Attempt to redefine variable \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_variable_name(found);
		num_errors++;

		//Give back true, they do exist
		return TRUE;
	}

	//Otherwise just return FALSE
	return FALSE;
}


/**
 * Determine whether or not a duplicate *member variable* exists.
 * This is done by looking in the local scope only
 *
 * Returns TRUE if successful, FALSE if not. This function handles
 * all error printing
 */
static inline u_int8_t do_duplicate_member_variables_exist(char* name, generic_type_t* current_type){
	//Look it up
	symtab_variable_record_t* found = lookup_variable_local_scope(variable_symtab, name);

	//Means that we have a duplicate
	if(found != NULL){
		sprintf(info, "A member with name %s already exists in type %s. First defined here:", name, current_type->type_name.string);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		print_variable_name(found);
		num_errors++;

		//Give back true, they do exist
		return TRUE;
	}

	//Otherwise just return FALSE
	return FALSE;
}


/**
 * Check if a duplicate type record exists
 *
 * We will check for both mutable & immutable types
 *
 * Returns TRUE if successful, FALSE if not. This function handles
 * all error printing
 */
static inline u_int8_t do_duplicate_types_exist(char* name){
	//Look it up
	symtab_type_record_t* found = lookup_type_name_only(type_symtab, name, NOT_MUTABLE);

	//If the immutable one was found, we can leave out
	if(found != NULL){
		sprintf(info, "Attempt to redefine type \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_type_name(found);
		num_errors++;

		//We got a duplicate so we leave
		return TRUE;
	}

	//Otherwise let's check the mutable one as well
	found = lookup_type_name_only(type_symtab, name, MUTABLE);

	//If the immutable one was found, we can leave out
	if(found != NULL){
		sprintf(info, "Attempt to redefine type \"%s\". First defined here:", name);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_type_name(found);
		num_errors++;

		//We got a duplicate so we leave
		return TRUE;
	}

	//If we make it down here then we're good
	return FALSE;
}


/**
 * Determine whether or not something is an assignment operator
 */
static inline u_int8_t is_assignment_operator(ollie_token_t op){
	switch(op){
		case EQUALS:
		case LSHIFTEQ:
		case RSHIFTEQ:
		case XOREQ:
		case OREQ:
		case ANDEQ:
		case PLUSEQ:
		case MINUSEQ:
		case STAREQ:
		case SLASHEQ:
		case MODEQ:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Convert a compressed assignment operator into the equivalent binary 
 * operation
 */
static inline ollie_token_t compressed_assignment_to_binary_op(ollie_token_t op){
	switch(op){
		case LSHIFTEQ:
			return L_SHIFT;
		case RSHIFTEQ:
			return R_SHIFT;
		case XOREQ:
			return CARROT;
		case OREQ:
			return SINGLE_OR;
		case MODEQ:
			return MOD;
		case ANDEQ:
			return SINGLE_AND;
		case PLUSEQ:
			return PLUS;
		case MINUSEQ:
			return MINUS;
		case STAREQ:
			return STAR;
		case SLASHEQ:
			return F_SLASH;
		//We should never actually reach this
		default:
			return BLANK;
	}
}


/**
 * Is a given postfix expression tree address eligible or not
 */
static inline u_int8_t is_postfix_expression_tree_address_eligible(generic_ast_node_t* parent){
	//Grab the second child to overcome the primary expression
	generic_ast_node_t* cursor = parent->first_child->next_sibling;

	switch (cursor->ast_node_type) {
		case AST_NODE_TYPE_ARRAY_ACCESSOR:
		case AST_NODE_TYPE_STRUCT_ACCESSOR:
		case AST_NODE_TYPE_STRUCT_POINTER_ACCESSOR:
		case AST_NODE_TYPE_UNION_ACCESSOR:
		case AST_NODE_TYPE_UNION_POINTER_ACCESSOR:
			break;
		default:
			print_parse_message(PARSE_ERROR, "Invalid return value for address operation &", parser_line_num);
			return FAILURE;
	}

	//Return true if we made it here
	return TRUE;
}



/**
 * Determine the minimum bit width for an unsigned integer field that is needed based on a value that is passed
 * in
 *
 * Rules:
 * 	If we bit shift left by 8 and have 0, then our value can fit in 8 bits
 * 	If we shift left by 16 and have 0, then we can fit in 16 bits
 * 	If we shift left by 32 and have 0, then we can fit in 32 bits
 * 	Anything else -> 64 bits
 *
 * All of these will have types that are immutable because we don't expect to be changing them
 */
static inline generic_type_t* determine_required_minimum_unsigned_integer_type_size(u_int64_t value, u_int32_t max_size){
	//The case where we can use a u8
	if(max_size <= 8 || value >> 8 == 0){
		return immut_u8;
	}

	//We'll use u16
	if(max_size <= 16 || value >> 16 == 0){
		return immut_u16;
	}

	//We'll use u32
	if(max_size <= 32 || value >> 32 == 0){
		return immut_u32;
	}

	//Otherwise, we need 64 bits
	return immut_u64;
}


/**
 * Determine the minimum bit width for a signed integer field that is needed based on a value that is passed
 * in
 *
 * Rules:
 * 	If we bit shift left by 8 and have 0 OR -1, then our value can fit in 8 bits
 * 	If we shift left by 16 and have 0 OR -1, then we can fit in 16 bits
 * 	If we shift left by 32 and have 0 OR -1, then we can fit in 32 bits
 * 	Anything else -> 64 bits
 *
 */
static generic_type_t* determine_required_minimum_signed_integer_type_size(int64_t value, u_int32_t max_size){
	//The case where we can use an i8
	if(max_size <= 8 || value >> 8 == 0 || value >> 8 == -1){
		return immut_i8;
	}

	//We'll use an i16
	if(max_size <= 16 || value >> 16 == 0 || value >> 16 == -1){
		return immut_i16;
	}

	//We'll use an i32
	if(max_size <= 32 || value >> 32 == 0 || value >> 32 == -1){
		return immut_i32;
	}

	//Otherwise, we need 64 bits
	return immut_i64;
}


/**
 * Print out an error message. This avoids code duplicatoin becuase of how much we do this
 */
static generic_ast_node_t* print_and_return_error(char* error_message, u_int32_t parser_line_num){
	//Display the error
	print_parse_message(PARSE_ERROR, error_message, parser_line_num);
	//Increment the number of errors
	num_errors++;
	//Allocate and return an error node
	return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
}


/**
 * Emit a binary operation for the purpose of address manipulation
 *
 * Example:
 * int* + 1 -> int* + 4(an int is 4 bytes), and so on...
 * Same goes for arrays
 */
static generic_ast_node_t* generate_pointer_arithmetic(generic_ast_node_t* pointer, ollie_token_t op, generic_ast_node_t* operand, side_type_t side){
	//Grab the pointer/array type out
	generic_type_t* type = pointer->inferred_type;

	//If this is a void pointer, we're done
	if(type->internal_values.is_void_pointer == TRUE){
		return print_and_return_error("Void pointers cannot be added or subtracted to", parser_line_num);
	}

	//What do we multiply by?
	int64_t multiplicand;	

	//Go based on the type class here
	switch(type->type_class){
		//If it's a pointer, we multiply by whatever it points to
		case TYPE_CLASS_POINTER:
			multiplicand = type->internal_types.points_to->type_size;
			break;

		//If it's an array, we multiply by the size of the member type
		case TYPE_CLASS_ARRAY:
			multiplicand = type->internal_types.member_type->type_size;
			break;
		
		//Should never hit this
		default:
			printf("Fatal internal compiler error: unreachable path hit\n");
			exit(1);
	}

	//An efficiency enhancement here - if the operand itself is a constant, we can just generate
	//the constant directly and skip all of the extra allocation that we're working with
	//below
	if(operand->ast_node_type == AST_NODE_TYPE_CONSTANT){
		//The type is always an i64
		operand->inferred_type = immut_i64;
		
		//Coerce the operand
		coerce_constant(operand);

		//Multiply it by the size
		operand->constant_value.signed_long_value *= multiplicand;

		//And we're done, just give back the value
		return operand;
	}

	//Write out our constant multplicand
	generic_ast_node_t* constant_multiplicand = ast_node_alloc(AST_NODE_TYPE_CONSTANT, side);
	//Mark the type too
	constant_multiplicand->constant_type = LONG_CONST;
	//Store the size in here
	constant_multiplicand->constant_value.signed_long_value = multiplicand;
	//This one's type is always an immutable i64
	constant_multiplicand->inferred_type = immut_i64; 

	//Allocate an adjustment node
	generic_ast_node_t* adjustment = ast_node_alloc(AST_NODE_TYPE_BINARY_EXPR, side);

	//This is a multiplication node
	adjustment->binary_operator = STAR;

	//The first child is the actual operand
	add_child_node(adjustment, operand);

	//The second child is the constant_multiplicand
	add_child_node(adjustment, constant_multiplicand);

	//Generate a binary expression that we'll eventually return
	generic_ast_node_t* return_node = ast_node_alloc(AST_NODE_TYPE_BINARY_EXPR, side);

	//Save the operator
	return_node->binary_operator = op;

	//Add the pointer type as the first child
	add_child_node(return_node, pointer);

	//Add this to the return node
	add_child_node(return_node, adjustment);

	//These will all have the exact same types
	return_node->variable = pointer->variable;
	return_node->inferred_type = immut_i64;

	//Give back the final node
	return return_node;
}


/**
 * We will always return a pointer to the node holding the identifier. Due to the times when
 * this will be called, we can not do any symbol table validation here. 
 *
 * BNF "Rule": <identifier> ::= (<letter> | <digit> | _ | $){(<letter>) | <digit> | _ | $}*
 * Note all actual string parsing and validation is handled by the lexer
 */
static generic_ast_node_t* identifier(FILE* fl, side_type_t side){
	//Grab the next token
	lexitem_t lookahead = get_next_token(fl, &parser_line_num);
	
	//If we can't find it that's bad
	if(lookahead.tok != IDENT){
		sprintf(info, "String %s is not a valid identifier", lookahead.lexeme.string);
		return print_and_return_error(info, parser_line_num);
	}

	//Create the identifier node
	generic_ast_node_t* ident_node = ast_node_alloc(AST_NODE_TYPE_IDENTIFIER, side); //Add the identifier into the node itself
	//Idents are assignable
	ident_node->is_assignable = TRUE;
	//Clone the string in
	ident_node->string_value = clone_dynamic_string(&(lookahead.lexeme));

	//Add the line number
	ident_node->line_number = parser_line_num;

	//Return our reference to the node
	return ident_node;
}


/**
 * Directly emit an integer constant node. This is used exclusively for the user-defined direct
 * jump, and allows us to make every user-defined jump a direct jump when(1) jump. This greatly
 * simplifies our development processes
 */
static generic_ast_node_t* emit_direct_constant(int32_t constant){
	//Create our constant node
	generic_ast_node_t* constant_node = ast_node_alloc(AST_NODE_TYPE_CONSTANT, SIDE_TYPE_RIGHT);
	//Add the line number
	constant_node->line_number = parser_line_num;

	//This is an int_const
	constant_node->constant_type = INT_CONST;
	
	//Just make this one a signed 32 bit integer
	constant_node->inferred_type = immut_i32;

	//Give it the value
	constant_node->constant_value.signed_int_value = constant;

	return constant_node;
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
static generic_ast_node_t* constant(FILE* fl, side_type_t side){
	//Lookahead token
	lexitem_t lookahead;

	//We should see one of the 4 constants here
	lookahead = get_next_token(fl, &parser_line_num);

	//Create our constant node
	generic_ast_node_t* constant_node = ast_node_alloc(AST_NODE_TYPE_CONSTANT, side);
	//Add the line number
	constant_node->line_number = parser_line_num;

	//We'll go based on what kind of constant that we have
	switch(lookahead.tok){
		//Regular signed short value 
		case SHORT_CONST:
			//Mark what it is
			constant_node->constant_type = SHORT_CONST;

			//Store the integer value
			constant_node->constant_value.signed_short_value = atoi(lookahead.lexeme.string);

			//Use the helper rule to determine what size int we should initially have
			constant_node->inferred_type = determine_required_minimum_signed_integer_type_size(constant_node->constant_value.signed_short_value, 16);

			break;

		//Regular unsigned short value 
		case SHORT_CONST_FORCE_U:
			//Mark what it is
			constant_node->constant_type = SHORT_CONST_FORCE_U;

			//Store the integer value
			constant_node->constant_value.signed_short_value = atoi(lookahead.lexeme.string);

			//Use the helper rule to determine what size int we should initially have
			constant_node->inferred_type = determine_required_minimum_unsigned_integer_type_size(constant_node->constant_value.unsigned_short_value, 16);

			break;


		//Regular signed int
		case INT_CONST:
			//Mark what it is
			constant_node->constant_type = INT_CONST;

			//Store the integer value
			constant_node->constant_value.signed_int_value = atoi(lookahead.lexeme.string);

			//Use the helper rule to determine what size int we should initially have
			constant_node->inferred_type = determine_required_minimum_signed_integer_type_size(constant_node->constant_value.signed_int_value, 32);

			break;

		//Forced unsigned
		case INT_CONST_FORCE_U:
			//Mark what it is
			constant_node->constant_type = INT_CONST;
			//Store the int value we were given
			constant_node->constant_value.unsigned_int_value = atoi(lookahead.lexeme.string);

			//Use the helper rule to determine what size int we should initially have
			constant_node->inferred_type = determine_required_minimum_unsigned_integer_type_size(constant_node->constant_value.unsigned_int_value, 32);
			break;

		//Hex constants are really just integers
		case HEX_CONST:
			//Mark what it is 
			constant_node->constant_type = INT_CONST;
			//Store the int value we were given
			constant_node->constant_value.signed_int_value = strtol(lookahead.lexeme.string, NULL, 0);

			//Use the helper rule to determine what size int we should initially have
			constant_node->inferred_type = determine_required_minimum_signed_integer_type_size(constant_node->constant_value.signed_int_value, 32);

			break;

		//Regular signed long constant
		case LONG_CONST:
			//Store the type
			constant_node->constant_type = LONG_CONST;

			//Store the value we've been given
			constant_node->constant_value.signed_long_value = atol(lookahead.lexeme.string);

			//Get the size up to 64 bits
			constant_node->inferred_type = determine_required_minimum_signed_integer_type_size(constant_node->constant_value.signed_long_value, 64);

			break;

		//Unsigned long constant
		case LONG_CONST_FORCE_U:
			//Store the type
			constant_node->constant_type = LONG_CONST;

			//Store the value we've been given
			constant_node->constant_value.unsigned_long_value = atol(lookahead.lexeme.string);

			constant_node->inferred_type = determine_required_minimum_unsigned_integer_type_size(constant_node->constant_value.unsigned_long_value, 64);

			break;

		case FLOAT_CONST:
			constant_node->constant_type = FLOAT_CONST;
			//Grab the float val
			float float_val = atof(lookahead.lexeme.string);

			//Store the float value we were given
			constant_node->constant_value.float_value = float_val;

			//By default, float constants are of type float32
			constant_node->inferred_type = immut_f32;
			break;

		case DOUBLE_CONST:
			constant_node->constant_type = DOUBLE_CONST;
			//Grab the float val
			double double_value = atof(lookahead.lexeme.string);

			//Store the float value we were given
			constant_node->constant_value.double_value = double_value;

			//Double constants are always an f64
			constant_node->inferred_type = immut_f64;

			break;

		case CHAR_CONST:
			constant_node->constant_type = CHAR_CONST;
			//Grab the char val
			char char_val = *(lookahead.lexeme.string);

			//Store the char value that we were given
			constant_node->constant_value.char_value = char_val;

			//Char consts are of type char(obviously)
			constant_node->inferred_type = immut_char;
			break;

		//For True & False, they are internally treated the exact same as 
		//unsigned 8 bit integers
		case TRUE_CONST:
			//Unsigned byte
			constant_node->constant_type = BYTE_CONST_FORCE_U;
				
			//Use the true value here
			constant_node->constant_value.unsigned_byte_value = TRUE;

			//Inferred type is u8
			constant_node->inferred_type = immut_u8;

			break;
			
		case FALSE_CONST:
			//Unsigned byte
			constant_node->constant_type = BYTE_CONST_FORCE_U;
			
			//Use the true value here
			constant_node->constant_value.unsigned_byte_value = FALSE;

			//Inferred type is u8
			constant_node->inferred_type = immut_u8;

			break;

		case BYTE_CONST:
			//Signed byte
			constant_node->constant_type = BYTE_CONST;

			constant_node->constant_value.signed_byte_value = atoi(lookahead.lexeme.string);

			//Inferred type is i8
			constant_node->inferred_type = immut_i8;

			break;

		case BYTE_CONST_FORCE_U:
			//Unsigned byte
			constant_node->constant_type = BYTE_CONST_FORCE_U;

			constant_node->constant_value.unsigned_byte_value = atoi(lookahead.lexeme.string);

			//Inferred type is u8
			constant_node->inferred_type = immut_u8;

			break;

		case STR_CONST:
			constant_node->constant_type = STR_CONST;
			//The type is an immutable char*
			constant_node->inferred_type = immut_char_ptr;
			
			//The dynamic string is our value
			constant_node->string_value = lookahead.lexeme;

			break;

		default:
			//Create and return an error node that will be propagated up
			return print_and_return_error("Invalid constant given", parser_line_num);
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
 * BNF Rule: <function-call> ::= @<identifier>({<ternary_expression>}?{, <ternary_expression>}*)
 */
static generic_ast_node_t* function_call(FILE* fl, side_type_t side){
	//For any error printing if need be
	char error[ERROR_SIZE];
	//The current line num
	u_int16_t current_line = parser_line_num;
	//The lookahead token
	lexitem_t lookahead;
	//We'll also keep a nicer reference to the function name
	dynamic_string_t function_name;
	//The number of parameters that we've seen
	u_int8_t num_params = 0;
	
	//Grab the next token using the lookahead
	lookahead = get_next_token(fl, &parser_line_num);

	//We have a general error-probably will be quite uncommon
	if(lookahead.tok != IDENT){
		//We'll let the node propogate up
		return print_and_return_error("Non-identifier provided as funciton call", parser_line_num);
	}

	//Grab the function name out for convenience
	function_name = lookahead.lexeme;

	//A pointer that holds our function call node
	generic_ast_node_t* function_call_node;

	//Hold the overall type for error printing
	generic_type_t* function_type;

	//The generic type that holds our function signature
	function_type_t* function_signature;

	/**
	 * This identifier has the possibility of being a direct function call or a function pointer
	 * of some kind. To determine which it is, we'll need to look the name up in both symtabs
	 * and go accordingly
	 */
	
	//Lookup the variable
	symtab_variable_record_t* function_pointer_variable = lookup_variable(variable_symtab, function_name.string);

	//Let's now look up the function name in the function symtab
	symtab_function_record_t* function_record = lookup_function(function_symtab, function_name.string);

	//This is the most common case - that we have a simple, direct function call
	if(function_record != NULL){
		//Allocate this as a regular function call node
		function_call_node = ast_node_alloc(AST_NODE_TYPE_FUNCTION_CALL, side);

		//Store the function record in the node
		function_call_node->func_record = function_record;

		//Store the overall type
		function_type = function_record->signature;

		//Store our function signature
		function_signature = function_record->signature->internal_types.function_type;

		//We'll also add in that the current function has called this one
		call_function(current_function->call_graph_node, function_record->call_graph_node);
		//We'll now note that this was indeed called
		function_record->called = TRUE;

	//Otherwise if we see this case, then we have an indirect function call to deal with
	} else if(function_pointer_variable != NULL){
		//Strip the type away here
		function_type = dealias_type(function_pointer_variable->type_defined_as);

		//If this is not a function signature, then we can't call it as one
		if(function_type->type_class != TYPE_CLASS_FUNCTION_SIGNATURE){
			//Print and fail out here
			sprintf(error, "\"%s\" is defined as type %s, and cannot be called as a function. Only function types may be called", function_name.string, function_type->type_name.string);
			return print_and_return_error(info, parser_line_num);
		}

		//Now that we know this exists, we'll allocate this one as an indirect function call
		function_call_node = ast_node_alloc(AST_NODE_TYPE_INDIRECT_FUNCTION_CALL, side);

		//Store our funcion signature
		function_signature = function_type->internal_types.function_type;

		//Store the variable too
		function_call_node->variable = function_pointer_variable;

	//This means that they're both NULL. We'll need to throw an error here
	} else{
		sprintf(info, "\"%s\" is not currently defined as a function or function pointer", function_name.string);
		//Return the error node and get out
		return print_and_return_error(info, current_line);
	}

	//Add the inferred type in for convenience as well
	function_call_node->inferred_type = function_signature->return_type;
	
	//We now need to see a left parenthesis for our param list
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail out here
	if(lookahead.tok != L_PAREN){
		//Send this error node up the chain
		return print_and_return_error("Left parenthesis expected on function call", parser_line_num);
	}

	//Push onto the grouping stack once we see this
	push_token(&grouping_stack, lookahead);

	//Let's check for this easy case first. If we have no parameters, then 
	//we'll expect to immediately see an R_PAREN
	if(function_signature->num_params == 0){
		//Refresh the lookahead
		lookahead = get_next_token(fl, &parser_line_num);
		
		//If it's not an R_PAREN, then we fail
		if(lookahead.tok != R_PAREN){
			sprintf(info, "Function \"%s\" expects 0 parameters. Defined as: %s", function_name.string, function_type->type_name.string);
			print_parse_message(PARSE_ERROR, info, current_line);
			//Print out the actual function record as well
			num_errors++;
			//Return the error node
			return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, side);
		}

		//Otherwise if it was fine, we'll now pop the grouping stack
		pop_token(&grouping_stack);

		//And package up and return here
		//Add the line number in
		function_call_node->line_number = current_line;

		//Otherwise, if we make it here, we're all good to return the function call node
		return function_call_node;
	}

	/**
	 * Otherwise, if we get all the way down here, we know that we expect to see at least one
	 * value passed in as a parameter. We'll use do-while logic to process this in here
	 */

	//A node to hold our current parameter
	generic_ast_node_t* current_param;


	//So long as we don't see the R_PAREN we aren't done
	do {
		//Record that we saw one more parameter
		num_params++;

		//If we've already seen more than one parameter, we'll need a comma here
		if(num_params > 1){
			//Otherwise it must be a comma. If it isn't we have a failure
			if(lookahead.tok != COMMA){
				//Create and return an error node
				return print_and_return_error("Commas must be used to separate parameters in function call", parser_line_num);
			}
		}

		//We'll let the error below handle this, we just don't
		//want to segfault
		if(num_params > function_signature->num_params){
			break;
		}

		//Grab the current function param
		generic_type_t* param_type = function_signature->parameters[num_params - 1];

		//Parameters are in the form of a ternary expression
		current_param = ternary_expression(fl, side);

		//We now have an error of some kind
		if(current_param->ast_node_type == AST_NODE_TYPE_ERR_NODE){
			return print_and_return_error("Bad parameter passed to function call", current_line);
		}

		//Needed in this scope
		generic_type_t* final_type = NULL;

		//If we do *not* have a reference type, we will go through the normal types_assignable
		//pipeline(most common case)
		if(param_type->type_class != TYPE_CLASS_REFERENCE){
			//Let's see if we're even able to assign this here
			final_type = types_assignable(param_type, current_param->inferred_type);

			//If this is null, it means that our check failed
			if(final_type == NULL){
				sprintf(info, "Function \"%s\" expects an input of type \"%s%s\" as parameter %d, but was given an input of type \"%s%s\". Defined as: %s",
						function_name.string, 
						(param_type->mutability == MUTABLE ? "mut ": ""),
						param_type->type_name.string, num_params,
						//Print the mut keyword if we need it
						(current_param->inferred_type->mutability == MUTABLE ? "mut " : ""),
						current_param->inferred_type->type_name.string, function_type->type_name.string);

				//Use the helper to return this
				return print_and_return_error(info, parser_line_num);
			}

		//Otherwise, we have a reference type and we will do some special handling
		} else {
			//This is a hard no - we cannot have references being created on-the-fly
			//here, so if the user is trying to do something like increment a reference - that's a no
			if(current_param->inferred_type->type_class != TYPE_CLASS_REFERENCE
				//If it's an identifier, we can automatically make it a reference. If it's not,
				//then we're out of luck here
				&& current_param->ast_node_type != AST_NODE_TYPE_IDENTIFIER){
				sprintf(info, "Attempt to pass type %s%s to parameter of type %s%s",
							current_param->inferred_type->mutability == MUTABLE ? "mut ": "",
							current_param->inferred_type->type_name.string,
							param_type->mutability == MUTABLE ? "mut ": "",
							param_type->type_name.string);

				return print_and_return_error(info, parser_line_num);
			}

			//Let's see if we're even able to assign this here
			final_type = types_assignable(param_type, current_param->inferred_type);

			//If this is null, it means that our check failed
			if(final_type == NULL){
				sprintf(info, "Function \"%s\" expects an input of type \"%s%s\" as parameter %d, but was given an input of type \"%s%s\". Defined as: %s",
						function_name.string, 
						(param_type->mutability == MUTABLE ? "mut ": ""),
						param_type->type_name.string, num_params,
						//Print the mut keyword if we need it
						(current_param->inferred_type->mutability == MUTABLE ? "mut " : ""),
						current_param->inferred_type->type_name.string, function_type->type_name.string);

				//Use the helper to return this
				return print_and_return_error(info, parser_line_num);
			}

			//The param type is a reference, but the inferred type is not. What we'll do in this case 
			//is automatically make the variable that we're dealing with a stack variable
			if(current_param->inferred_type->type_class != TYPE_CLASS_REFERENCE){
				//Make this a stack variable - the CFG will auto-insert into the stack
				current_param->variable->stack_variable = TRUE;

				//Create the stack region here and now to avoid any confusion
				current_param->variable->stack_region = create_stack_region_for_type(&(current_function->data_area), current_param->inferred_type);
			}
		}

		//If this is a constant node, we'll force it to be whatever we expect from the type assignability
		if(current_param->ast_node_type == AST_NODE_TYPE_CONSTANT){
			current_param->inferred_type = final_type;

			//Do coercion
			perform_constant_assignment_coercion(current_param, final_type);
		}

		//We can now safely add this into the function call node as a child. In the function call node, 
		//the parameters will appear in order from left to right
		add_child_node(function_call_node, current_param);

		//Refresh the token
		lookahead = get_next_token(fl, &parser_line_num);

	//Keep going so long as we don't see a right paren
	} while (lookahead.tok != R_PAREN);


	//If we have a mismatch between what the function takes and what we want, throw an
	//error
	if(num_params != function_signature->num_params){
		sprintf(info, "Function %s expects %d parameters, but was given %d. Defined as: %s", 
		  function_name.string, function_signature->num_params, num_params, function_type->type_name.string);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		//Error out
		return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, side);
	}

	//Once we get here, we do need to finally verify that the closing R_PAREN matched the opening one
	if(pop_token(&grouping_stack).tok != L_PAREN){
		//Return the error node
		return print_and_return_error("Unmatched parenthesis detected in function call", parser_line_num);
	}

	//Add the line number in
	function_call_node->line_number = current_line;

	//Otherwise, if we make it here, we're all good to return the function call node
	return function_call_node;
}


/**
 * Handle a sizeof statement
 *
 * NOTE: By the time we get here, we have already seen and consumed the sizeof token
 */
static generic_ast_node_t* sizeof_statement(FILE* fl, side_type_t side){
	//Lookahead token
	lexitem_t lookahead;

	//We must then see left parenthesis
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail case here
	if(lookahead.tok != L_PAREN){
		//Use the helper for the error
		return print_and_return_error("Left parenthesis expected after sizeof call", parser_line_num);
	}

	//Otherwise we'll push to the stack for checking
	push_token(&grouping_stack, lookahead);

	//We now need to see a valid logical or expression. This expression will contain everything that we need to know, and the
	//actual expression result will be unused. It's important to note that we will not actually evaluate the expression here at
	//all - sall we can about is the return type
	generic_ast_node_t* expr_node = logical_or_expression(fl, side);
	
	//If it's an error
	if(expr_node->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		print_parse_message(PARSE_ERROR, "Unable to use sizeof on invalid expression",  parser_line_num);
		num_errors++;
		//It's already an error, so give it back that way
		return expr_node;
	}

	//Otherwise if we get here it actually was defined, so now we'll look for an R_PAREN
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail out here if we don't see it
	if(lookahead.tok != R_PAREN){
		return print_and_return_error("Right parenthesis expected after expression", parser_line_num);
	}

	//We can also fail if we somehow see unmatched parenthesis
	if(pop_token(&grouping_stack).tok != L_PAREN){
		return print_and_return_error("Unmatched parenthesis detected in typesize expression", parser_line_num);
	}

	//Now we know that we have an entirely syntactically valid call to sizeof. Let's now extract the 
	//type information for ourselves
	generic_type_t* return_type = expr_node->inferred_type;

	//Create a constant node
	generic_ast_node_t* const_node = ast_node_alloc(AST_NODE_TYPE_CONSTANT, side);

	//This will be an int const
	const_node->constant_type = INT_CONST_FORCE_U;
	//Store the actual value of the type size
	const_node->constant_value.unsigned_int_value = return_type->type_size;
	//Grab and store type info
	//This will always end up as a generic signed int
	const_node->inferred_type = determine_required_minimum_unsigned_integer_type_size(return_type->type_size, 32);
	//We cannot assign to this
	const_node->is_assignable = FALSE;
	//Store this too
	const_node->line_number = parser_line_num;

	//Finally we'll return this constant node
	return const_node;
}


/**
 * Handle a typesize expression
 *
 * NOTE: by the time we get here, we have already seen and consumed the typesize token
 */
static generic_ast_node_t* typesize_statement(FILE* fl, side_type_t side){
	//Lookahead token
	lexitem_t lookahead;

	//We must then see left parenthesis
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail case here
	if(lookahead.tok != L_PAREN){
		return print_and_return_error("Left parenthesis expected after typesize call", parser_line_num);
	}

	//Otherwise we'll push to the stack for checking
	push_token(&grouping_stack, lookahead);

	//Now we need to see a valid type-specifier. It is important to note that the type
	//specifier requires that a type has actually been defined. If it wasn't defined,
	//then this will return an error node
	generic_type_t* type_spec = type_specifier(fl);

	//If it's an error
	if(type_spec == NULL){
		return print_and_return_error("Unable to use typesize on undefined type",  parser_line_num);
	}

	//Once we've done this, we can grab the actual size of the type-specifier
	u_int32_t type_size = type_spec->type_size;

	//And then we no longer need the type-spec node, we can just remove it

	//Otherwise if we get here it actually was defined, so now we'll look for an R_PAREN
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail out here if we don't see it
	if(lookahead.tok != R_PAREN){
		return print_and_return_error("Right parenthesis expected after type specifer", parser_line_num);
	}

	//We can also fail if we somehow see unmatched parenthesis
	if(pop_token(&grouping_stack).tok != L_PAREN){
		return print_and_return_error("Unmatched parenthesis detected in typesize expression", parser_line_num);
	}

	//Create a constant node
	generic_ast_node_t* const_node = ast_node_alloc(AST_NODE_TYPE_CONSTANT, side);

	//Add the line number
	const_node->line_number = parser_line_num;
	//Add the constant
	const_node->constant_type = INT_CONST_FORCE_U;
	//Store the actual value
	const_node->constant_value.unsigned_int_value = type_size;
	//Grab and store type info
	//These will be generic signed ints
	const_node->inferred_type = determine_required_minimum_unsigned_integer_type_size(type_size, 32);

	//Finally we'll return this constant node
	return const_node;
}


/**
 * A primary expression is, in a way, the termination of our expression chain. However, it can be used 
 * to chain back up to an expression in general using () as an enclosure. Just like all rules, a primary expression
 * itself has a parent and will produce children. The reference to the primary expression itself is always returned
 *
 * BNF Rule: <primary-expression> ::= <identifier>
 * 									| <constant> 
 * 									| (<ternary_expression>)
 * 									| sizeof(<logical-or-expression>)
 * 									| typesize(<type-name>)
 * 									| <function-call>
 */
static generic_ast_node_t* primary_expression(FILE* fl, side_type_t side){
	//For the function call rule if we make it there
	generic_ast_node_t* func_call;

	//Freeze the current line number
	u_int16_t current_line = parser_line_num;
	//Lookahead token
	lexitem_t lookahead;

	//Grab the next token, we'll multiplex on this
	lookahead = get_next_token(fl, &parser_line_num);

	//Switch based on the token
	switch(lookahead.tok){
		//We've seen an ident, so we'll put it back and let
		//that rule handle it. This identifier will always be 
		//a variable. It must also be a variable that has been initialized.
		//We will check that it was initialized here
		case IDENT:
			//Put it back
			push_back_token(lookahead);

			//We will let the identifier rule actually grab the ident. In this case
			//the identifier will be a variable of some sort, that we'll need to check
			//against the symbol table
			generic_ast_node_t* ident = identifier(fl, side);

			//If there was a failure of some kind, we'll allow it to propogate up
			if(ident->ast_node_type == AST_NODE_TYPE_ERR_NODE){
				//Send the error up the chain
				return ident;
			}

			//Grab this out for convenience
			char* var_name = ident->string_value.string;

			//We have a few options here, we could find a constant that has been declared
			//like this. If so, we'll return a duplicate of the constant node that we have
			//inside of here
			symtab_constant_record_t* found_const = lookup_constant(constant_symtab, var_name);
			
			//If this is in fact a constant, we'll duplicate the whole thing and send it
			//out the door
			if(found_const != NULL){
				return duplicate_node(found_const->constant_node, side);
			}

			//Now we will look this up in the variable symbol table
			symtab_variable_record_t* found_var = lookup_variable(variable_symtab, var_name);

			//Let's look and see if we have a variable for use here. If we do, then
			//we're done with this exploration
			if(found_var != NULL){
				//If this var is itself an enum member, we need to treat it as a constant
				if(found_var->membership == ENUM_MEMBER){
					//We'll change the type of this node from an identifier to a constant
					ident->ast_node_type = AST_NODE_TYPE_CONSTANT;

					//Store the enum type inside of optional storage here
					ident->optional_storage.enum_type = found_var->type_defined_as;

					//Extract the enum integer type from here
					ident->inferred_type = found_var->type_defined_as->internal_values.enum_integer_type;

					//Store the constant value appropriately
					switch(ident->inferred_type->type_size){
						case 1:
							ident->constant_type = BYTE_CONST;
							ident->constant_value.signed_byte_value = found_var->enum_member_value;
							break;

						case 2:
							ident->constant_type = SHORT_CONST;
							ident->constant_value.signed_short_value = found_var->enum_member_value;
							break;

						case 4:
							ident->constant_type = INT_CONST;
							ident->constant_value.signed_int_value = found_var->enum_member_value;
							break;

						default:
							ident->constant_type = LONG_CONST;
							ident->constant_value.signed_long_value = found_var->enum_member_value;
							break;
							
					}

					//It is not assignable
					ident->is_assignable = FALSE;

					//Give it back
					return ident;
				}

				//If this is the right hand side and our variable is not initialized,
				//this is invalid as we are trying to use before initialization
				if(side == SIDE_TYPE_RIGHT 
					&& found_var->membership != GLOBAL_VARIABLE //We do not care for such checks with global vars
					&& found_var->initialized == FALSE){
					sprintf(info, "Attempt to use variable %s before initialization", found_var->var_name.string);
					return print_and_return_error(info, parser_line_num);
				}

				//Store the inferred type
				ident->inferred_type = found_var->type_defined_as;
				//Store the variable that's associated
				ident->variable = found_var;
				//Idents are assignable
				ident->is_assignable = TRUE;

				//Give back the ident node
				return ident;
			}

			//Attempt to find the function in here
			symtab_function_record_t* found_func = lookup_function(function_symtab, var_name);

			//Since a function value is constant and never changes, we will classify this record as a constant
			//If it could be found, then we're all set
			if(found_func != NULL){
				//We'll change the type of this node from an identifier to a constant
				ident->ast_node_type = AST_NODE_TYPE_CONSTANT;

				//The type of this value is a function constant
				ident->constant_type = FUNC_CONST;

				//This values type is the function's signature
				ident->inferred_type = found_func->signature;

				//Store the function record that we've found
				ident->func_record = found_func;

				//It is not assignable
				ident->is_assignable = FALSE;

				//Give it back
				return ident;
			}

			//Otherwise, if we reach all the way down to here, then we have an issue as
			//this identifier has never been declared as a function, variable or constant.
			//We'll through an error if this happens
			sprintf(info, "Variable \"%s\" has not been declared", var_name);
			return print_and_return_error(info, current_line);

		//If we see any constant
		case INT_CONST:
		case STR_CONST:
		case FLOAT_CONST:
		case DOUBLE_CONST:
		case CHAR_CONST:
		case BYTE_CONST:
		case BYTE_CONST_FORCE_U:
		case LONG_CONST:
		case HEX_CONST:
		case INT_CONST_FORCE_U:
		case LONG_CONST_FORCE_U:
			//Again put the token back
			push_back_token(lookahead);

			//Call the constant rule to grab the constant node
			generic_ast_node_t* constant_node = constant(fl, side);

			//Give back the constant node - if it's an error, the parent will handle
			return constant_node;
		
		//We can see a sizeof call
		case SIZEOF:
			//Let the helper handle this
			return sizeof_statement(fl, side);

		//If we see the typesize keyword, we are locked in to the typesize rule
		//The typesize rule is a compiler only directive. Since we know the size of all
		//valid types at compile-time, we will be able to return an INT-CONST node with the
		//size here
		case TYPESIZE:
			//Let the helper deal with this
			return typesize_statement(fl, side);

		//We could see a case where we have a parenthesis in an expression
		case L_PAREN:
			//We'll push it up to the stack for matching
			push_token(&grouping_stack, lookahead);

			//We are now required to see a valid ternary expression
			generic_ast_node_t* expr = ternary_expression(fl, side);

			//If it's an error, just give the node back
			if(expr->ast_node_type == AST_NODE_TYPE_ERR_NODE){
				return expr;
			}

			//Otherwise it worked, but we're still not done. We now must see the R_PAREN and
			//match it with the accompanying L_PAREN
			lookahead = get_next_token(fl, &parser_line_num);

			//Fail case here
			if(lookahead.tok != R_PAREN){
				//Create and return an error node
				return print_and_return_error("Right parenthesis expected after expression", parser_line_num);
			}

			//Another fail case, if they're unmatched
			if(pop_token(&grouping_stack).tok != L_PAREN){
				return print_and_return_error("Unmatched parenthesis detected", parser_line_num);
			}

			//Return the expression node
			return expr;

		//We could see a function call
		case AT:
			//We will let this rule handle the function call
			func_call = function_call(fl, side);

			//If we failed here
			if(func_call->ast_node_type == AST_NODE_TYPE_ERR_NODE){
				return func_call;
			}

			//Return the function call node
			return func_call;

		//If we get here we fail
		default:
			sprintf(info, "Expected identifier, constant or (<expression>), but got %s", lookahead.lexeme.string);
			return print_and_return_error(info, current_line);
	}
}


/**
 * Perform all mutability checking/bookkeeping for an assignment expression
 *
 * Cases that we cover:
 * 1.) Attempting to assign to an immutable "field variable" - think struct/union field
 * 2.) Attempting to assign to an immutable array area
 * 3.) Attempting to assign to a type regularly after it has been initialized
 */
static generic_ast_node_t* perform_mutability_checking(generic_ast_node_t* left_hand_expression_tree){
	//Extract the 
	symtab_variable_record_t* assignee = left_hand_expression_tree->variable;

	/**
	 * If we have a so-called "field variable", that means that this 
	 * unary expression is a postfix access of some kind. This is important
	 * because we'll need to check the type's mutability, not the assignee's
	 */
	if(left_hand_expression_tree->optional_storage.field_variable != NULL){
		//If this is immutable, we fail. We are not checking for anything like
		//initialization here, that is not possible to track
		if(left_hand_expression_tree->optional_storage.field_variable->type_defined_as->mutability == NOT_MUTABLE){
			//Fail out appropriately
			sprintf(info, "Field \"%s\" is not mutable. Fields must be declared as mutable to be assigned to.",
		   				left_hand_expression_tree->optional_storage.field_variable->var_name.string);
			return print_and_return_error(info, parser_line_num);
		}

		//If we have a variable, then this is definitely a mutation
		if(left_hand_expression_tree->variable != NULL){
			left_hand_expression_tree->variable->mutated = TRUE;
		}

	/**
	 * If we are ending in an array accessor, we would see a tree structure like:
	 * 			<postifx-node>
	 * 		       /   \
	 *      <subtree>  <array-accessor>
	 *
	 *  We need to dig through the tree to actually check this
	 */
	} else if(left_hand_expression_tree->ast_node_type == AST_NODE_TYPE_POSTFIX_EXPR){
		//Extract for our convenience
		generic_ast_node_t* first_child = left_hand_expression_tree->first_child;
		generic_ast_node_t* accessor = first_child->next_sibling;

		//We have an array accessor to check
		if(accessor->ast_node_type == AST_NODE_TYPE_ARRAY_ACCESSOR){
			if(first_child->inferred_type->mutability == NOT_MUTABLE){
				sprintf(info, "Attempt to mutate an immutable memory reference type \"%s\"", first_child->inferred_type->type_name.string);
				return print_and_return_error(info, parser_line_num);
			}
		}

		//If we have a variable, then this is definitely a mutation
		if(left_hand_expression_tree->variable != NULL){
			left_hand_expression_tree->variable->mutated = TRUE;
		}

	/**
	 * If we have some kind of pointer dereference, we would see a structure
	 * like this
	 *
	 * 		<unary-node>
	 * 		  /    \ 
	 * 	  <star>   <unary-node>
	 */
	} else if(left_hand_expression_tree->ast_node_type == AST_NODE_TYPE_UNARY_EXPR){
		//Extract for our convenience
		generic_ast_node_t* first_child = left_hand_expression_tree->first_child;
		generic_ast_node_t* dereferenced = first_child->next_sibling;

		if(first_child->ast_node_type == AST_NODE_TYPE_UNARY_OPERATOR
			&& first_child->unary_operator == STAR){

			//If this is immutable, we are trying to assign to an immutable value
			if(dereferenced->inferred_type->mutability == NOT_MUTABLE){
				sprintf(info, "Attempt to mutate an immutable memory reference type \"%s\"", dereferenced->inferred_type->type_name.string);
				return print_and_return_error(info, parser_line_num);
			}
		}

		//If we have a variable, then this is definitely a mutation
		if(left_hand_expression_tree->variable != NULL){
			left_hand_expression_tree->variable->mutated = TRUE;
		}

	} else {
		//This is the case where we have a plain variable assignment
		if(can_variable_be_assigned_to(assignee) == FALSE){
			sprintf(info, "Variable \"%s\" is not mutable and has already been initialized. Use mut keyword if you wish to mutate. First defined here:", assignee->var_name.string);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			print_variable_name(assignee);
			return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
		}

		/**
		 * Since we are not doing any kind of memory access here, now we can go
		 * through and update our mutability/initialization
		 */
		if(assignee->initialized == TRUE){
			assignee->mutated = TRUE;
		} else {
			assignee->initialized = TRUE;
		}
	}

	//Just give this back as a flag that we're fine
	return left_hand_expression_tree;
}


/**
 * An assignment expression can decay into a conditional expression or it
 * can actually do assigning. There is no chaining in Ollie language of assignments. There are two
 * options for treenodes here. If we see an actual assignment, there is a special assignment node
 * that will be made. If not, we will simply pass the parent along. An assignment expression will return
 * a reference to the subtree created by it
 *
 * BNF Rule: <assignment-expression> ::= <ternary-expression> 
 * 									   | <unary-expression> = <ternary-expression>
 * 									   | <unary-expression> <<= <ternary-expression>
 * 									   | <unary-expression> >>= <ternary-expression>
 * 									   | <unary-expression> += <ternary-expression>
 * 									   | <unary-expression> -= <ternary-expression>
 * 									   | <unary-expression> *= <ternary-expression>
 * 									   | <unary-expression> /= <ternary-expression>
 * 									   | <unary-expression> |= <ternary-expression>
 * 									   | <unary-expression> &= <ternary-expression>
 * 									   | <unary-expression> ^= <ternary-expression>
 *
 */
static generic_ast_node_t* assignment_expression(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	//Lookahead token
	lexitem_t lookahead;
	//For any eventual duplication
	generic_ast_node_t* left_hand_duplicate;

	//This will hold onto the assignment operator for us
	ollie_token_t assignment_operator = BLANK;

	//Probably way too much, just to be safe
	lex_stack_t stack = lex_stack_alloc();
	
	//Grab the next token
	lookahead = get_next_token(fl, &parser_line_num);

	//So long as we don't see a semicolon(end) or an assignment op, or a left or right curly
	while(is_assignment_operator(lookahead.tok) == FALSE && lookahead.tok != SEMICOLON && lookahead.tok != L_CURLY && lookahead.tok != R_CURLY){
		//Push lookahead onto the stack
		push_token(&stack, lookahead);

		//Otherwise refresh
		lookahead = get_next_token(fl, &parser_line_num);
	}

	//Save the assignment operator for later
	assignment_operator = lookahead.tok;

	//First push back lookahead, this won't be on the stack so it's needed that we do this
	push_back_token(lookahead);

	//Once we get here, we either found the assignment op or we didn't. First though, let's
	//put everything back where we found it
	while(lex_stack_is_empty(&stack) == FALSE){
		//Pop the token off and put it back
		push_back_token(pop_token(&stack));
	}
	
	//Once we make it here the lexstack has served its purpose, so we can scrap it
	lex_stack_dealloc(&stack);

	//If whatever our operator here is is not an assignment operator, we can just use the ternary rule
	if(is_assignment_operator(assignment_operator) == FALSE){
		return ternary_expression(fl, SIDE_TYPE_RIGHT);
	}

	//If we make it here however, that means that we did see the assign keyword. Since
	//this is the case, we'll make a new assignment node and take the appropriate actions here 
	generic_ast_node_t* asn_expr_node = ast_node_alloc(AST_NODE_TYPE_ASNMNT_EXPR, SIDE_TYPE_LEFT);
	//Add in the line number
	asn_expr_node->line_number = current_line;

	//We'll let this rule handle it
	generic_ast_node_t* left_hand_unary = unary_expression(fl, SIDE_TYPE_LEFT);

	//Fail out here
	if(left_hand_unary->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		return print_and_return_error("Invalid left hand side given to assignment expression", current_line);
	}
	
	//If it isn't assignable, we also fail
	if(left_hand_unary->is_assignable == FALSE){
		return print_and_return_error("Expression is not assignable", left_hand_unary->line_number);
	}

	//Sanitize based on the types here. Arrays and references specifically
	//cannot be assigned in a traditional sense
	switch(left_hand_unary->inferred_type->type_class){
		case TYPE_CLASS_ARRAY:
			return print_and_return_error("Array types are not assignable", left_hand_unary->line_number);

		//Reference types, whether they are mutable or not, may not be reassigned after they are declared and
		//initialized
		case TYPE_CLASS_REFERENCE:
			//Can't have an equals
			if(assignment_operator == EQUALS){
				return print_and_return_error("Reference types may only be assigned to in a \"let\" statement", parser_line_num);
			}

			//Other things are fine, like the compressed equality operators
			break;

		//If we don't have the 2 above, then we have no issue
		default:
			break;
	}

	//Otherwise it worked, so we'll add it in as the left child
	add_child_node(asn_expr_node, left_hand_unary);

	//Now we are required to see the := terminal
	lookahead = get_next_token(fl, &parser_line_num);
	
	//Fail case here
	if(is_assignment_operator(lookahead.tok) == FALSE){
		sprintf(info, "Expected assignment operator symbol in assignment expression");
		return print_and_return_error(info, parser_line_num);
	}

	//Holder for our expression
	generic_ast_node_t* expr = ternary_expression(fl, SIDE_TYPE_RIGHT);

	//Fail case here
	if(expr->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		return print_and_return_error("Invalid right hand side given to assignment expression", current_line);
	}

	//Let the helper do all mutability checking
	generic_ast_node_t* result = perform_mutability_checking(left_hand_unary);
	if(result->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		return result;
	}

	//Let's now see if we have compatible types
	generic_type_t* left_hand_type = left_hand_unary->inferred_type;
	generic_type_t* right_hand_type = expr->inferred_type;

	//What is our final type?
	generic_type_t* final_type = NULL;

	//If we have a generic assignment(=), we can just do the assignability
	//check
	if(assignment_operator == EQUALS){
		/**
		 * We will make use of the types assignable module here, as the rules are slightly 
		 * different than the types compatible rule
		 */
		final_type = types_assignable(left_hand_type, right_hand_type);

		//If they're not, we fail here
		if(final_type == NULL){
			//Let the helper generate
			generate_types_assignable_failure_message(info, right_hand_type, left_hand_type);
			return print_and_return_error(info, parser_line_num);
		}

		//If we have a constant, we will perform coercion
		if(expr->ast_node_type == AST_NODE_TYPE_CONSTANT){
			//Force the constant value to be the final type
			expr->inferred_type = final_type;

			//Now do the coercion
			perform_constant_assignment_coercion(expr, final_type);
		}

		//Otherwise the overall type is the final type
		asn_expr_node->inferred_type = final_type;

		//Otherwise we know it worked, so we'll add the expression in as the right child
		add_child_node(asn_expr_node, expr);

		//Return the reference to the overall node
		return asn_expr_node;
		
	//Otherwise, we'll need to perform any needed type coercion
	} else {
		//Convert this into the assignment operator
		ollie_token_t binary_op = compressed_assignment_to_binary_op(assignment_operator);

		//We need to account for the automatic dereference here
		if(left_hand_type->type_class == TYPE_CLASS_REFERENCE){
			left_hand_type = dereference_type(left_hand_type);
		}

		//Let's check if the left is valid
		if(is_binary_operation_valid_for_type(left_hand_type, binary_op, SIDE_TYPE_LEFT) == FALSE){
			sprintf(info, "Type %s is invalid for operation %s", left_hand_type->type_name.string, operator_to_string(assignment_operator));
			return print_and_return_error(info, parser_line_num);
		}

		//Let's also see if the right hand type is valid
		if(is_binary_operation_valid_for_type(right_hand_type, binary_op, SIDE_TYPE_RIGHT) == FALSE){
			sprintf(info, "Type %s is invalid for operation %s", right_hand_type->type_name.string, operator_to_string(assignment_operator));
			return print_and_return_error(info, parser_line_num);
		}

		//Go based on the first type to handle any special cases
		switch(left_hand_type->type_class){
			case TYPE_CLASS_POINTER:
			case TYPE_CLASS_ARRAY:
				//We'll also want to create a complete, distinct copy of the subtree here
				left_hand_duplicate = duplicate_subtree(left_hand_unary, SIDE_TYPE_RIGHT);

				//Let's first determine if they're compatible
				final_type = determine_compatibility_and_coerce(type_symtab, &(left_hand_duplicate->inferred_type), &(right_hand_type), binary_op);

				//If this fails, that means that we have an invalid operation
				if(final_type == NULL){
					sprintf(info, "Types %s and %s cannot be applied to operator %s", left_hand_duplicate->inferred_type->type_name.string, right_hand_type->type_name.string, operator_to_string(binary_op));
					return print_and_return_error(info, parser_line_num);
				}
				
				//We'll now generate the appropriate pointer arithmetic here where the right child is adjusted appropriately
				generic_ast_node_t* pointer_arithmetic = generate_pointer_arithmetic(left_hand_duplicate, binary_op, expr, SIDE_TYPE_RIGHT);

				//This is an overall child of the assignment expression
				add_child_node(asn_expr_node, pointer_arithmetic);

				break;

			default:
				/**
				 * If we have something like this:
				 * 				y(i32) += x(i64)
				 * 	This needs to fail because we cannot coerce y to be bigger than it already is, it's not assignable.
				 * 	As such, we need to check if the types are assignable first
				 */
				final_type = types_assignable(left_hand_type, right_hand_type);

				//If this fails, that means that we have an invalid operation
				if(final_type == NULL){
					generate_types_assignable_failure_message(info, right_hand_type, left_hand_type);
					return print_and_return_error(info, parser_line_num);
				}

				//If the expression is a constant, we force it to be the final type
				if(expr->ast_node_type == AST_NODE_TYPE_CONSTANT){
					expr->inferred_type = final_type;

					coerce_constant(expr);
				}

				//We'll also want to create a complete, distinct copy of the subtree here
				generic_ast_node_t* left_hand_duplicate = duplicate_subtree(left_hand_unary, SIDE_TYPE_RIGHT);

				//Determine type compatibility and perform coercions. We can only perform coercions on the left hand duplicate, because we
				//don't want to mess with the actual type of the variable
				final_type = determine_compatibility_and_coerce(type_symtab, &(left_hand_duplicate->inferred_type), &(expr->inferred_type), binary_op);

				//If this fails, that means that we have an invalid operation
				if(final_type == NULL){
					sprintf(info, "Types %s and %s cannot be applied to operator %s", left_hand_duplicate->inferred_type->type_name.string, right_hand_type->type_name.string, operator_to_string(assignment_operator));
					return print_and_return_error(info, parser_line_num);
				}

				//By the time that we get here, we know that all coercion has been completed
				//We can now construct our final result
				//Allocate the binary expression
				generic_ast_node_t* binary_op_node = ast_node_alloc(AST_NODE_TYPE_BINARY_EXPR, SIDE_TYPE_RIGHT);
				//Store the type and operator
				binary_op_node->inferred_type = final_type;
				binary_op_node->binary_operator = binary_op;

				//Now we'll add the duplicates in as children
				add_child_node(binary_op_node, left_hand_duplicate);
				add_child_node(binary_op_node, expr);

				//This is an overall child of the assignment expression
				add_child_node(asn_expr_node, binary_op_node);

				break;
		}

		//And now we can return this
		return asn_expr_node;
	}
}


/**
 * Handle a union pointer accessor(->) and return an appropriate AST node that
 * represents it
 *
 * BNF RULE: <union-pointer-accessor> ::= -> <identifier>
 *
 * NOTE: by the time we reach here, we'll have already seen the -> lexeme
 */
static generic_ast_node_t* union_pointer_accessor(FILE* fl, generic_type_t* current_type, side_type_t side){
	//Lookahead token
	lexitem_t lookahead;

	//If the current type here is not a pointer, then we can't do this
	current_type = dealias_type(current_type);

	//If this is not a pointer, then we can't use ::
	if(current_type->type_class != TYPE_CLASS_POINTER){
		sprintf(info, "Type \"%s\" is not a pointer to a union and cannot be accessed with the -> operator.", current_type->type_name.string);
		return print_and_return_error(info, parser_line_num);
	}

	//Now that we know this is a pointer, let's get what it points to
	generic_type_t* points_to = current_type->internal_types.points_to;

	//If this is not a union type, it immediately cannot be correct
	if(points_to->type_class != TYPE_CLASS_UNION){
		sprintf(info, "Type \"%s\" is not a union type and is incompatible with the -> operator", points_to->type_name.string);
		return print_and_return_error(info, parser_line_num);
	}

	//Following this, we need to see an identifier
	lookahead = get_next_token(fl, &parser_line_num);

	//If this is not an identifier, we fail out
	if(lookahead.tok != IDENT){
		return print_and_return_error("Identifier required after the . operator", parser_line_num);
	}

	//Extract the actual name
	dynamic_string_t variable_name = lookahead.lexeme;

	//Now we need to see if this is acutally a variable in the union
	symtab_variable_record_t* union_member = get_union_member(points_to, variable_name.string);

	//If this ends up being NULL, then we have an incorrect access
	if(union_member == NULL){
		sprintf(info, "Type %s does not have a member %s", points_to->type_name.string, variable_name.string);
		return print_and_return_error(info, parser_line_num);
	}

	//Now we'll allocate and pack up everything that we need
	generic_ast_node_t* union_accessor_node = ast_node_alloc(AST_NODE_TYPE_UNION_POINTER_ACCESSOR, side);

	union_accessor_node->line_number = parser_line_num;
	union_accessor_node->variable = union_member;
	//Store the field variable
	union_accessor_node->optional_storage.field_variable = union_member;
 	union_accessor_node->inferred_type = union_member->type_defined_as;
	union_accessor_node->is_assignable = TRUE;

	//And give it back
	return union_accessor_node;
}


/**
 * A struct accessor is used to access a struct either on the heap of or on the stack.
 * Like all rules, it will return a reference to the root node of the tree that it created
 *
 * A struct accessor node will be a subtree with the parent holding the actual operator
 * and its child holding the variable identifier
 *
 * BNF Rule: <struct-pointer-accessor> ::= => <variable-identifier> 
 */
static generic_ast_node_t* struct_pointer_accessor(FILE* fl, generic_type_t* current_type, side_type_t side){
	//The lookahead token
	lexitem_t lookahead;

	//Grab a convenient reference to the type that we're working with
	current_type = dealias_type(current_type);

	//If this is not a pointer, then we can't use ::
	if(current_type->type_class != TYPE_CLASS_POINTER){
		sprintf(info, "Type \"%s\" is not a pointer to a struct and cannot be accessed with the => operator.", current_type->type_name.string);
		return print_and_return_error(info, parser_line_num);
	}

	//Otherwise, we can symbolically dereference here and carry on
	current_type = current_type->internal_types.points_to;

	//We need to specifically see a struct here. If we don't then we leave
	if(current_type->type_class != TYPE_CLASS_STRUCT){
		sprintf(info, "Type \"%s\" cannot be accessed with the : operator.", current_type->type_name.string);
		return print_and_return_error(info, parser_line_num);
	}

	//Now we are required to see a valid variable identifier.
	lookahead = get_next_token(fl, &parser_line_num);

	//For now we're just doing error checking
	if(lookahead.tok != IDENT){
		return print_and_return_error("Struct accessor could not find valid identifier", parser_line_num);
	}

	//Grab this for nicety
	char* member_name = lookahead.lexeme.string;

	//Let's see if we can look this up inside of the type
	symtab_variable_record_t* var_record = get_struct_member(current_type, member_name);

	//If we can't find it we're out
	if(var_record == NULL){
		sprintf(info, "Variable \"%s\" is not a known member of struct %s", member_name, current_type->type_name.string);
		return print_and_return_error(info, parser_line_num);
	}

	//Otherwise we'll now make the node here
	generic_ast_node_t* struct_pointer_access_node = ast_node_alloc(AST_NODE_TYPE_STRUCT_POINTER_ACCESSOR, side);
	//Add the line number
	struct_pointer_access_node->line_number = parser_line_num;
	
	//Add the variable record into the node
	struct_pointer_access_node->is_assignable = TRUE;

	//Store the variable in here
	struct_pointer_access_node->variable = var_record;

	//Store the struct variable
	struct_pointer_access_node->optional_storage.field_variable = var_record;

	//Store the type
	struct_pointer_access_node->inferred_type = var_record->type_defined_as;

	//And now we're all done, so we'll just give back the root reference
	return struct_pointer_access_node;
}


/**
 * A struct accessor is used to access a struct either on the heap of or on the stack.
 * Like all rules, it will return a reference to the root node of the tree that it created
 *
 * A constructor accessor node will be a subtree with the parent holding the actual operator
 * and its child holding the variable identifier
 *
 * We will expect to see the => or : here
 *
 * BNF Rule: <struct-accessor> ::= : <variable-identifier> 
 */
static generic_ast_node_t* struct_accessor(FILE* fl, generic_type_t* current_type, side_type_t side){
	//Freeze the current line
	u_int16_t current_line = parser_line_num;
	//The lookahead token
	lexitem_t lookahead;

	//Grab a convenient reference to the type that we're working with
	current_type = dealias_type(current_type);

	//We need to specifically see a struct here. If we don't then we leave
	if(current_type->type_class != TYPE_CLASS_STRUCT){
		sprintf(info, "Type \"%s\" cannot be accessed with the : operator.", current_type->type_name.string);
		return print_and_return_error(info, parser_line_num);
	}

	//Now we are required to see a valid variable identifier.
	lookahead = get_next_token(fl, &parser_line_num);

	//For now we're just doing error checking
	if(lookahead.tok != IDENT){
		return print_and_return_error("Struct accessor could not find valid identifier", current_line);
	}

	//Grab this for nicety
	char* member_name = lookahead.lexeme.string;

	//Let's see if we can look this up inside of the type
	symtab_variable_record_t* var_record = get_struct_member(current_type, member_name);

	//If we can't find it we're out
	if(var_record == NULL){
		sprintf(info, "Variable \"%s\" is not a known member of struct %s", member_name, current_type->type_name.string);
		return print_and_return_error(info, parser_line_num);
	}

	//Otherwise we'll now make the node here
	generic_ast_node_t* struct_access_node = ast_node_alloc(AST_NODE_TYPE_STRUCT_ACCESSOR, side);
	//Add the line number
	struct_access_node->line_number = current_line;

	//Add the variable record into the node
	struct_access_node->is_assignable = TRUE;

	//Store the variable in here
	struct_access_node->variable = var_record;

	//Store the struct variable
	struct_access_node->optional_storage.field_variable = var_record;

	//Store the type
	struct_access_node->inferred_type = var_record->type_defined_as;

	//And now we're all done, so we'll just give back the root reference
	return struct_access_node;
}


/**
 * Access a member inside of a union node
 *
 * BNF RULE: <union-accessor> ::= .<identifier>
 *
 * REMEMBER: BY the time we get here, we have already seen the "." token
 */
static generic_ast_node_t* union_accessor(FILE* fl, generic_type_t* type, side_type_t side){
	//Lookahead token
	lexitem_t lookahead;

	//If this is not a union type, it immediately cannot be correct
	if(type->type_class != TYPE_CLASS_UNION){
		sprintf(info, "Type \"%s\" is not a union type and is incompatible with the . operator", type->type_name.string);
		return print_and_return_error(info, parser_line_num);
	}

	//Following this, we need to see an identifier
	lookahead = get_next_token(fl, &parser_line_num);

	//If this is not an identifier, we fail out
	if(lookahead.tok != IDENT){
		return print_and_return_error("Identifier required after the . operator", parser_line_num);
	}

	//Following this, we need to look and see if this identifier is indeed a type of this union
	symtab_variable_record_t* union_variable = get_union_member(type, lookahead.lexeme.string);

	//If this is NULL, it means that the var is not present and therefore this is incorrect
	if(union_variable == NULL){
		sprintf(info, "Variable %s is not a member of type %s\n", lookahead.lexeme.string, type->type_name.string); 
		return print_and_return_error(info, parser_line_num);
	}

	//Otherwise if we get here, we know that we do indeed have the needed union type. We can now go 
	//ahead with constructing the accessor
	generic_ast_node_t* union_accessor = ast_node_alloc(AST_NODE_TYPE_UNION_ACCESSOR, side);

	//Let's now populate with the appropriate variable and type
	union_accessor->variable = union_variable;
	//Store the field variable
	union_accessor->optional_storage.field_variable = union_variable;
	union_accessor->inferred_type = union_variable->type_defined_as;
	union_accessor->is_assignable = TRUE;

	//And give this back
	return union_accessor;
}


/**
 * An array accessor represents a request to get something from an array memory region. Like all
 * nodes, an array accessor will return a reference to the subtree that it creates
 *
 * We expect that the caller has given back the [ token for this rule
 *
 * BNF Rule: <array-accessor> ::= [ <ternary-expression> ]
 *
 */
static generic_ast_node_t* array_accessor(FILE* fl, generic_type_t* type, side_type_t side){
	//The lookahead token
	lexitem_t lookahead;
	//Freeze the current line
	u_int16_t current_line = parser_line_num;

	//Before we go on, let's see what we have as the current type here. Both arrays and pointers are subscriptable items
	if(type->type_class != TYPE_CLASS_ARRAY && type->type_class != TYPE_CLASS_POINTER){
		sprintf(info, "Type \"%s\" is not subscriptable", type->type_name.string);
		return print_and_return_error(info, parser_line_num);
	}

	//Now we are required to see a valid constant expression representing what
	//the actual index is.
	generic_ast_node_t* expr = ternary_expression(fl, side);

	//If we fail, automatic exit here
	if(expr->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		return print_and_return_error("Invalid conditional expression given to array accessor", current_line);
	}

	//Let's first check to see if this can be used in an array at all
	//If we can't we'll fail out here
	if(is_type_valid_for_memory_addressing(expr->inferred_type) == FALSE){
		sprintf(info, "Type %s cannot be used as an array index", expr->inferred_type->type_name.string);
		return print_and_return_error(info, parser_line_num);
	}

	//We use a u_int64 as our reference
	generic_type_t* reference_type = immut_u64;

	//Find the final type here. If it's not currently a U64, we'll need to coerce it
	generic_type_t* final_type = types_assignable(reference_type, expr->inferred_type);

	//Let's make sure that this is an int
	if(final_type == NULL){
		sprintf(info, "Array accessing requires types compatible with \"u64\", but instead got \"%s%s\"",
		  (expr->inferred_type->mutability == MUTABLE ? "mut ": ""),
		  expr->inferred_type->type_name.string);
		return print_and_return_error(info, parser_line_num);
	}

	//If this is a constant, we'll force it to be the final type
	if(expr->ast_node_type == AST_NODE_TYPE_CONSTANT){
		expr->inferred_type = final_type;
	}

	//Otherwise, once we get here we need to check for matching brackets
	lookahead = get_next_token(fl, &parser_line_num);

	//If wedon't see a right bracket, we'll fail out
	if(lookahead.tok != R_BRACKET){
		return print_and_return_error("Right bracket expected at the end of array accessor", parser_line_num);
	}

	//We also must check for matching with the brackets
	if(pop_token(&grouping_stack).tok != L_BRACKET){
		return print_and_return_error("Unmatched brackets detected in array accessor", current_line);
	}

	//Now that we've done all of our checks have been done, we can create the actual node
	generic_ast_node_t* accessor_node = ast_node_alloc(AST_NODE_TYPE_ARRAY_ACCESSOR, side);

	//Add the line number
	accessor_node->line_number = current_line;

	//The conditional expression is a child of this node
	add_child_node(accessor_node, expr);

	//The values type is what we point to/have as a member type
	if(type->type_class == TYPE_CLASS_POINTER){
		accessor_node->inferred_type = type->internal_types.points_to;
	} else {
		accessor_node->inferred_type = type->internal_types.member_type;
	}

	//This is assignable
	accessor_node->is_assignable = TRUE;

	//And now we're done so give back the root reference
	return accessor_node;
}


/**
 * Operates on a value before usage. This could be postincrement or postdecrement
 */
static generic_ast_node_t* postoperation(generic_type_t* current_type, generic_ast_node_t* parent_node, ollie_token_t operator, side_type_t side){
	//Let's first check and see if this is valid. If it's not we fail out
	if(is_unary_operation_valid_for_type(current_type, operator) == FALSE){
		sprintf(info, "Type %s is invalid for operator %s", current_type->type_name.string, operator_to_string(operator));
		return print_and_return_error(info, parser_line_num);
	}

	//Necessary checking here
	if(parent_node->variable != NULL){
		//This is a mutation - so we need to check it
		if(parent_node->variable->type_defined_as->mutability  == NOT_MUTABLE){
			sprintf(info, "Attempt to mutate immutable variable %s", parent_node->variable->var_name.string);
			return print_and_return_error(info, parser_line_num);
		}

		//This was assigned to
		parent_node->variable->mutated = TRUE;
	}

	//Otherwise let's allocate this
	generic_ast_node_t* postoperation_node = ast_node_alloc(AST_NODE_TYPE_POSTOPERATION, side);

	//The parent node is a child of this one
	add_child_node(postoperation_node, parent_node);

	//The inferred type is the current type *or* the dereferenced type
	//if we have a reference
	if(current_type->type_class != TYPE_CLASS_REFERENCE){
		postoperation_node->inferred_type = current_type;
	//Otherwise we have a reference - so on-the-fly deref here
	} else {
		postoperation_node->inferred_type = current_type->internal_types.references;
	}

	postoperation_node->line_number = parser_line_num;
	//Store the unary operator too
	postoperation_node->unary_operator = operator;

	//Add this in here
	postoperation_node->variable = parent_node->variable;

	//This is *not* assignable
	postoperation_node->is_assignable = FALSE;

	//And give it back
	return postoperation_node;
}


/**
 * A postfix expression decays into a primary expression, and there are certain
 * operators that can be chained if context allows. Like all other rules, this rule
 * returns a reference to the root node that it creates
 *
 * <postfix-expression> ::= <primary-expression> 
 *						  | <postfix-expression> => <identifier>
 *						  | <postfix-expression> [ <expression> ]
 *						  | <postfix-expression> : <identifier>
 *						  | <postfix-expression> . <identifier>
 *						  | <postfix-expression> -> <identifier>
 *						  | <postfix-expression> ++
 *						  | <postfix-expression> --
 *
 * We need to use factored form to avoid the left recursive issue
 *
 * The primary expression becomes the very bottom most part of the tree. We'll construct a tree in a similar was
 * as we do the expression trees, with each new postfix node becoming the new parent
 *
 * NOTE: Because the case statement operators(-> and :) lead to an ambiguous parse here, we have a special nesting
 * stack flag NESTING_CASE_CONDITION that allows us to disqualify all of these values here and bypass the rule
 *
 * <factored_postfix_expression> ::= <primary-expression> <postfix-tail>
 * 			<postfix-tail> ::= {=> <identifier> | : <identifier> | . <identifier> | -> <identifier> | [ <expression> ] | ++ | --}
 */ 
static generic_ast_node_t* postfix_expression(FILE* fl, side_type_t side){
	//Lookahead token
	lexitem_t lookahead;

	//No matter what, we have to first see a valid primary expression
	generic_ast_node_t* primary_expression_node = primary_expression(fl, side);

	//If we fail, then we're bailing out here
	if(primary_expression_node->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		//Just return, no need for any errors here
		return primary_expression_node;
	}

	//If we are inside of a case condition, we will just skip
	//out all of these rules because they cannot possibly
	//work
	if(peek_nesting_level(&nesting_stack) == NESTING_CASE_CONDITION){
		return primary_expression_node;
	}

	//The parent initially is the primary expression
	generic_ast_node_t* parent = primary_expression_node;
	//The temp holder node
	generic_ast_node_t* temp_holder = NULL;
	//Hold onto the operator node here
	generic_ast_node_t* operator_node = NULL;

	//So long as we keep seeing operators here, we keep chaining them
	while(TRUE){
		//Refresh the token
		lookahead = get_next_token(fl, &parser_line_num);

		//Let's grab whatever type that we currently have
		generic_type_t* current_type = parent->inferred_type;

		//Go based on what the token is
		switch(lookahead.tok){
			//Array accessor
			case L_BRACKET:
				//We'll push this onto the grouping stack for later matching
				push_token(&grouping_stack, lookahead);

				operator_node = array_accessor(fl, current_type, side);
				break;

			//Struct accessor
			case COLON:
				operator_node = struct_accessor(fl, current_type, side);
				break;

			//Struct pointer accessor
			case FAT_ARROW:
				operator_node = struct_pointer_accessor(fl, current_type, side);
				break;

			//Union accessor
			case DOT:
				//Let the rule handle it
				operator_node = union_accessor(fl, current_type, side);
				break;

			//Union pointer accessor
			case ARROW:
				operator_node = union_pointer_accessor(fl, current_type, side);
				break;

			//Postincrement
			case PLUSPLUS:
			case MINUSMINUS:
				//Copy this over
				parent->variable = primary_expression_node->variable;
				//Flag the parent as final - you can't go on past this
				parent->dereference_needed = TRUE;

				//We let this rule handle everything
				return postoperation(current_type, parent, lookahead.tok, side);

			//When we hit this, it means that we're done. We return the parent
			//node in this case
			default:
				push_back_token(lookahead);
				//Store the variable too
				parent->variable = primary_expression_node->variable;

				//Mark as final
				parent->dereference_needed = TRUE;
				//And give it back
				return parent;
		}

		//If this is invalid, then we'll just leave
		if(operator_node->ast_node_type == AST_NODE_TYPE_ERR_NODE){
			return print_and_return_error("Invalid postfix operation", parser_line_num);
		}

		//Hang onto the old parent
		temp_holder = parent;

		//The new parent now becomes a postfix expression
		parent = ast_node_alloc(AST_NODE_TYPE_POSTFIX_EXPR, side);

		//The old parent now becomes a child to the new postfix expression node
		add_child_node(parent, temp_holder);
		
		//And the operator is the other child
		add_child_node(parent, operator_node);

		//The type of this parent is always the type of the operator node
		parent->inferred_type = operator_node->inferred_type;

		//Transfer this around
		parent->optional_storage.field_variable = operator_node->optional_storage.field_variable;

		//The parent's assignability inherits from the operator
		parent->is_assignable = operator_node->is_assignable;
	}

	//We should never get here - just to make the static analyzer happy
	return parent;
}


/**
 * Is a given token a unary operator
 */
static u_int8_t is_unary_operator(ollie_token_t tok){
	//Switch on tok
	switch (tok) {
		case SINGLE_AND:
		case STAR:
		case MINUS:
		case MINUSMINUS:
		case PLUSPLUS:
		case L_NOT:
		case B_NOT:
			return TRUE;
		//By default no
		default:
			return FALSE;
	}
}


/**
 * A unary expression decays into a postfix expression. With a unary expression, we are able to
 * apply unary operators and take the size of given types. Like all rules, a unary expression
 * will always return a pointer to the root node of the tree that it creates
 *
 * BNF Rule: <unary-expression> ::= <postfix-expression> 
 * 								  | <unary-operator> <cast-expression> 
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
static generic_ast_node_t* unary_expression(FILE* fl, side_type_t side){
	//The lookahead token
	lexitem_t lookahead;
	//Is this assignable
	u_int8_t is_assignable = TRUE;

	//Let's see what we have
	lookahead = get_next_token(fl, &parser_line_num);
	//Save this for searching
	ollie_token_t unary_op_tok = lookahead.tok;

	//If this is not a unary operator, we don't need to go any more
	if(is_unary_operator(unary_op_tok) == FALSE){
		//Push it back
		push_back_token(lookahead);

		//Let this handle the heavy lifting
		return postfix_expression(fl, side);
	}

	//Otherwise, if we get down here we know that we have a unary operator
	
	//We'll first create the unary operator node for ourselves here
	generic_ast_node_t* unary_op = ast_node_alloc(AST_NODE_TYPE_UNARY_OPERATOR, side);
	//Assign the operator to this
	unary_op->unary_operator = lookahead.tok;

	//We will process this as if we are on the "right hand side" of an equation, because we are
	//still reading from items here
	generic_ast_node_t* cast_expr = cast_expression(fl, SIDE_TYPE_RIGHT);

	//Let's check for errors
	if(cast_expr->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		return print_and_return_error("Invalid cast expression given after unary operator", parser_line_num);
	}

	//Holder for our return type
	generic_type_t* return_type;

	//An is-valid holder for our validations
	u_int8_t is_valid = FALSE;

	//Now we'll switch based on the operator
	switch(unary_op_tok){
		//Pointer derferencing
		case STAR:
			//Check to see if it's valid
			is_valid = is_unary_operation_valid_for_type(cast_expr->inferred_type, unary_op_tok);

			//If it it's invalid, we fail here
			if(is_valid == FALSE){
				sprintf(info, "Type %s is invalid for operator %s", cast_expr->inferred_type->type_name.string, operator_to_string(unary_op_tok));
				return print_and_return_error(info, parser_line_num);
			}

		
			//Otherwise if we made it here, we only have one final tripping point
			//Ensure that we aren't trying to deref a null pointer
			if(cast_expr->inferred_type->type_class == TYPE_CLASS_POINTER && cast_expr->inferred_type->internal_values.is_void_pointer == TRUE){
				return print_and_return_error("Attempt to derefence void*, you must cast before derefencing", parser_line_num);
			}

			//Otherwise our dereferencing worked, so the return type will be whatever this points to
			//Grab what it references whether its a pointer or an array
			if(cast_expr->inferred_type->type_class == TYPE_CLASS_POINTER){
				return_type = cast_expr->inferred_type->internal_types.points_to;
			} else {
				return_type = cast_expr->inferred_type->internal_types.member_type;
			}

			//This is assignable
			is_assignable = TRUE;
			break;

		//Address operator case
		case SINGLE_AND:
			switch(cast_expr->ast_node_type){
				//We can take an identifiers address
				case AST_NODE_TYPE_IDENTIFIER:
					//If this is not already a memory region, then we need to flag it as one
					//for later so that the cfg constructor knows what we'll eventually need to
					//load
					if(is_memory_region(cast_expr->variable->type_defined_as) == FALSE
						//AND it's not a global var
						&& cast_expr->variable->membership != GLOBAL_VARIABLE){

						//IMPORTANT - we need to flag this as a stack variable now
						cast_expr->variable->stack_variable = TRUE;
					}

					break;
				//And we can handle a postfix expression
				case AST_NODE_TYPE_POSTFIX_EXPR:
					//If this fails then we leave
					if(is_postfix_expression_tree_address_eligible(cast_expr) == FALSE){
						return print_and_return_error("Invalid address operation attempt", parser_line_num);
					}
					break;
				
				//Otherwise it doesn't work
				default:
					return print_and_return_error("Invalid return value for address operator &", parser_line_num);
			}

			//Check to see if it's valid
			u_int8_t is_valid = is_unary_operation_valid_for_type(cast_expr->inferred_type, unary_op_tok);

			//If it it's invalid, we fail here
			if(is_valid == FALSE){
				sprintf(info, "Type %s is invalid for operator %s", cast_expr->inferred_type->type_name.string, operator_to_string(unary_op_tok));
				return print_and_return_error(info, parser_line_num);
			}

			//Otherwise it worked just fine, so we'll create a type of pointer to whatever it's type was. The mutability here is *always*
			//the child's mutability by default. If the user wants that to change, they need to cast
			generic_type_t* pointer = create_pointer_type(cast_expr->inferred_type, parser_line_num, cast_expr->inferred_type->mutability);

			//We'll check to see if this type is already in existence
			symtab_type_record_t* type_record = lookup_type(type_symtab, pointer);
			
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
			is_assignable = FALSE;

			break;

		//Logical not case
		case L_NOT:
			//Check to see if it's valid
			is_valid = is_unary_operation_valid_for_type(cast_expr->inferred_type, unary_op_tok);

			//If it it's invalid, we fail here
			if(is_valid == FALSE){
				sprintf(info, "Type %s is invalid for operator %s", cast_expr->inferred_type->type_name.string, operator_to_string(unary_op_tok));
				return print_and_return_error(info, parser_line_num);
			}

			//The return type of a logical not is a boolean
			return_type = lookup_type_name_only(type_symtab, "bool", NOT_MUTABLE)->type;
			
			//This is not assignable
			is_assignable = FALSE;
			
			break;
	
		//Bitwise not case
		case B_NOT:
			//Check to see if it's valid
			is_valid = is_unary_operation_valid_for_type(cast_expr->inferred_type, unary_op_tok);

			//If it it's invalid, we fail here
			if(is_valid == FALSE){
				sprintf(info, "Type %s is invalid for operator %s", cast_expr->inferred_type->type_name.string, operator_to_string(unary_op_tok));
				return print_and_return_error(info, parser_line_num);
			}

			//The inferred type is the current type *or* the dereferenced type
			//if we have a reference
			if(cast_expr->inferred_type->type_class != TYPE_CLASS_REFERENCE){
				return_type = cast_expr->inferred_type;
			//Otherwise we have a reference - so on-the-fly deref here
			} else {
				return_type = cast_expr->inferred_type->internal_types.references;
			}

			//This is not assignable
			is_assignable = FALSE;
			
			break;
	
		//Arithmetic negation case
		case MINUS:
			//Let's see if it's valid
			is_valid = is_unary_operation_valid_for_type(cast_expr->inferred_type, unary_op_tok);

			//If it it's invalid, we fail here
			if(is_valid == FALSE){
				sprintf(info, "Type %s is invalid for operator %s", cast_expr->inferred_type->type_name.string, operator_to_string(unary_op_tok));
				return print_and_return_error(info, parser_line_num);
			}

			//The inferred type is the current type *or* the dereferenced type
			//if we have a reference
			if(cast_expr->inferred_type->type_class != TYPE_CLASS_REFERENCE){
				return_type = cast_expr->inferred_type;
			//Otherwise we have a reference - so on-the-fly deref here
			} else {
				return_type = cast_expr->inferred_type->internal_types.references;
			}

			//This is not assignable
			is_assignable = FALSE;

			break;

		/**
		 * Prefix operations involve the actual increment/decrement and the saving operation. The value
		 * that is returned to be used by the user is the incremented/decremented value unlike in a
		 * postfix expression
		 *
		 * We'll need to apply desugaring here
		 * ++x is really temp = x + 1
		 * 				 x = temp
		 * 				 use temp going forward
		 *
		 * We must note that preincrement is not assignable at all
		 */
		case PLUSPLUS:
		case MINUSMINUS:
			//Check to see if it is valid
			is_valid = is_unary_operation_valid_for_type(cast_expr->inferred_type, unary_op_tok);

			//If it it's invalid, we fail here
			if(is_valid == FALSE){
				sprintf(info, "Type %s is invalid for operator %s", cast_expr->inferred_type->type_name.string, operator_to_string(unary_op_tok));
				return print_and_return_error(info, parser_line_num);
			}

			//The inferred type is the current type *or* the dereferenced type
			//if we have a reference
			if(cast_expr->inferred_type->type_class != TYPE_CLASS_REFERENCE){
				return_type = cast_expr->inferred_type;
			//Otherwise we have a reference - so on-the-fly deref here
			} else {
				return_type = cast_expr->inferred_type->internal_types.references;
			}

			//This counts as mutation -- unless it's a constant
			if(cast_expr->variable != NULL){
				//If this is not mutable, then we cannot change it in this way
				if(cast_expr->variable->type_defined_as->mutability == NOT_MUTABLE){
					sprintf(info, "Attempt to mutate immutable variable %s", cast_expr->variable->var_name.string);
					return print_and_return_error(info, parser_line_num);
				}

				//This is a mutation
				cast_expr->variable->mutated = TRUE;
			}

			//Force this to be an rvalue for the cfg constructor
			cast_expr->side = SIDE_TYPE_RIGHT;

			//These expressions are never assignable
			is_assignable = FALSE;

			break;

		//In reality, we should never reach here
		default:
			return print_and_return_error("Fatal internal compiler error: invalid unary operation", parser_line_num);
	}

	//If we have a constant here, we have a chance to do some optimizations
	if(cast_expr->ast_node_type == AST_NODE_TYPE_CONSTANT){
		//Go based on this
		switch (unary_op_tok) {
			case MINUS:
				negate_constant_value(cast_expr);
				return cast_expr;

			case MINUSMINUS:
				decrement_constant_value(cast_expr);
				return cast_expr;

			case PLUSPLUS:
				increment_constant_value(cast_expr);
				return cast_expr;

			case L_NOT:
				logical_not_constant_value(cast_expr);
				return cast_expr;

			case B_NOT:
				bitwise_not_constant_value(cast_expr);
				return cast_expr;
			//Just do nothing
			default:
				break;
		}
	}

	//One we get here, we have both nodes that we need
	generic_ast_node_t* unary_node = ast_node_alloc(AST_NODE_TYPE_UNARY_EXPR, side);
	
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
}


/**
 * A cast expression decays into a unary expression
 *
 * BNF Rule: <cast-expression> ::= <unary-expression> 
 * 						    	| < <type-specifier> > <unary-expression>
 */
static generic_ast_node_t* cast_expression(FILE* fl, side_type_t side){
	//The lookahead token
	lexitem_t lookahead;

	//If we first see an angle bracket, we know that we are truly doing
	//a cast. If we do not, then this expression is just a pass through for
	//a unary expression
	lookahead = get_next_token(fl, &parser_line_num);
	
	//If it's not the <, put the token back and just return the unary expression
	if(lookahead.tok != L_THAN){
		push_back_token(lookahead);

		//Let this handle it
		return unary_expression(fl, side);
	}

	//Push onto the stack for matching
	push_token(&grouping_stack, lookahead);

	//Grab the type specifier
	generic_type_t* type_spec = type_specifier(fl);

	//If it's an error, we'll print and propagate it up
	if(type_spec == NULL){
		return print_and_return_error("Invalid type specifier given to cast expression", parser_line_num);
	}

	//We now have to see the closing braces that we need
	lookahead = get_next_token(fl, &parser_line_num);

	//If we didn't see a match
	if(lookahead.tok != G_THAN){
		return print_and_return_error("Expected closing > at end of cast", parser_line_num);
	}

	//Make sure we match
	if(pop_token(&grouping_stack).tok != L_THAN){
		return print_and_return_error("Unmatched angle brackets given to cast statement", parser_line_num);
	}

	//Now we have to see a valid unary expression. This is our last potential fail case in the chain
	//The unary expression will handle this for us
	generic_ast_node_t* right_hand_unary = unary_expression(fl, side);

	//If it's an error we'll jump out
	if(right_hand_unary->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		return right_hand_unary;
	}

	//No we'll need to determine if we can actually cast here
	//What we're trying to cast to
	generic_type_t* casting_to_type = dealias_type(type_spec);
	//What is being casted
	generic_type_t* being_casted_type = dealias_type(right_hand_unary->inferred_type);

	//You can never cast a "void" to anything
	if(IS_VOID_TYPE(being_casted_type) == TRUE){
		sprintf(info, "Type %s cannot be casted to any other type", being_casted_type->type_name.string);
		return print_and_return_error(info, parser_line_num);
	}

	//Likewise, you can never cast anything to void
	if(IS_VOID_TYPE(casting_to_type) == TRUE){
		sprintf(info, "Type %s cannot be casted to type %s", being_casted_type->type_name.string, casting_to_type->type_name.string);
		return print_and_return_error(info, parser_line_num);
	}

	//You can never cast anything to be a struct 
	if(casting_to_type->type_class == TYPE_CLASS_STRUCT){
		return print_and_return_error("No type can be casted to a struct type", parser_line_num);
	}

	//You can never cast anything to be a union 
	if(casting_to_type->type_class == TYPE_CLASS_UNION){
		return print_and_return_error("No type can be casted to a union type", parser_line_num);
	}

	//You can never cast anything to be an array 
	if(casting_to_type->type_class == TYPE_CLASS_ARRAY){
		return print_and_return_error("No type can be casted to an array type", parser_line_num);
	}

	/**
	 * If the type that is being casted is a memory region, and we are
	 * trying to cast it to a pointer of some kind, we need to be careful about 
	 * the way in which mutability is handled
	 */
	if(is_memory_region(being_casted_type) == TRUE
		&& casting_to_type->type_class == TYPE_CLASS_POINTER){

		//This is an error
		if(being_casted_type->mutability == NOT_MUTABLE
			&& casting_to_type->mutability == MUTABLE){
			//Fail out here
			sprintf(info, "Attempt to cast an immutable type %s to a mutable pointer type %s is illegal", being_casted_type->type_name.string, casting_to_type->type_name.string);
			return print_and_return_error(info, parser_line_num);
		}
	}

	/**
	 * We will use the types_assignable function to check this
	 */
	generic_type_t* return_type = types_assignable(casting_to_type, being_casted_type);

	//This is our fail case
	if(return_type == NULL){
		generate_types_assignable_failure_message(info, being_casted_type, casting_to_type);
		return print_and_return_error(info, parser_line_num);
	}

	//These types are now inferenced
	right_hand_unary->inferred_type = type_spec;

	//If we are casting a constant node, we should perform all needed
	//type coercion now
	if(right_hand_unary->ast_node_type == AST_NODE_TYPE_CONSTANT){
		perform_constant_assignment_coercion(right_hand_unary, type_spec);
	}

	//Finally, we're all set to go here, so we can return the root reference
	return right_hand_unary;
}


/**
 * A multiplicative expression can be chained and decays into a cast expression. This method
 * will return a pointer to the root of the subtree that is created by it, whether that subtree
 * originated here or not
 *
 * BNF Rule: <multiplicative-expression> ::= <cast-expression>{ (* | / | %) <cast-expression>}*
 */
static generic_ast_node_t* multiplicative_expression(FILE* fl, side_type_t side){
	//Lookahead token
	lexitem_t lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;
	//Holding the return type
	generic_type_t* return_type;
	//Is the temp holder a constant node?
	u_int8_t temp_holder_is_constant = FALSE;
	//Is the right child a constant node?
	u_int8_t right_child_is_constant = FALSE;

	//No matter what, we do need to first see a valid cast expression expression
	generic_ast_node_t* sub_tree_root = cast_expression(fl, side);

	//Obvious fail case here
	if(sub_tree_root->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		//If this is an error, we can just propogate it up
		return sub_tree_root;
	}

	//There are now two options. If we do not see any *'s or %'s or /, we just add 
	//this node in as the child and move along. But if we do see * or % or / symbols,
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num);
	
	//As long as we have a multiplication operators(* or % or /) 
	while(lookahead.tok == MOD || lookahead.tok == STAR || lookahead.tok == F_SLASH){
		//Save this lexer item
		lexitem_t op = lookahead;

		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//Store whether or not the temp holder is a constant for later processing
		temp_holder_is_constant = temp_holder->ast_node_type == AST_NODE_TYPE_CONSTANT ? TRUE : FALSE; 

		//Let's see if this is a valid type or not
		u_int8_t temp_holder_valid = is_binary_operation_valid_for_type(temp_holder->inferred_type, op.tok, SIDE_TYPE_LEFT);

		//Fail case here
		if(temp_holder_valid == FALSE){
			sprintf(info, "Type %s is invalid for operator %s", temp_holder->inferred_type->type_name.string, operator_to_string(op.tok));
			return print_and_return_error(info, parser_line_num);
		}

		//Now we have no choice but to see a valid cast expression again
		right_child = cast_expression(fl, side);

		//If it's an error, just fail out
		if(right_child->ast_node_type == AST_NODE_TYPE_ERR_NODE){
			//If this is an error we can just propogate it up
			return right_child;
		}

		//Store whether or not the right child is a constant for later
		//processing
		right_child_is_constant = right_child->ast_node_type == AST_NODE_TYPE_CONSTANT ? TRUE : FALSE;


		//If we have a 0 in the right hand child, and we're trying
		//to divide or mod, that would cause errors so we will catch it now
		if(right_child_is_constant == TRUE 
			&& is_constant_node_value_0(right_child) == TRUE){
			//Go based on the operator
			switch(op.tok){
				//Hard fail case, can't divide by 0
				case F_SLASH:
					return print_and_return_error("Attempt to divide by 0", parser_line_num);
				//Hard fail case, can't mod by 0
				case MOD:
					return print_and_return_error("Attempt to modulo by 0", parser_line_num);
				//Anything else(the * operation) is just fine
				default:
					break;
			}
		}

		//Let's see if this is a valid type or not
		u_int8_t right_child_valid = is_binary_operation_valid_for_type(temp_holder->inferred_type, op.tok, SIDE_TYPE_RIGHT);

		//Fail case here
		if(right_child_valid == FALSE){
			sprintf(info, "Type %s is invalid for operator %s", temp_holder->inferred_type->type_name.string, operator_to_string(op.tok));
			return print_and_return_error(info, parser_line_num);
		}

		//Use the type compatibility function to determine compatibility and apply necessary coercions
		return_type = determine_compatibility_and_coerce(type_symtab, &(temp_holder->inferred_type), &(right_child->inferred_type), op.tok);

		//If this fails, that means that we have an invalid operation
		if(return_type == NULL){
			sprintf(info, "Types %s and %s cannot be applied to operator %s", temp_holder->inferred_type->type_name.string, right_child->inferred_type->type_name.string, operator_to_string(op.tok));
			return print_and_return_error(info, parser_line_num);
		}

		//Now that we have the return type, we will perform type coercion based on whether or not each
		//leaf node is a constant
		if(temp_holder_is_constant == TRUE){
			coerce_constant(temp_holder);
		}

		//Same for the right child
		if(right_child_is_constant == TRUE){
			coerce_constant(right_child);
		}

		/**
		 * If we discover that the user is just trying to add/subtract two constants together, then we can do a preemptive
		 * optimization by adding them together right now and just giving back a constant node. The node that we give back
		 * will always
		 */
		if(temp_holder_is_constant == TRUE && right_child_is_constant == TRUE){
			//Go based on the token
			switch(op.tok){
				case STAR:
					//Multiply, result is in temp holder
					multiply_constant_nodes(temp_holder, right_child);
					break;

				case F_SLASH:
					//Divide, result is in temp holder
					divide_constant_nodes(temp_holder, right_child);
					break;

				case MOD:
					//Modulo, result is in temp holder
					mod_constant_nodes(temp_holder, right_child);
					break;

				//Unreachable
				default:
					break;
			}

			//The right child is now useless, so we can scrap it. The temp holder is the sub-tree-root
			sub_tree_root = temp_holder;

			//The root's type is now the inferred type
			sub_tree_root->inferred_type = return_type;

			//By the end of this, we always have a proper subtree with the operator as the root, being held in 
			//"sub-tree root". We'll now refresh the token to keep looking
			lookahead = get_next_token(fl, &parser_line_num);
			
			//Skip forward
			continue;
		}

		//We now need to make an operator node
		sub_tree_root = ast_node_alloc(AST_NODE_TYPE_BINARY_EXPR, side);

		//We'll now assign the binary expression it's operator
		sub_tree_root->binary_operator = lookahead.tok;

		//Add these both in in order
		add_child_node(sub_tree_root, temp_holder);
		add_child_node(sub_tree_root, right_child);

		//Assign the node type
		sub_tree_root->inferred_type = return_type;

		//By the end of this, we always have a proper subtree with the operator as the root, being held in 
		//"sub-tree root". We'll now refresh the token to keep looking
		lookahead = get_next_token(fl, &parser_line_num);
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
static generic_ast_node_t* additive_expression(FILE* fl, side_type_t side){
	//Lookahead token
	lexitem_t lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;
	//Hold the return type for us here
	generic_type_t* return_type;
	//In case we need it for the pointer math
	symtab_variable_record_t* variable = NULL;
	//Is the temp holder a constant node?
	u_int8_t temp_holder_is_constant = FALSE;
	//Is the right child a constant node?
	u_int8_t right_child_is_constant = FALSE;

	//No matter what, we do need to first see a valid multiplicative expression
	generic_ast_node_t* sub_tree_root = multiplicative_expression(fl, side);

	//Obvious fail case here
	if(sub_tree_root->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		//If this is an error, we can just propogate it up
		return sub_tree_root;
	}
	
	//There are now two options. If we do not see any +'s or -'s, we just add 
	//this node in as the child and move along. But if we do see + or - symbols,
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num);
	
	//As long as we have a additive operators(+ or -) 
	while(lookahead.tok == PLUS || lookahead.tok == MINUS){
		//Save the lookahead
		lexitem_t op = lookahead;
		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//Cache the comparison here - we'll need it later
		temp_holder_is_constant = temp_holder->ast_node_type == AST_NODE_TYPE_CONSTANT ? TRUE : FALSE;

		//Let's see if this actually works
		u_int8_t left_type_valid = is_binary_operation_valid_for_type(temp_holder->inferred_type, op.tok, SIDE_TYPE_LEFT);
		
		//Fail out here
		if(left_type_valid == FALSE){
			sprintf(info, "Type %s is invalid for operator %s", temp_holder->inferred_type->type_name.string, operator_to_string(op.tok));
			return print_and_return_error(info, parser_line_num);
		}


		//Now we have no choice but to see a valid multiplicative expression again
		right_child = multiplicative_expression(fl, side);

		//If it's an error, just fail out
		if(right_child->ast_node_type == AST_NODE_TYPE_ERR_NODE){
			//If this is an error we can just propogate it up
			return right_child;
		}

		//Cache the result - we'll need it for later
		right_child_is_constant = right_child->ast_node_type == AST_NODE_TYPE_CONSTANT ? TRUE : FALSE;

		//Let's see if this actually works
		u_int8_t right_type_valid = is_binary_operation_valid_for_type(right_child->inferred_type, op.tok, SIDE_TYPE_RIGHT);
		
		//Fail out here
		if(right_type_valid == FALSE){
			sprintf(info, "Type %s is invalid for operator %s on the right side of a binary operation", right_child->inferred_type->type_name.string, operator_to_string(op.tok));
			return print_and_return_error(info, parser_line_num);
		}

		//Go based on what kind of type we have here
		switch(temp_holder->inferred_type->type_class){
			case TYPE_CLASS_POINTER:
			case TYPE_CLASS_ARRAY:
				//Let's first determine if they're compatible
				return_type = determine_compatibility_and_coerce(type_symtab, &(temp_holder->inferred_type), &(right_child->inferred_type), op.tok);

				//If this fails, that means that we have an invalid operation
				if(return_type == NULL){
					sprintf(info, "Types %s and %s cannot be applied to operator %s", temp_holder->inferred_type->type_name.string, right_child->inferred_type->type_name.string, operator_to_string(op.tok));
					return print_and_return_error(info, parser_line_num);
				}

				//We'll now generate the appropriate pointer arithmetic here where the right child is adjusted appropriately
				generic_ast_node_t* pointer_arithmetic = generate_pointer_arithmetic(temp_holder, op.tok, right_child, side);

				//Copy the variable over here for later use
				variable = temp_holder->variable;

				//Once we're done here, the right child is the pointer arithmetic
				right_child = pointer_arithmetic;
				
				break;
				
			default:
				//Use the type compatibility function to determine compatibility and apply necessary coercions
				return_type = determine_compatibility_and_coerce(type_symtab, &(temp_holder->inferred_type), &(right_child->inferred_type), op.tok);

				//If this fails, that means that we have an invalid operation
				if(return_type == NULL){
					sprintf(info, "Types %s and %s cannot be applied to operator %s", temp_holder->inferred_type->type_name.string, right_child->inferred_type->type_name.string, operator_to_string(op.tok));
					return print_and_return_error(info, parser_line_num);
				}

				break;
		}

		/**
		 * For constant types, we need to perform type/storage
		 * rememediation on the internal constant values based on 
		 * what the return type is. We will do this now. Remember
		 * that the inferred types of the constant nodes have already
		 * been manipulated by the type coercer above
		 */
		if(temp_holder_is_constant == TRUE){
			coerce_constant(temp_holder);
		}

		if(right_child_is_constant == TRUE){
			coerce_constant(right_child);
		}

		/**
		 * If we discover that the user is just trying to add/subtract two constants together, then we can do a preemptive
		 * optimization by adding them together right now and just giving back a constant node. The node that we give back
		 * will always
		 */
		if(temp_holder_is_constant == TRUE && right_child_is_constant == TRUE){
			//Go based on the token
			switch(op.tok){
				//Add the two constants
				case PLUS:
					//Add them, result is in temp-holder
					add_constant_nodes(temp_holder, right_child);
					break;

				case MINUS:
					//Subtract them, result is in temp-holder
					subtract_constant_nodes(temp_holder, right_child);
					break;

				//Unreachable
				default:
					break;
			}

			//The right child is now useless, so we can scrap it. The temp holder is the sub-tree-root
			sub_tree_root = temp_holder;

			//Update the type appropriately
			sub_tree_root->inferred_type = return_type;

			//By the end of this, we always have a proper subtree with the operator as the root, being held in 
			//"sub-tree root". We'll now refresh the token to keep looking
			lookahead = get_next_token(fl, &parser_line_num);
			
			//Skip forward
			continue;
		}

		//Now that everything above is good, we can make the operator node
		sub_tree_root = ast_node_alloc(AST_NODE_TYPE_BINARY_EXPR, side);
		//We'll now assign the binary expression it's operator
		sub_tree_root->binary_operator = op.tok;

		//We actually already know this guy's first child--it's the previous root currently
		//being held in temp_holder. We'll add the temp holder in as the subtree root
		add_child_node(sub_tree_root, temp_holder);

		//Otherwise, he is the right child of the sub_tree_root, so we'll add it in
		add_child_node(sub_tree_root, right_child);
		
		//Now we can finally assign the sub tree type
		sub_tree_root->inferred_type = return_type;

		//Copy over the variable. It will either be NULL(common case) or it will carry
		//the value of the pointer that we manipulated
		sub_tree_root->variable = variable;

		//By the end of this, we always have a proper subtree with the operator as the root, being held in 
		//"sub-tree root". We'll now refresh the token to keep looking
		lookahead = get_next_token(fl, &parser_line_num);
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
static generic_ast_node_t* shift_expression(FILE* fl, side_type_t side){
	//Lookahead token
	lexitem_t lookahead;
	//Op token for clarity
	lexitem_t op;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;
	//Hold the return type
	generic_type_t* return_type;
	//Flag whether the temp holder is a constant
	u_int8_t temp_holder_is_constant = FALSE;
	//Flag whether the right child is a constant
	u_int8_t right_child_is_constant = FALSE;

	//No matter what, we do need to first see a valid additive expression
	generic_ast_node_t* sub_tree_root = additive_expression(fl, side);

	//Obvious fail case here
	if(sub_tree_root->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		//If this is an error, we can just propogate it up
		return sub_tree_root;
	}
	
	//There are now two options. If we do not see any shift operators, we just add 
	//this node in as the child and move along. But if we do see shift operator symbols,
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num);

	//Go based on which token we have
	switch(lookahead.tok){
		case L_SHIFT:
		case R_SHIFT:
			//Save the lexer item here
			op = lookahead;

			//Hold the reference to the prior root
			temp_holder = sub_tree_root;

			//Flag whether this is constant or not
			temp_holder_is_constant = temp_holder->ast_node_type == AST_NODE_TYPE_CONSTANT ? TRUE : FALSE;

			//Let's see if this actually works
			u_int8_t is_left_type_shiftable = is_binary_operation_valid_for_type(temp_holder->inferred_type, op.tok, SIDE_TYPE_LEFT);
			
			//Fail out here
			if(is_left_type_shiftable == FALSE){
				sprintf(info, "Type %s is invalid for a bitwise shift operation", temp_holder->inferred_type->type_name.string); 
				return print_and_return_error(info, parser_line_num);
			}

			//Now we have no choice but to see a valid additive expression again
			right_child = additive_expression(fl, side);

			//If it's an error, just fail out
			if(right_child->ast_node_type == AST_NODE_TYPE_ERR_NODE){
				//If this is an error we can just propogate it up
				return right_child;
			}

			//Flag whether or not the right child is a constant
			right_child_is_constant = right_child->ast_node_type == AST_NODE_TYPE_CONSTANT ? TRUE : FALSE;

			//Let's see if this actually works
			u_int8_t is_right_type_shiftable = is_binary_operation_valid_for_type(right_child->inferred_type, op.tok, SIDE_TYPE_RIGHT);
			
			//Fail out here
			if(is_right_type_shiftable == FALSE){
				sprintf(info, "Type %s is invalid for a bitwise shift operation", temp_holder->inferred_type->type_name.string); 
				return print_and_return_error(info, parser_line_num);
			}

			//The return type is always the left child's type
			return_type = determine_compatibility_and_coerce(type_symtab, &(temp_holder->inferred_type), &(right_child->inferred_type), op.tok);

			//If this fails, that means that we have an invalid operation
			if(return_type == NULL){
				sprintf(info, "Types %s and %s cannot be applied to operator %s", temp_holder->inferred_type->type_name.string, right_child->inferred_type->type_name.string, operator_to_string(op.tok));
				return print_and_return_error(info, parser_line_num);
			}

			//If the temp holder is a constant, perform any needed coercions here
			if(temp_holder_is_constant == TRUE){
				coerce_constant(temp_holder);
			}

			//Same for the right child
			if(right_child_is_constant == TRUE){
				coerce_constant(right_child);
			}

			//If they are both constants, we will skip any extra allocations and just do
			//the constant math right inside of here
			if(temp_holder_is_constant == TRUE && right_child_is_constant == TRUE){
				//Go based on the operator and invoke the appropriate rule
				//here
				switch(op.tok){
					case R_SHIFT:
						right_shift_constant_nodes(temp_holder, right_child);
						break;
					case L_SHIFT:
						left_shift_constant_nodes(temp_holder, right_child);
						break;
					default:
						break;
				}

				//The temp holder now has our entire answer
				sub_tree_root = temp_holder;

				//And get out - we'll skip the below allocations
				break;
			}

			//Only now are we good to allocate
			sub_tree_root = ast_node_alloc(AST_NODE_TYPE_BINARY_EXPR, side);
			//We'll now assign the binary expression it's operator
			sub_tree_root->binary_operator = lookahead.tok;

			//Store the return type
			sub_tree_root->inferred_type = return_type;

			//Put both children in order
			add_child_node(sub_tree_root, temp_holder);
			add_child_node(sub_tree_root, right_child);

			break;

		default:
			//Otherwise just push the token back
			push_back_token(lookahead);
			break;
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
static generic_ast_node_t* relational_expression(FILE* fl, side_type_t side){
	//Lookahead token
	lexitem_t lookahead;
	//Op token for readability
	lexitem_t op;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;
	//The return type
	generic_type_t* return_type;
	//Track whether temp and right child collapsed to constants
	u_int8_t temp_holder_is_constant = FALSE;
	u_int8_t right_child_is_constant = FALSE;

	//No matter what, we do need to first see a valid shift expression
	generic_ast_node_t* sub_tree_root = shift_expression(fl, side);

	//Obvious fail case here
	if(sub_tree_root->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		//If this is an error, we can just propogate it up
		return sub_tree_root;
	}
	
	//There are now two options. If we do not see any relational operators, we just add 
	//this node in as the child and move along. But if we do see relational operator symbols,
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num);

	//Switch is faster since we have an enum so it can use a jump table
	switch(lookahead.tok){
		//Handle all relational operations
		case G_THAN:
		case G_THAN_OR_EQ:
		case L_THAN:
		case L_THAN_OR_EQ:
			//Store this for readability
			op = lookahead;

			//Hold the reference to the prior root
			temp_holder = sub_tree_root;

			//Flag whether it's a constant
			temp_holder_is_constant = temp_holder->ast_node_type == AST_NODE_TYPE_CONSTANT ? TRUE : FALSE;

			//Let's check to see if this type is valid for our operation
			u_int8_t is_temp_holder_valid = is_binary_operation_valid_for_type(temp_holder->inferred_type, op.tok, SIDE_TYPE_LEFT);

			//This is our fail case
			if(is_temp_holder_valid == FALSE){
				sprintf(info, "Type %s is invalid for operator %s", temp_holder->inferred_type->type_name.string, operator_to_string(op.tok)); 
				return print_and_return_error(info, parser_line_num);
			}

			//Now we have no choice but to see a valid shift again
			right_child = shift_expression(fl, side);

			//If it's an error, just fail out
			if(right_child->ast_node_type == AST_NODE_TYPE_ERR_NODE){
				//If this is an error we can just propogate it up
				return right_child;
			}

			//Is this a constant? Figure out and store
			right_child_is_constant = right_child->ast_node_type == AST_NODE_TYPE_CONSTANT ? TRUE : FALSE;

			//Let's check to see if this type is valid for our operation
			u_int8_t is_right_child_valid = is_binary_operation_valid_for_type(right_child->inferred_type, op.tok, SIDE_TYPE_RIGHT);

			//This is our fail case
			if(is_right_child_valid == FALSE){
				sprintf(info, "Type %s is invalid for operator %s", right_child->inferred_type->type_name.string, operator_to_string(op.tok)); 
				return print_and_return_error(info, parser_line_num);
			}

			//The return type is always the left child's type
			return_type = determine_compatibility_and_coerce(type_symtab, &(temp_holder->inferred_type), &(right_child->inferred_type), op.tok);

			//If this fails, that means that we have an invalid operation
			if(return_type == NULL){
				sprintf(info, "Types %s and %s cannot be applied to operator %s", temp_holder->inferred_type->type_name.string, right_child->inferred_type->type_name.string, operator_to_string(op.tok));
				return print_and_return_error(info, parser_line_num);
			}

			//Now that we know we've done valid type coercion, perform any constant coercion
			//that is needed here
			if(temp_holder_is_constant == TRUE){
				coerce_constant(temp_holder);
			}

			//And same for the right child
			if(right_child_is_constant == TRUE){
				coerce_constant(right_child);
			}

			//If these are both constants, we can skip the entire allocation
			//below and just invoke the simplifier straight out
			if(temp_holder_is_constant == TRUE && right_child_is_constant == TRUE){
				//Invoke the specific rule that corresponds to our operator. The result
				//will always come back to us in the temp holder
				switch(op.tok){
					case G_THAN:
						greater_than_constant_nodes(temp_holder, right_child);
						break;
					case L_THAN:
						less_than_constant_nodes(temp_holder, right_child);
						break;
					case G_THAN_OR_EQ:
						greater_than_or_equal_to_constant_nodes(temp_holder, right_child);
						break;
					case L_THAN_OR_EQ:
						less_than_or_equal_to_constant_nodes(temp_holder, right_child);
						break;
					//Unreachable
					default:
						break;
				}

				//The sub tree root is the temp holder
				sub_tree_root = temp_holder;

				//Get out
				break;
			}

			//Only now do we allocate the operator node, since we know that we've
			//passed all validations
			sub_tree_root = ast_node_alloc(AST_NODE_TYPE_BINARY_EXPR, side);
			//We'll now assign the binary expression it's operator
			sub_tree_root->binary_operator = lookahead.tok;

			//Set the return type as well
			sub_tree_root->inferred_type = return_type;

			//Add these 2 in order now that we're sure it's valid
			add_child_node(sub_tree_root, temp_holder);
			add_child_node(sub_tree_root, right_child);

			break;

		//By default, all we do is scrap this and get out
		default:
			push_back_token(lookahead);
			break;
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
 * BNF Rule: <equality-expression> ::= <relational-expression>{ (==|!=) <relational-expression> }*
 */
static generic_ast_node_t* equality_expression(FILE* fl, side_type_t side){
	//Lookahead token
	lexitem_t lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;
	//Holder for the return type
	generic_type_t* return_type;
	//Flags for whether or not both of these is constant
	u_int8_t temp_holder_is_constant = FALSE;
	u_int8_t right_child_is_constant = FALSE;

	//No matter what, we do need to first see a valid relational expression
	generic_ast_node_t* sub_tree_root = relational_expression(fl, side);

	//Obvious fail case here
	if(sub_tree_root->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		//If this is an error, we can just propogate it up
		return sub_tree_root;
	}
	
	//There are now two options. If we do not see any =='s or !='s, we just add 
	//this node in as the child and move along. But if we do see == or != symbols,
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num);
	
	//As long as we have a relational operators(== or !=) 
	while(lookahead.tok == NOT_EQUALS || lookahead.tok == DOUBLE_EQUALS){
		//Store this locally
		lexitem_t op = lookahead;

		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//Store the flag now
		temp_holder_is_constant = temp_holder->ast_node_type == AST_NODE_TYPE_CONSTANT ? TRUE : FALSE;

		//Let's check to see if this is valid
		u_int8_t is_temp_holder_valid = is_binary_operation_valid_for_type(temp_holder->inferred_type, op.tok, SIDE_TYPE_LEFT);

		//If this fails, there's no point in going forward
		if(is_temp_holder_valid == FALSE){
			sprintf(info, "Type %s is invalid for operator %s", temp_holder->inferred_type->type_name.string, operator_to_string(op.tok));
			return print_and_return_error(info, parser_line_num);
		}

		//Now we have no choice but to see a valid relational expression again
		right_child = relational_expression(fl, side);

		//If it's an error, just fail out
		if(right_child->ast_node_type == AST_NODE_TYPE_ERR_NODE){
			//If this is an error we can just propogate it up
			return right_child;
		}

		//Set the flag now
		right_child_is_constant = right_child->ast_node_type == AST_NODE_TYPE_CONSTANT ? TRUE : FALSE;

		//Let's check to see if this is valid
		u_int8_t is_right_child_valid = is_binary_operation_valid_for_type(right_child->inferred_type, op.tok, SIDE_TYPE_RIGHT);

		//If this fails, there's no point in going forward
		if(is_right_child_valid == FALSE){
			sprintf(info, "Type %s is invalid for operator %s", right_child->inferred_type->type_name.string, operator_to_string(op.tok));
			return print_and_return_error(info, parser_line_num);
		}

		//Get the return type and perform any needed coercions
		return_type = determine_compatibility_and_coerce(type_symtab, &(temp_holder->inferred_type), &(right_child->inferred_type), op.tok);

		//If this fails, that means that we have an invalid operation
		if(sub_tree_root->inferred_type == NULL){
			sprintf(info, "Types %s and %s cannot be applied to operator %s", temp_holder->inferred_type->type_name.string, right_child->inferred_type->type_name.string, operator_to_string(op.tok));
			return print_and_return_error(info, parser_line_num);
		}

		//If either of these are constants, we will invoke the constant coercer here
		if(temp_holder_is_constant == TRUE){
			coerce_constant(temp_holder);
		}

		//Same for the right child
		if(right_child_is_constant == TRUE){
			coerce_constant(right_child);
		}

		//If these are both constants, then we can invoke the appropriate simplifier
		//rule to deal with them
		if(temp_holder_is_constant == TRUE && right_child_is_constant == TRUE){
			//Perform the operation appropriately
			switch(op.tok){
				case NOT_EQUALS:
					not_equals_constant_nodes(temp_holder, right_child);
					break;
				case DOUBLE_EQUALS:
					equals_constant_nodes(temp_holder, right_child);
					break;
				//Should be completely unreachable
				default:
					break;
			}

			//The temp holder is now the subtree-root
			sub_tree_root = temp_holder;

			//Refresh the lookahead and skip ahead
			lookahead = get_next_token(fl, &parser_line_num);
			continue;
		}

		//We now need to make an operator node
		sub_tree_root = ast_node_alloc(AST_NODE_TYPE_BINARY_EXPR, side);
		//We'll now assign the binary expression it's operator
		sub_tree_root->binary_operator = lookahead.tok;

		//Add both child nodes in order only now that we know everything is valid
		add_child_node(sub_tree_root, temp_holder);
		//Otherwise, he is the right child of the sub_tree_root, so we'll add it in
		add_child_node(sub_tree_root, right_child);

		//Store the return type
		sub_tree_root->inferred_type = return_type;

		//By the end of this, we always have a proper subtree with the operator as the root, being held in 
		//"sub-tree root". We'll now refresh the token to keep looking
		lookahead = get_next_token(fl, &parser_line_num);
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
 * BNF Rule: <and-expression> ::= <equality-expression>{& <equality-expression>}* 
 */
static generic_ast_node_t* and_expression(FILE* fl, side_type_t side){
	//Lookahead token
	lexitem_t lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;
	//Store whether both of these are constants or not
	u_int8_t temp_holder_is_constant = FALSE;
	u_int8_t right_child_is_constant = FALSE;

	//No matter what, we do need to first see a valid equality expression
	generic_ast_node_t* sub_tree_root = equality_expression(fl, side);

	//Obvious fail case here
	if(sub_tree_root->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		//If this is an error, we can just propogate it up
		return sub_tree_root;
	}
	
	//There are now two options. If we do not see any ^'s, we just add 
	//this node in as the child and move along. But if we do see ^ symbols, 
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num);
	
	//As long as we have a single and(&) 
	while(lookahead.tok == SINGLE_AND){
		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//If we have a constant here then flag it
		temp_holder_is_constant = temp_holder->ast_node_type == AST_NODE_TYPE_CONSTANT ? TRUE : FALSE;

		//Let's see if this type is valid
		u_int8_t is_temp_holder_valid = is_binary_operation_valid_for_type(temp_holder->inferred_type, SINGLE_AND, SIDE_TYPE_LEFT);

		//This is our fail case
		if(is_temp_holder_valid == FALSE){
			sprintf(info, "Type %s is not valid for the & operator", temp_holder->inferred_type->type_name.string);
			return print_and_return_error(info, parser_line_num);
		}

		//Now we have no choice but to see a valid equality expression again
		right_child = equality_expression(fl, side);

		//If it's an error, just fail out
		if(right_child->ast_node_type == AST_NODE_TYPE_ERR_NODE){
			//If this is an error we can just propogate it up
			return right_child;
		}

		//Let's see if this type is valid
		u_int8_t is_right_child_valid = is_binary_operation_valid_for_type(right_child->inferred_type, SINGLE_AND, SIDE_TYPE_RIGHT);

		//This is our fail case
		if(is_right_child_valid == FALSE){
			sprintf(info, "Type %s is not valid for the & operator", right_child->inferred_type->type_name.string);
			return print_and_return_error(info, parser_line_num);
		}

		//Flag whether or not our right child is a constant
		right_child_is_constant = right_child->ast_node_type == AST_NODE_TYPE_CONSTANT ? TRUE : FALSE;

		//Apply the compatibility and coercion layer
		generic_type_t* final_type = determine_compatibility_and_coerce(type_symtab, &(temp_holder->inferred_type), &(right_child->inferred_type), SINGLE_AND);

		//If this fails, that means that we have an invalid operation
		if(final_type == NULL){
			sprintf(info, "Types %s and %s cannot be applied to operator %s", temp_holder->inferred_type->type_name.string, right_child->inferred_type->type_name.string, "&");
			return print_and_return_error(info, parser_line_num);
		}

		//If the temp holder is a constant, perform coercion
		if(temp_holder_is_constant == TRUE){
			coerce_constant(temp_holder);
		}

		//Same for the right child
		if(right_child_is_constant == TRUE){
			coerce_constant(right_child);
		}

		//If these are both constants, we can invoke the simplifier here
		//to turn them into one node, and skip all of the extra allocation
		if(temp_holder_is_constant == TRUE && right_child_is_constant == TRUE){
			//Invoke the helper. The result will be stored in the temp holder
			bitwise_and_constant_nodes(temp_holder, right_child);

			//This now is the subtree root going forward. The right child is irrelevant
			sub_tree_root = temp_holder;

			//Refresh the lookahead and continue
			lookahead = get_next_token(fl, &parser_line_num);
			continue;
		}

		//We now need to make an operator node
		sub_tree_root = ast_node_alloc(AST_NODE_TYPE_BINARY_EXPR, side);
		sub_tree_root->binary_operator = lookahead.tok;

		//Add the child nodes in the proper order here
		add_child_node(sub_tree_root, temp_holder);
		add_child_node(sub_tree_root, right_child);

		//We now know that the subtree root has a type of u_int8(boolean)
		sub_tree_root->inferred_type = final_type;

		//By the end of this, we always have a proper subtree with the operator as the root, being held in 
		//"sub-tree root". We'll now refresh the token to keep looking
		lookahead = get_next_token(fl, &parser_line_num);
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
 * BNF Rule: <exclusive-or-expression> ::= <and-expression>{^ <and-expression}*
 */
static generic_ast_node_t* exclusive_or_expression(FILE* fl, side_type_t side){
	//Lookahead token
	lexitem_t lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;
	//Flags for whether or not our right child and holder are constants
	u_int8_t temp_holder_is_constant = FALSE;
	u_int8_t right_child_is_constant = FALSE;

	//No matter what, we do need to first see a valid and expression
	generic_ast_node_t* sub_tree_root = and_expression(fl, side);

	//Obvious fail case here
	if(sub_tree_root->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		//If this is an error, we can just propogate it up
		return sub_tree_root;
	}
	
	//There are now two options. If we do not see any ^'s, we just add 
	//this node in as the child and move along. But if we do see ^ symbols, 
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num);
	
	//As long as we have a single xor(^)
	while(lookahead.tok == CARROT){
		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//Flag whether we have a constant
		temp_holder_is_constant = temp_holder->ast_node_type == AST_NODE_TYPE_CONSTANT ? TRUE : FALSE;

		//Let's see if this type is valid
		u_int8_t is_temp_holder_valid = is_binary_operation_valid_for_type(temp_holder->inferred_type, CARROT, SIDE_TYPE_LEFT);

		//This is our fail case
		if(is_temp_holder_valid == FALSE){
			sprintf(info, "Type %s is not valid for the ^ operator", temp_holder->inferred_type->type_name.string);
			return print_and_return_error(info, parser_line_num);
		}

		//Now we have no choice but to see a valid and expression again
		right_child = and_expression(fl, side);

		//If it's an error, just fail out
		if(right_child->ast_node_type == AST_NODE_TYPE_ERR_NODE){
			//If this is an error we can just propogate it up
			return right_child;
		}

		//Let's see if this type is valid
		u_int8_t is_right_child_valid = is_binary_operation_valid_for_type(right_child->inferred_type, CARROT, SIDE_TYPE_RIGHT);

		//This is our fail case
		if(is_right_child_valid == FALSE){
			sprintf(info, "Type %s is not valid for the | operator", right_child->inferred_type->type_name.string);
			return print_and_return_error(info, parser_line_num);
		}

		//Flag whether or not the right child is contant
		right_child_is_constant = right_child->ast_node_type == AST_NODE_TYPE_CONSTANT ? TRUE : FALSE;

		//Apply the compatibility and coercion layer
		generic_type_t* final_type = determine_compatibility_and_coerce(type_symtab, &(temp_holder->inferred_type), &(right_child->inferred_type), CARROT);

		//If this fails, that means that we have an invalid operation
		if(final_type == NULL){
			sprintf(info, "Types %s and %s cannot be applied to operator %s", temp_holder->inferred_type->type_name.string, right_child->inferred_type->type_name.string, "^");
			return print_and_return_error(info, parser_line_num);
		}

		//If it's constant, invoke the constant coercer
		if(temp_holder_is_constant == TRUE){
			coerce_constant(temp_holder);
		}

		//Same for the right child
		if(right_child_is_constant == TRUE){
			coerce_constant(right_child);
		}

		//If they are both constants, then we should invoke the helper to do this on the spot
		//and avoid the allocation of any new node altogether
		if(temp_holder_is_constant == TRUE && right_child_is_constant == TRUE){
			//Bitwise xor them - the result is in the constant node itself
			bitwise_exclusive_or_constant_nodes(temp_holder, right_child);

			//This is the root now
			sub_tree_root = temp_holder;

			//And push ahead
			lookahead = get_next_token(fl, &parser_line_num);
			continue;
		}

		//We now need to make an operator node
		sub_tree_root = ast_node_alloc(AST_NODE_TYPE_BINARY_EXPR, side);
		//We'll now assign the binary expression it's operator
		sub_tree_root->binary_operator = lookahead.tok;

		//Add both children in order now that everything is valid
		add_child_node(sub_tree_root, temp_holder);
		add_child_node(sub_tree_root, right_child);

		//Store the final type
		sub_tree_root->inferred_type = final_type;

		//By the end of this, we always have a proper subtree with the operator as the root, being held in 
		//"sub-tree root". We'll now refresh the token to keep looking
		lookahead = get_next_token(fl, &parser_line_num);
	}

	//If we get here, it means that we did not see the "SINGLE_AND" token, so we are done. We'll put
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
 * BNF rule: <inclusive-or-expression> ::= <exclusive-or-expression>{ | <exclusive-or-expression>}*
 */
static generic_ast_node_t* inclusive_or_expression(FILE* fl, side_type_t side){
	//Lookahead token
	lexitem_t lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;
	//Are either of these constants? We'll have flags to avoid repeated
	//comparison
	u_int8_t is_temp_holder_constant = FALSE;
	u_int8_t is_right_child_constant = FALSE;

	//No matter what, we do need to first see a valid exclusive or expression
	generic_ast_node_t* sub_tree_root = exclusive_or_expression(fl, side);

	//Obvious fail case here
	if(sub_tree_root->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		//If this is an error, we can just propogate it up
		return sub_tree_root;
	}
	
	//There are now two options. If we do not see any |'s, we just add 
	//this node in as the child and move along. But if we do see | symbols, 
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num);
	
	//As long as we have a single or(|)
	while(lookahead.tok == SINGLE_OR){
		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//Store the constant status now
		is_temp_holder_constant = temp_holder->ast_node_type == AST_NODE_TYPE_CONSTANT ? TRUE : FALSE;

		//Let's see if this type is valid
		u_int8_t is_temp_holder_valid = is_binary_operation_valid_for_type(temp_holder->inferred_type, SINGLE_OR, SIDE_TYPE_LEFT);

		//This is our fail case
		if(is_temp_holder_valid == FALSE){
			sprintf(info, "Type %s is not valid for the | operator", temp_holder->inferred_type->type_name.string);
			return print_and_return_error(info, parser_line_num);
		}

		//Now we have no choice but to see a valid exclusive or expression again
		right_child = exclusive_or_expression(fl, side);

		//If it's an error, just fail out
		if(right_child->ast_node_type == AST_NODE_TYPE_ERR_NODE){
			//If this is an error we can just propogate it up
			return right_child;
		}

		//Let's see if this type is valid
		u_int8_t is_right_child_valid = is_binary_operation_valid_for_type(right_child->inferred_type, SINGLE_OR, SIDE_TYPE_RIGHT);

		//This is our fail case
		if(is_right_child_valid == FALSE){
			sprintf(info, "Type %s is not valid for the | operator", right_child->inferred_type->type_name.string);
			return print_and_return_error(info, parser_line_num);
		}

		//Store whether or not the right child is a constant
		is_right_child_constant = right_child->ast_node_type == AST_NODE_TYPE_CONSTANT ? TRUE : FALSE;

		//Apply the compatibility and coercion layer
		generic_type_t* final_type = determine_compatibility_and_coerce(type_symtab, &(temp_holder->inferred_type), &(right_child->inferred_type), CARROT);

		//If this fails, that means that we have an invalid operation
		if(final_type == NULL){
			sprintf(info, "Types %s and %s cannot be applied to operator %s", temp_holder->inferred_type->type_name.string, right_child->inferred_type->type_name.string, "^");
			return print_and_return_error(info, parser_line_num);
		}

		//Once we have gone through type coercion - if we have constants here we will coerce them
		//now
		if(is_temp_holder_constant == TRUE){
			coerce_constant(temp_holder);
		}

		//Same for the right child
		if(is_right_child_constant == TRUE){
			coerce_constant(right_child);
		}

		//If these are both constants, we can take a shortcut and do the bitwise or
		//right here
		if(is_temp_holder_constant == TRUE && is_right_child_constant == TRUE){
			//Invoke the helper to do this, the result is stored in the temp_holder
			bitwise_or_constant_nodes(temp_holder, right_child);

			//This is the root now
			sub_tree_root = temp_holder;

			//And push ahead
			lookahead = get_next_token(fl, &parser_line_num);
			continue;
		}

		//We now need to make an operator node
		sub_tree_root = ast_node_alloc(AST_NODE_TYPE_BINARY_EXPR, side);
		//We'll now assign the binary expression it's operator
		sub_tree_root->binary_operator = lookahead.tok;

		//Now we add the 2 children in order
		add_child_node(sub_tree_root, temp_holder);
		add_child_node(sub_tree_root, right_child);

		//Store the final type
		sub_tree_root->inferred_type = final_type;

		//By the end of this, we always have a proper subtree with the operator as the root, being held in 
		//"sub-tree root". We'll now refresh the token to keep looking
		lookahead = get_next_token(fl, &parser_line_num);
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
static generic_ast_node_t* logical_and_expression(FILE* fl, side_type_t side){
	//Lookahead token
	lexitem_t lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;
	//Store these flags - we will populate them as we
	//go through to see if we have constants. If we do,
	//then some peemptive optimizations can happen
	u_int8_t temp_holder_is_constant = FALSE;
	u_int8_t right_child_is_constant = FALSE;

	//No matter what, we do need to first see a valid inclusive or expression
	generic_ast_node_t* sub_tree_root = inclusive_or_expression(fl, side);

	//Obvious fail case here
	if(sub_tree_root->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		//If this is an error, we can just propogate it up
		return sub_tree_root;
	}
	
	//There are now two options. If we do not see any &&'s, we just add 
	//this node in as the child and move along. But if we do see && symbols, 
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num);

	//As long as we have a double and 
	while(lookahead.tok == DOUBLE_AND){
		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//Get this out now
		temp_holder_is_constant = temp_holder->ast_node_type == AST_NODE_TYPE_CONSTANT ? TRUE : FALSE;

		//Let's see if this type is valid
		u_int8_t is_temp_holder_valid = is_binary_operation_valid_for_type(temp_holder->inferred_type, DOUBLE_AND, SIDE_TYPE_LEFT);

		//This is our fail case
		if(is_temp_holder_valid == FALSE){
			sprintf(info, "Type %s is not valid for the && operator", temp_holder->inferred_type->type_name.string);
			return print_and_return_error(info, parser_line_num);
		}

		//Now we have no choice but to see a valid inclusive or expression again
		right_child = inclusive_or_expression(fl, side);

		//If it's an error, just fail out
		if(right_child->ast_node_type == AST_NODE_TYPE_ERR_NODE){
			//If this is an error we can just propogate it up
			return right_child;
		}

		//Let's see if this type is valid
		u_int8_t is_right_child_valid = is_binary_operation_valid_for_type(right_child->inferred_type, DOUBLE_AND, SIDE_TYPE_RIGHT);

		//This is our fail case
		if(is_right_child_valid == FALSE){
			sprintf(info, "Type %s is not valid for the && operator", right_child->inferred_type->type_name.string);
			return print_and_return_error(info, parser_line_num);
		}

		//Now that we know it's valid, store whether or not it's a constant
		right_child_is_constant = right_child->ast_node_type == AST_NODE_TYPE_CONSTANT ? TRUE : FALSE;

		//Use the type compatibility function to determine compatibility and apply necessary coercions
		generic_type_t* return_type = determine_compatibility_and_coerce(type_symtab, &(temp_holder->inferred_type), &(right_child->inferred_type), DOUBLE_AND);

		//If this fails, that means that we have an invalid operation
		if(return_type == NULL){
			sprintf(info, "Types %s and %s cannot be applied to operator %s", temp_holder->inferred_type->type_name.string, right_child->inferred_type->type_name.string, "&&");
			return print_and_return_error(info, parser_line_num);
		}
		
		//Perform the coercions now if they're constants
		if(temp_holder_is_constant == TRUE){
			coerce_constant(temp_holder);
		}

		//Same for the right child
		if(right_child_is_constant == TRUE){
			coerce_constant(right_child);
		}

		/**
		 * We will not do any simplification here immediately because the user may still
		 * want part of their logical and expression to execute. We will only do simplification
		 * here if both areas are constants
		 */
		//If we have 2 constants we should just do this right now. The result will be in the temp holder,
		//and the right child will not be used
		if(temp_holder_is_constant == TRUE && right_child_is_constant == TRUE){
			//Get these values out now
			u_int8_t temp_holder_0 = is_constant_node_value_0(temp_holder);
			u_int8_t right_child_0 = is_constant_node_value_0(right_child);

			//All that we need to do is this
			temp_holder->constant_value.unsigned_long_value = !temp_holder_0 && !right_child_0;

			//The new root is just the temp holder
			sub_tree_root = temp_holder;
			
			//And push ahead
			lookahead = get_next_token(fl, &parser_line_num);
			continue;
		}

		//We now need to make an operator node
		sub_tree_root = ast_node_alloc(AST_NODE_TYPE_BINARY_EXPR, side);
		//We'll now assign the binary expression it's operator
		sub_tree_root->binary_operator = lookahead.tok;

		//Add the 2 children in order - now that they're both known to be valid
		add_child_node(sub_tree_root, temp_holder);
		add_child_node(sub_tree_root, right_child);

		//Give this to the root
		sub_tree_root->inferred_type = return_type;

		//By the end of this, we always have a proper subtree with the operator as the root, being held in 
		//"sub-tree root". We'll now refresh the token to keep looking
		lookahead = get_next_token(fl, &parser_line_num);
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
 * This function will also attempt to do some preemptive optimization. For example, if the user does something
 * like x || 3 , the result should be 1(true) off of the bat here
 *
 * BNF Rule: <logical-or-expression> ::= <logical-and-expression>{||<logical-and-expression>}*
 */
static generic_ast_node_t* logical_or_expression(FILE* fl, side_type_t side){
	//Lookahead token
	lexitem_t lookahead;
	//Temp holder for our use
	generic_ast_node_t* temp_holder;
	//For holding the right child
	generic_ast_node_t* right_child;
	//Store these flags - we will populate them as we
	//go through to see if we have constants. If we do,
	//then some peemptive optimizations can happen
	u_int8_t temp_holder_is_constant = FALSE;
	u_int8_t right_child_is_constant = FALSE;

	//No matter what, we do need to first see a logical and expression
	generic_ast_node_t* sub_tree_root = logical_and_expression(fl, side);

	//Obvious fail case here
	if(sub_tree_root->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		//It's already an error node, so allow it to propogate
		return sub_tree_root;
	}
	
	//There are now two options. If we do not see any ||'s, we just add 
	//this node in as the child and move along. But if we do see || symbols, 
	//we will on the fly construct a subtree here
	lookahead = get_next_token(fl, &parser_line_num);
	
	//As long as we have a double or
	while(lookahead.tok == DOUBLE_OR){
		//Hold the reference to the prior root
		temp_holder = sub_tree_root;

		//Store whether it is a constant or not
		temp_holder_is_constant = temp_holder->ast_node_type == AST_NODE_TYPE_CONSTANT ? TRUE : FALSE;

		//Let's see if this type is valid
		u_int8_t is_temp_holder_valid = is_binary_operation_valid_for_type(temp_holder->inferred_type, DOUBLE_OR, SIDE_TYPE_LEFT);

		//This is our fail case
		if(is_temp_holder_valid == FALSE){
			sprintf(info, "Type %s is not valid for the || operator", temp_holder->inferred_type->type_name.string);
			return print_and_return_error(info, parser_line_num);
		}

		//Now we have no choice but to see a valid logical and expression again
		right_child = logical_and_expression(fl, side);

		//If it's an error, just fail out
		if(right_child->ast_node_type == AST_NODE_TYPE_ERR_NODE){
			//It's already an error node, so allow it to propogate
			return right_child;
		}

		//Store whether or not the right child is a constant
		right_child_is_constant = right_child->ast_node_type == AST_NODE_TYPE_CONSTANT ? TRUE : FALSE;

		//Let's see if this type is valid
		u_int8_t is_right_child_valid = is_binary_operation_valid_for_type(right_child->inferred_type, DOUBLE_AND, SIDE_TYPE_RIGHT);

		//This is our fail case
		if(is_right_child_valid == FALSE){
			sprintf(info, "Type %s is not valid for the && operator", right_child->inferred_type->type_name.string);
			return print_and_return_error(info, parser_line_num);
		}

		//Use the type compatibility function to determine compatibility and apply necessary coercions
		generic_type_t* return_type = determine_compatibility_and_coerce(type_symtab, &(temp_holder->inferred_type), &(right_child->inferred_type), DOUBLE_OR);

		//If this fails, that means that we have an invalid operation
		if(return_type == NULL){
			sprintf(info, "Types %s and %s cannot be applied to operator %s", temp_holder->inferred_type->type_name.string, right_child->inferred_type->type_name.string, "||");
			return print_and_return_error(info, parser_line_num);
		}

		//Perform any needed constant coercion
		if(temp_holder_is_constant == TRUE){
			coerce_constant(temp_holder);
		}

		//Same for the right child
		if(right_child_is_constant == TRUE){
			coerce_constant(right_child);
		}

		/**
		 * We will not do any simplification here immediately because the user may still
		 * want part of their logical or expression to execute. We will only do simplification
		 * here if both areas are constants
		 */
		//If we have 2 constants we should just do this right now. The result will be in the temp holder,
		//and the right child will not be used
		if(temp_holder_is_constant == TRUE && right_child_is_constant == TRUE){
			//Get these values out now
			u_int8_t temp_holder_0 = is_constant_node_value_0(temp_holder);
			u_int8_t right_child_0 = is_constant_node_value_0(right_child);

			//All that we need to do is this
			temp_holder->constant_value.unsigned_long_value = !temp_holder_0 || !right_child_0;

			//The new root is just the temp holder
			sub_tree_root = temp_holder;
			
			//And push ahead
			lookahead = get_next_token(fl, &parser_line_num);
			continue;
		}

		//We now need to make an operator node
		sub_tree_root = ast_node_alloc(AST_NODE_TYPE_BINARY_EXPR, side);
		//We'll now assign the binary expression it's operator
		sub_tree_root->binary_operator = lookahead.tok;

		//Add the children in order
		add_child_node(sub_tree_root, temp_holder);
		add_child_node(sub_tree_root, right_child);

		//We now know that the subtree root has a type of u_int8(boolean)
		sub_tree_root->inferred_type = return_type;

		//By the end of this, we always have a proper subtree with the operator as the root, being held in 
		//"sub-tree root". We'll now refresh the token to keep looking
		lookahead = get_next_token(fl, &parser_line_num);
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
 * An array initializer is a set of one or more initializers in between
 * [], separated by commas
 *
 * BNF Rule: <array-initializer> ::= [<intializer>{, <initializer>}*]
 *
 * REMEMBER: by the time that we've arrived here, we've already seen and consumed the
 * first [ token
 */
static generic_ast_node_t* array_initializer(FILE* fl, side_type_t side){
	//Lookahead token for parsing
	lexitem_t lookahead;

	//Let's first allocate our initializer node. The initializer node will store
	//all of our ternary expressions inside of it as children
	generic_ast_node_t* initializer_list_node = ast_node_alloc(AST_NODE_TYPE_ARRAY_INITIALIZER_LIST, side);

	//Store the line number
	initializer_list_node->line_number = parser_line_num;

	//We are required to see at least one initializer inside of here. As such, we'll use a do-while loop
	//to process
	do{
		//We now must see an initializer node
		generic_ast_node_t* initializer_node = initializer(fl, side);

		//If this is an error, then the whole thing is invalid
		if(initializer_node->ast_node_type == AST_NODE_TYPE_ERR_NODE){
			return print_and_return_error("Invalid initializer given in array initializer", parser_line_num);
		}

		//Add this in as a child of the initializer list
		add_child_node(initializer_list_node, initializer_node);

		//Refresh the lookahead
		lookahead = get_next_token(fl, &parser_line_num);

	//So long as we keep seeing commas, we continue
	} while(lookahead.tok == COMMA);

	//Once we reach down here, we need to check and see if we have the closing bracket that would
	//mark a valid end for us
	if(lookahead.tok != R_BRACKET){
		return print_and_return_error("Closing bracket(]) required at the end of array initializer", parser_line_num);
	}

	//Pop the grouping stack and ensure it matches
	if(pop_token(&grouping_stack).tok != L_BRACKET){
		return print_and_return_error("Unmatched brackets detected in array initializer", parser_line_num);
	}

	//Give back the intializer list node
	return initializer_list_node;
}


/**
 * A struct initializer is a set of one or more initializers in between
 * [], separated by commas
 *
 * BNF Rule: <struct-initializer> ::= {<intializer>{, <initializer>}*}
 */
static generic_ast_node_t* struct_initializer(FILE* fl, side_type_t side){
	//Lookahead token for parsing
	lexitem_t lookahead;

	//Let's first allocate our initializer node. The initializer node will store
	//all of our ternary expressions inside of it as children
	generic_ast_node_t* initializer_list_node = ast_node_alloc(AST_NODE_TYPE_STRUCT_INITIALIZER_LIST, side);

	//Store the line number
	initializer_list_node->line_number = parser_line_num;

	//We are required to see at least one initializer inside of here. As such, we'll use a do-while loop
	//to process
	do{
		//We now must see an initializer node
		generic_ast_node_t* initializer_node = initializer(fl, side);

		//If this is an error, then the whole thing is invalid
		if(initializer_node->ast_node_type == AST_NODE_TYPE_ERR_NODE){
			return print_and_return_error("Invalid initializer given in struct initializer", parser_line_num);
		}

		//Add this in as a child of the initializer list
		add_child_node(initializer_list_node, initializer_node);

		//Refresh the lookahead
		lookahead = get_next_token(fl, &parser_line_num);

	//So long as we keep seeing commas, we continue
	} while(lookahead.tok == COMMA);

	//Once we reach down here, we need to check and see if we have the closing bracket that would
	//mark a valid end for us
	if(lookahead.tok != R_CURLY){
		return print_and_return_error("Closing curly brace(}) required at the end of struct initializer", parser_line_num);
	}

	//Pop the grouping stack and ensure it matches
	if(pop_token(&grouping_stack).tok != L_CURLY){
		return print_and_return_error("Unmatched brackets detected in struct initializer", parser_line_num);
	}

	//Give back the intializer list node
	return initializer_list_node;
}


/**
 * An initializer can either decay into an expression chain or it can turn into an initializer of
 * some kind(string or list)
 *
 * BNF Rule: <initializer> ::= <ternary_expression> | <initializer_list>
 */
static generic_ast_node_t* initializer(FILE* fl, side_type_t side){
	//Grab the next token
	lexitem_t lookahead = get_next_token(fl, &parser_line_num);
	
	switch(lookahead.tok){
		//A left bracket symbol means that we're encountering an array initializer
		case L_BRACKET:
			//Push this onto the grouping stack
			push_token(&grouping_stack, lookahead);

			//Let the helper handle it
			return array_initializer(fl, side);

		//An L_CURLY signifies the start of a struct initializer
		case L_CURLY:
			//Push this onto the grouping stack for matching later
			push_token(&grouping_stack, lookahead);

			//Let the helper handle it
			return struct_initializer(fl, side);

		//By default, we haven't found anything in here that would indicate we'll need an initializer.
		//As such, we'll push the token back and call the ternary expression rule
		default:
			push_back_token(lookahead);
			return ternary_expression(fl, side);
	}
}


/**
 * A ternary expression is a kind of syntactic sugar that allows if/else chains to be
 * inlined. They can be nested, though this is not recommended
 *
 * BNF Rule: <logical_or_expression> ? <ternary_expression> else <ternary_expression>
 */
static generic_ast_node_t* ternary_expression(FILE* fl, side_type_t side){
	//Declare the lookahead token
	lexitem_t lookahead;

	//We are first required to see a valid logical or expression. If we don't see this,
	//then we fail
	generic_ast_node_t* conditional = logical_or_expression(fl, side);

	//If this is an error, then the whole thing is over - we're done here
	if(conditional->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		return conditional;
	}

	//Let's now see what comes after this ternary expression
	lookahead = get_next_token(fl, &parser_line_num);

	//If this is not a question mark, then we are done here, and we should push this token
	//back and return the conditional
	if(lookahead.tok != QUESTION){
		push_back_token(lookahead);

		//We'll just give back whatever this was
		return conditional;
	}

	//Otherwise if we make it here, then we know that we are seeing a ternary expression.
	
	//If it's not of this type or a compatible type(pointer, smaller int, etc, it is out)
	if(is_type_valid_for_conditional(conditional->inferred_type) == FALSE){
		sprintf(info, "Type %s is invalid to be used in a conditional", conditional->inferred_type->type_name.string);
		return print_and_return_error(info, parser_line_num);
	}

	//Allocate the ternary expression node
	generic_ast_node_t* ternary_expression_node = ast_node_alloc(AST_NODE_TYPE_TERNARY_EXPRESSION, side);

	//The first child is the conditional
	add_child_node(ternary_expression_node, conditional);

	//We must now see a valid top level expression
	generic_ast_node_t* if_branch = ternary_expression(fl, side);

	//If this is invalid, then we bail out
	if(if_branch->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		return print_and_return_error("Invalid if branch given in ternary operator", parser_line_num);
	}

	//Otherwise it's fine so we add it and move on
	add_child_node(ternary_expression_node, if_branch);

	//Once we've seen the if branch, we need to see the colon to separate the else branch
	lookahead = get_next_token(fl, &parser_line_num);

	//If we don't see an else, we have a failure here
	if(lookahead.tok != ELSE){
		return print_and_return_error("else expected between branches in ternary operator", parser_line_num);
	}
	
	//We now must see another valid logical or expression 
	generic_ast_node_t* else_branch = ternary_expression(fl, side);

	//If this is invalid, then we bail out
	if(else_branch->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		return print_and_return_error("Invalid else branch given in ternary operator", parser_line_num);
	}

	//Otherwise it's fine so we add it and move on
	add_child_node(ternary_expression_node, else_branch);

	//Determine the compatibility of these ternary nodes, and coerce it
	ternary_expression_node->inferred_type = determine_compatibility_and_coerce(type_symtab, &(if_branch->inferred_type), &(else_branch->inferred_type), QUESTION);

	//A ternary is not assignable
	ternary_expression_node->is_assignable = FALSE;

	//Give back the parent level node
	return ternary_expression_node;
}


/**
 * A construct member declares a variable within a struct. Structs have their own tables
 * that store variables within. Because these variables are unique to structs, we don't need to
 * do any validation on duplicates. All references to these variables will be by default
 * unambiguous
 *
 * As a reminder, type specifier will give us an error if the type is not defined
 *
 * BNF Rule: <construct-member> ::= <identifier> : <type-specifier> 
 */
static u_int8_t struct_member(FILE* fl, generic_type_t* mutable_struct_type, generic_type_t* immutable_struct_type){
	//The lookahead token
	lexitem_t lookahead;

	//Get the first token
	lookahead = get_next_token(fl, &parser_line_num);

	//Let's make sure it actually worked
	if(lookahead.tok != IDENT){
		print_parse_message(PARSE_ERROR, "Invalid identifier given as struct member name", parser_line_num);
		num_errors++;
		//It's an error, so we'll propogate it up
		return FAILURE;
	}

	//Grab this for convenience
	dynamic_string_t name = lookahead.lexeme;

	//The field, if we can find it. We only need to check it from one of the versions, they
	//are the same internally
	symtab_variable_record_t* duplicate = get_struct_member(mutable_struct_type, name.string);

	//Is this a duplicate? If so, we fail out
	if(duplicate != NULL){
		sprintf(info, "A member with name %s already exists in type %s. First defined here:", name.string, mutable_struct_type->type_name.string);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		print_variable_name(duplicate);
		num_errors++;
		return FAILURE;
	}

	//Are we defining a duplicated type?
	if(do_duplicate_types_exist(name.string) == TRUE){
		return FAILURE;
	}

	//Look for duplicated functions too
	if(do_duplicate_functions_exist(name.string) == TRUE){
		return FAILURE;
	}

	//After the ident, we need to see a colon
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail out here
	if(lookahead.tok != COLON){
		print_parse_message(PARSE_ERROR, "Colon required between ident and type specifier in struct member declaration", parser_line_num);
		num_errors++;
		//Error out
		return FAILURE;
	}

	//Now we are required to see a valid type specifier
	generic_type_t* type_spec = type_specifier(fl);

	//If this is an error, the whole thing fails
	if(type_spec == NULL){
		print_parse_message(PARSE_ERROR, "Attempt to use undefined type in struct member", parser_line_num);
		num_errors++;
		//It's already an error, so just send it up
		return FAILURE;
	}

	//Error out if this happens
	if(type_spec == immut_void){
		print_parse_message(PARSE_ERROR, "Struct members may not be typed as void", parser_line_num);
		num_errors++;
		return FAILURE;;
	}

	//Add extra validation to ensure that the size of said type is known at comptime. This will stop
	//the user from adding a field the mut a:char[] that is unknown at compile time
	if(type_spec->type_complete == FALSE){
		sprintf(info, "Attempt to use incomplete type %s as a struct member. Struct members must have a size known at compile time", type_spec->type_name.string);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		return FAILURE;
	}

	//Now if we finally make it all of the way down here, we are actually set. We'll construct the
	//node that we have and also add it into our symbol table
	
	//We'll first create the symtab record
	symtab_variable_record_t* member_record = create_variable_record(name);
	//Store the line number for error printing
	member_record->line_number = parser_line_num;
	//Store what the type is
	member_record->type_defined_as = type_spec;

	//Add it to both versions
	add_struct_member(mutable_struct_type, member_record);
	add_struct_member(immutable_struct_type, member_record);

	//All went well so we can send this up the chain
	return SUCCESS;
}


/**
 * A construct member list holds all of the nodes that themselves represent construct members. Like all
 * other rules, this function returns the root node of the subtree that it creates
 *
 * BNF Rule: <construct-member-list> ::= { <construct-member> ; }*
 */
static u_int8_t struct_member_list(FILE* fl, generic_type_t* mutable_struct_type, generic_type_t* immutable_struct_type){
	//Now we are required to see a curly brace
	lexitem_t lookahead = get_next_token(fl, &parser_line_num);

	//Fail case here
	if(lookahead.tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Unelaborated struct definition is not supported", parser_line_num);
		num_errors++;
		//Fail out
		return FAILURE;
	}

	//Otherwise we'll push onto the stack for later matching
	push_token(&grouping_stack, lookahead);

	//This is just to seed our search
	lookahead = get_next_token(fl, &parser_line_num);

	//We can see as many construct members as we please here, all delimited by semicols
	do{
		//Put what we saw back
		push_back_token(lookahead);

		//We must first see a valid construct member
		u_int8_t status = struct_member(fl, mutable_struct_type, immutable_struct_type);

		//If it's an error, we'll fail right out
		if(status == FAILURE){
			print_parse_message(PARSE_ERROR, "Invalid struct member declaration", parser_line_num);
			num_errors++;
			//It's already an error node so just let it propogate
			return FAILURE;
		}
		
		//Now we will refresh the lookahead
		lookahead = get_next_token(fl, &parser_line_num);

		//We must now see a valid semicolon
		if(lookahead.tok != SEMICOLON){
			print_parse_message(PARSE_ERROR, "Struct members must be delimited by ;", parser_line_num);
			num_errors++;
			return FAILURE;
		}

		//Refresh it once more
		lookahead = get_next_token(fl, &parser_line_num);

	//So long as we don't see the end
	} while (lookahead.tok != R_CURLY);

	//Check for unamtched curlies
	if(pop_token(&grouping_stack).tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Unmatched curly braces in struct definition", parser_line_num);
		num_errors++;
		//Fail out here
		return FAILURE;
	}

	//Once done, we need to finalize the alignment for the construct table
	finalize_struct_alignment(mutable_struct_type);
	finalize_struct_alignment(immutable_struct_type);

	//Give the member list back
	return SUCCESS;
}


/**
 * A function pointer definer defines a function signature that can be used to dynamically call functions 
 * of the same signature
 *
 * define fn(<parameter_list>) -> {mut}? <type> as <identifier>;
 *
 * Unlike constructs & enums, we'll force the user to use an as keyword here for their type definition to
 * enforce readability
 *
 * NOTE: We've already seen the "define" and "fn" keyword by the time that we arrive here
 */
static u_int8_t function_pointer_definer(FILE* fl){
	//Declare a token for search-ahead
	lexitem_t lookahead = get_next_token(fl, &parser_line_num);

	//Now we need to see an L_PAREN
	if(lookahead.tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Left parenthesis required after fn keyword", parser_line_num);
	}

	//Otherwise push this onto the grouping stack for later
	push_token(&grouping_stack, lookahead);

	//Once we've gotten past this point, we're safe to allocate this type. Function
	//pointers are always private
	generic_type_t* mutable_function_type = create_function_pointer_type(FALSE, parser_line_num, MUTABLE);
	generic_type_t* immutable_function_type = create_function_pointer_type(FALSE, parser_line_num, NOT_MUTABLE);

	//Let's see if we have nothing in here. This is possible. We can also just see a "void"
	//as an alternative way of saying this function takes no parameters
	
	//Grab the next token
	lookahead = get_next_token(fl, &parser_line_num);

	//We can optionally see a void type that we need to consume
	switch(lookahead.tok){
		//We just need to consume this and move along
		case VOID:
			//Refresh the token
			lookahead = get_next_token(fl, &parser_line_num);
			break;

		default:
			//If we hit the default, then we need to push the token back
			push_back_token(lookahead);
			break;
	}

	//Keep processing so long as we keep seeing commas
	do{
		//Now we need to see a valid type
		generic_type_t* type = type_specifier(fl);

		//If this is NULL, we'll error out
		if(type == NULL){
			return FALSE;
		}

		//Add it to the mutable version
		u_int8_t status = add_parameter_to_function_type(mutable_function_type, type);

		//This means that we have been given too many parameters
		if(status == FAILURE){
			print_parse_message(PARSE_ERROR, "Maximum function parameter count of 6 exceeded", parser_line_num);
			return FALSE;
		}

		//Let's also add it to the immutable version
		status = add_parameter_to_function_type(immutable_function_type, type);

		//This means that we have been given too many parameters
		if(status == FAILURE){
			print_parse_message(PARSE_ERROR, "Maximum function parameter count of 6 exceeded", parser_line_num);
			return FALSE;
		}

		//Refresh the lookahead token
		lookahead = get_next_token(fl, &parser_line_num);

	} while(lookahead.tok == COMMA);

	//Now that we're done processing the list, we need to ensure that we have a right paren
	if(lookahead.tok != R_PAREN){
		//Fail out
		print_parse_message(PARSE_ERROR, "Right parenthesis required after parameter list declaration", parser_line_num);
		num_errors++;
		return FALSE;
	}

	//Ensure that we pop the grouping stack and get a match
	if(pop_token(&grouping_stack).tok != L_PAREN){
		//Fail out
		print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected in parameter list declaration", parser_line_num);
		num_errors++;
		return FALSE;
	}

	//Now we need to see an arrow operator
	lookahead = get_next_token(fl, &parser_line_num);

	//If we don't see it, we fail out
	if(lookahead.tok != ARROW){
		//Fail out
		print_parse_message(PARSE_ERROR, "Arrow (->) required after function parameter list", parser_line_num);
		num_errors++;
		return FALSE;
	}

	//Now we need to see a return type
	generic_type_t* return_type = type_specifier(fl);

	//If this is NULL, then we have an invalid return type
	if(return_type == NULL){
		print_parse_message(PARSE_ERROR, "Invalid return type given in function type definition", parser_line_num);
		num_errors++;
		return FALSE;
	}

	//Let's now store the return type
	mutable_function_type->internal_types.function_type->return_type = return_type;
	immutable_function_type->internal_types.function_type->return_type = return_type;

	//Mark whether or not it's void as well
	mutable_function_type->internal_types.function_type->returns_void = IS_VOID_TYPE(return_type);
	immutable_function_type->internal_types.function_type->returns_void = IS_VOID_TYPE(return_type);

	//Otherwise this did work, so now we need to see the AS keyword. Ollie forces the user to use AS to avoid the
	//confusing syntactical mess that C function pointer declarations have
	
	//Refresh the token
	lookahead = get_next_token(fl, &parser_line_num);

	//If it isn't an AS keyword, we're done
	if(lookahead.tok != AS){
		print_parse_message(PARSE_ERROR, "\"as\" keyword is required after function type definition", parser_line_num);
		num_errors++;
		return FALSE;
	}


	//If we make it here then we know we're good to look for an identifier
	lookahead = get_next_token(fl, &parser_line_num);

	//If this is an error, then we're going to fail out
	if(lookahead.tok != IDENT){
		print_parse_message(PARSE_ERROR, "Invalid identifier given as alias type", parser_line_num);
		num_errors++;
		return FALSE;
	}

	//We know that it wasn't an error, but now we need to perform duplicate checking

	//Grab this out for convenience
	dynamic_string_t identifier_name = lookahead.lexeme;

	//Let's close the parsing out here - we'll need to see & consume a semicolon
	lookahead = get_next_token(fl, &parser_line_num);

	//If we didn't see it, then we fail out
	if(lookahead.tok != SEMICOLON){
		print_parse_message(PARSE_ERROR, "Semicolon required after definition statement", parser_line_num);
		num_errors++;
		return FALSE;
	}

	//Check for function name duplications
	if(do_duplicate_functions_exist(identifier_name.string) == TRUE){
		return FALSE;
	}

	//Check for duplicate variables
	if(do_duplicate_variables_exist(identifier_name.string) == TRUE){
		return FALSE;
	}

	//Finally check for type duplication
	if(do_duplicate_types_exist(identifier_name.string) == TRUE){
		return FALSE;
	}

	//Generate the names for both of our versions
	generate_function_pointer_type_name(mutable_function_type);
	generate_function_pointer_type_name(immutable_function_type);

	//Now that we've created it, we'll store it in the symtab
	symtab_type_record_t* mutable_record = create_type_record(mutable_function_type);
	symtab_type_record_t* immutable_record = create_type_record(immutable_function_type);

	//Insert both of these in
	insert_type(type_symtab, mutable_record);
	insert_type(type_symtab, immutable_record);

	//Now that we've done that part, we also need to create the mutable and immutable aliases for the type
	generic_type_t* mutable_alias_type = create_aliased_type(identifier_name.string, mutable_function_type, parser_line_num, MUTABLE);

	//Once we've created this, we'll add this into the symtab
	insert_type(type_symtab, create_type_record(mutable_alias_type));

	//Now that we've done that part, we also need to create the mutable and immutable aliases for the type
	generic_type_t* immutable_alias_type = create_aliased_type(identifier_name.string, immutable_function_type, parser_line_num, NOT_MUTABLE);

	//Once we've created this, we'll add this into the symtab
	insert_type(type_symtab, create_type_record(immutable_alias_type));

	//This worked
	return TRUE;
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
 * BNF Rule: <struct-definer> ::= define struct <identifier> { <construct-member-list> } {as <identifer>}?;
 */
static u_int8_t struct_definer(FILE* fl){
	//Freeze the line num
	u_int16_t current_line = parser_line_num;
	//Lookahead token for our uses
	lexitem_t lookahead;

	//Allocate it
	dynamic_string_t type_name = dynamic_string_alloc();

	//Set it
	dynamic_string_set(&type_name, "struct ");

	//Get the next token
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail case
	if(lookahead.tok != IDENT){
		print_parse_message(PARSE_ERROR, "Valid identifier required after struct keyword", parser_line_num);
		num_errors++;
		//Fail out
		return FAILURE;
	}

	//Add the name on the end
	dynamic_string_concatenate(&type_name, lookahead.lexeme.string);

	//Check that there are no duplicated types
	if(do_duplicate_types_exist(type_name.string) == TRUE){
		return FAILURE;
	}

	//If we make it here, we've made it far enough to know what we need to build our type for this construct
	//We start with the immutable type
	generic_type_t* immutable_struct_type = create_struct_type(type_name, current_line, NOT_MUTABLE);
	generic_type_t* mutable_struct_type = create_struct_type(clone_dynamic_string(&type_name), current_line, MUTABLE);
	
	//Now we'll insert the struct type into the symtab
	insert_type(type_symtab, create_type_record(immutable_struct_type));
	insert_type(type_symtab, create_type_record(mutable_struct_type));

	//We are now required to see a valid construct member list
	u_int8_t success = struct_member_list(fl, mutable_struct_type, immutable_struct_type);

	//Automatic fail case here
	if(success == FAILURE){
		print_parse_message(PARSE_ERROR, "Invalid struct member list given in construct definition", parser_line_num);
		//Fail out
		return FAILURE;
	}

	//Once we get here, the struct type's size is known and as such it is complete
	immutable_struct_type->type_complete = TRUE;
	mutable_struct_type->type_complete = TRUE;
	
	//Now we have one final thing to account for. The syntax allows for us to alias the type right here. This may
	//be preferable to doing it later, and is certainly more convenient. If we see a semicol right off the bat, we'll
	//know that we're not aliasing however
	lookahead = get_next_token(fl, &parser_line_num);

	//We're out of here, just return the node that we made
	if(lookahead.tok == SEMICOLON){
		//No aliasing here so we're done
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
	lookahead = get_next_token(fl, &parser_line_num);

	//If it was invalid leave
	if(lookahead.tok != IDENT){
		print_parse_message(PARSE_ERROR, "Invalid identifier given as alias", parser_line_num);
		num_errors++;
		//Deallocate and fail
		return FAILURE;
	}

	//Let's grab the actual name out
	dynamic_string_t alias_name = lookahead.lexeme;

	//Once we have this, the alias ident is of no use to us

	//Real quick, let's check to see if we have the semicol that we need now
	lookahead = get_next_token(fl, &parser_line_num);

	//Last chance for us to fail syntactically 
	if(lookahead.tok != SEMICOLON){
		print_parse_message(PARSE_ERROR, "Semicolon expected after construct definition",  parser_line_num);
		num_errors++;
		//Fail out
		return FAILURE;
	}

	//Fail out if they exist
	if(do_duplicate_variables_exist(alias_name.string) == TRUE){
		return FAILURE;
	}

	//If we find duplicates then leave
	if(do_duplicate_variables_exist(alias_name.string) == TRUE){
		return FAILURE;
	}

	//Use the helper to look for duplicates
	if(do_duplicate_types_exist(alias_name.string) == TRUE){
		return FAILURE;
	}

	//Now we'll make the actual record for the aliased type that is immutable
	generic_type_t* immutable_aliased_type = create_aliased_type(alias_name.string, immutable_struct_type, parser_line_num, NOT_MUTABLE);

	//Once we've made the aliased type, we can record it in the symbol table
	insert_type(type_symtab, create_type_record(immutable_aliased_type));

	//Now that we've made the immutable alias, we must also make the mutable alias
	generic_type_t* mutable_aliased_type = create_aliased_type(alias_name.string, mutable_struct_type, parser_line_num, MUTABLE);

	//Add this into the symtab too
	insert_type(type_symtab, create_type_record(mutable_aliased_type));

	//Succeeded so
	return SUCCESS;
}


/**
 * The union type specifier is a distinct version of the type specifier
 * rule that is designed just for union members. It is designed this way
 * because union members *may not be declared as mutable*. Their mutability
 * is added later on. The mutability is given to this rule
 *
 * BNF Rule: <union-type-specifier> ::= <type-name>{<type-address-specifier>}*
 */
static generic_type_t* union_type_specifier(FILE* fl, mutability_type_t mutability){
	//Lookahead token
	lexitem_t lookahead;

	//Now we'll hand off the rule to the <type-name> function. The type name function will
	//return a record of the node that the type name has. If the type name function could not
	//find the name, then it will send back an error that we can handle here
	symtab_type_record_t* type = type_name(fl, mutability);

	//We'll just fail here, no need for any error printing
	if(type == NULL){
		//It's already in error so just NULL out
		return NULL;
	}

	//Now once we make it here, we know that we have a name that actually exists in the symtab
	//The current type record is what we will eventually point our node to
	symtab_type_record_t* current_type_record = type;
	
	//Let's see where we go from here
	lookahead = get_next_token(fl, &parser_line_num);

	//As long as we are seeing pointer specifiers
	while(lookahead.tok == STAR){
		//We keep seeing STARS, so we have a pointer type
		//Let's create the pointer type. This pointer type will point to the current type
		generic_type_t* pointer = create_pointer_type(current_type_record->type, parser_line_num, mutability);

		//We'll now add it into the type symbol table. If it's already in there, which it very well may be, that's
		//also not an issue
		symtab_type_record_t* found_pointer = lookup_type(type_symtab, pointer);

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

		//Refresh the search, keep hunting
		lookahead = get_next_token(fl, &parser_line_num);
	}

	//If we don't see an array here, we can just leave now
	if(lookahead.tok != L_BRACKET){
		//Put it back
		push_back_token(lookahead);

		//It is not possible to have a void type as a union member
		if(current_type_record->type == immut_void
			|| current_type_record->type == mut_void){
			print_parse_message(PARSE_ERROR, "Unions may not have members that are void", parser_line_num);
			num_errors++;
			return NULL;
		}

		//We're done here
		return current_type_record->type;
	}

	//Otherwise, we know we're in for the array
	//We'll use a lightstack for the bounds reversal
	lightstack_t lightstack = lightstack_initialize();

	//As long as we are seeing L_BRACKETS
	while(lookahead.tok == L_BRACKET){
		//Scan ahead to see
		lookahead = get_next_token(fl, &parser_line_num);

		//We could just see an empty one here. This tells us that we have 
		//an empty array initializer. If we do see this, we can break out here
		if(lookahead.tok == R_BRACKET){
			//Scan ahead to see
			lookahead = get_next_token(fl, &parser_line_num);

			//This is a special case where we are able to have an unitialized array for the time
			//being. This only works if we have an array initializer afterwards
			
			//We're all set, push this onto the lightstack
			lightstack_push(&lightstack, 0);

			//Onto the next iteration
			continue;
		}

		//Otherwise we need to put this token back
		push_back_token(lookahead);

		//The next thing that we absolutely must see is a constant. If we don't, we're
		//done here
		generic_ast_node_t* const_node = constant(fl, SIDE_TYPE_LEFT);

		//If it failed, then we're done here
		if(const_node->ast_node_type == AST_NODE_TYPE_ERR_NODE){
			print_parse_message(PARSE_ERROR, "Invalid constant given in array declaration", parser_line_num);
			num_errors++;
			return NULL;
		}

		//One last thing before we do expensive validation - what if there's no closing bracket? If there's not, this
		//is an easy fail case 
		lookahead = get_next_token(fl, &parser_line_num);

		//Fail case here 
		if(lookahead.tok != R_BRACKET){
			print_parse_message(PARSE_ERROR, "Unmatched brackets in array declaration", parser_line_num);
			num_errors++;
			return NULL;
		}

		//Determine if we have an illegal constant given as the array bounds
		switch(const_node->constant_type){
			case FLOAT_CONST:
			case STR_CONST:
				print_parse_message(PARSE_ERROR, "Illegal constant given as array bounds", parser_line_num);
				num_errors++;
				return NULL;
			default:
				break;
		}

		//The constant value
		int64_t constant_numeric_value = const_node->constant_value.unsigned_long_value;

		//What if this is a negative or zero?
		//If it's negative we fail like this
		if(constant_numeric_value < 0){
			print_parse_message(PARSE_ERROR, "Array bounds may not be negative", parser_line_num);
			num_errors++;
			return NULL;
		}

		//If it's zero we fail like this
		if(constant_numeric_value == 0){
			print_parse_message(PARSE_ERROR, "Array bounds may not be zero", parser_line_num);
			num_errors++;
			return NULL;
		}

		//We're all set, push this onto the lightstack
		lightstack_push(&lightstack, constant_numeric_value);

		//Refresh the search
		lookahead = get_next_token(fl, &parser_line_num);
	}

	//Since we made it down here, we need to push the token back
	push_back_token(lookahead);

	//Now we'll go back through and unwind the lightstack
	while(lightstack_is_empty(&lightstack) == FALSE){
		//Grab the number of bounds out
		u_int32_t num_bounds = lightstack_pop(&lightstack);

		//If we're trying to create an array out of a type that is not yet fully
		//defined, we also need to fail out. There exists a special exception here for array types, because we can
		//initially define them as blank if and only if we're using an initializer
		if(current_type_record->type->type_class != TYPE_CLASS_ARRAY && current_type_record->type->type_complete == FALSE){
			sprintf(info, "Attempt to use incomplete type %s as an array member. Array member types must be fully defined before use", current_type_record->type->type_name.string);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			num_errors++;
			return NULL;
		}

		//If we get here though, we know that this one is good
		//Lets create the array type
		generic_type_t* array_type = create_array_type(current_type_record->type, parser_line_num, num_bounds, mutability);

		//Let's see if we can find this one
		symtab_type_record_t* found_array = lookup_type(type_symtab, array_type);

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

	//We're done with it, so deallocate
	lightstack_dealloc(&lightstack);

	//Give back whatever the current type may be
	return current_type_record->type;
}


/**
 * Parse and add a union member into our union type
 *
 * It is important to remember that union types do not allow for 
 * their variables to individually be mutable/immutable. This is because
 * a union type shares all memory, so it makes no sense to have a mutable 
 * member and a non-mutable member. It is for this reason that each union member is given a
 * mutable and immutable version
 *
 *
 * BNF Rule: <union-member> ::= <identifier>:<union-type-specifier>;
 */
static u_int8_t union_member(FILE* fl, generic_type_t* mutable_union_type, generic_type_t* immutable_union_type){
	//Our lookahead token
	lexitem_t lookahead;

	//Let's fetch the first token
	lookahead = get_next_token(fl, &parser_line_num);

	//Once we're here, we need to see an identifier token. If we don't, we'll fail out
	if(lookahead.tok != IDENT){
		print_parse_message(PARSE_ERROR, "Identifier expected in union member declaration", parser_line_num);
		num_errors++;
		return FAILURE;
	}

	//Otherwise we did find it, so let's grab the name out
	dynamic_string_t name = lookahead.lexeme;

	//Check for duplicate member vars. We only need to check one of the lists,
	//both lists have the same physical variables
	if(do_duplicate_member_variables_exist(name.string, mutable_union_type) == TRUE){
		return FAILURE;
	}

	//Check for duplicated functions
	if(do_duplicate_functions_exist(name.string) == TRUE){
		return FAILURE;
	}

	//If we have duplicate types, that is also a failure
	if(do_duplicate_types_exist(name.string) == TRUE){
		return FAILURE;
	}

	//Now that we know it's all good, we can keep parsing. We next need to see a colon
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail out if we don't have it
	if(lookahead.tok != COLON){
		print_parse_message(PARSE_ERROR, "Colon required after identifier in union member definition", parser_line_num);
		num_errors++;
		return FAILURE;
	}

	/**
	 * Unique strategy here - we need to get a mutable and immutable version of this
	 * type for our mutable and immutable union type. To do this, we'll simply
	 * consume the data twice, once as mutable and once as not mutable
	 *
	 * We will do this by hanging onto where we started consuming from
	 */

	//Where did we start consuming from
	int64_t type_start = GET_CURRENT_FILE_POSITION(fl);

	//Now we need to see a valid type-specifier
	generic_type_t* mutable_type = union_type_specifier(fl, MUTABLE);

	//If this is NULL we've failed
	if(mutable_type == NULL){
		print_parse_message(PARSE_ERROR, "Invalid type given to union type", parser_line_num);
		num_errors++;
		return FAILURE;
	}

	//Add extra validation to ensure that the size of said type is known at comptime. This will stop
	//the user from adding a field the mut a:char[] that is unknown at compile time
	if(mutable_type->type_complete == FALSE){
		sprintf(info, "Attempt to use incomplete type %s as a union member. Union members must have a size known at compile time", mutable_type->type_name.string);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		return FAILURE;
	}

	//Rewind our position
	reconsume_tokens(fl, type_start);

	//Now we do the exact same consumption for the immutable version. We don't need
	//to do any of the checking, as it's the exact same type
	generic_type_t* immutable_type = union_type_specifier(fl, NOT_MUTABLE);

	//Now that we have the type as well, we can finally see the semicolon to close it off
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail out here if we don't have it
	if(lookahead.tok != SEMICOLON){
		print_parse_message(PARSE_ERROR, "Semicolon required after union member declaration", parser_line_num);
		num_errors++;
		return FAILURE;
	}

	//Finally we can create our members
	//First goes the mutable one
	symtab_variable_record_t* mutable_union_member = create_variable_record(name);
	//Give it its type
	mutable_union_member->type_defined_as = mutable_type;
	//And we'll let the helper add it into the union type
	add_union_member(mutable_union_type, mutable_union_member);

	//Now the immutable version
	symtab_variable_record_t* immutable_union_member = create_variable_record(clone_dynamic_string(&name));
	//Give it its type
	immutable_union_member->type_defined_as = immutable_type;
	//And we'll let the helper add it into the union type
	add_union_member(immutable_union_type, immutable_union_member);

	//If we make it here then we succeeded
	return SUCCESS;
}


/**
 * Parse the union member list for a given union type
 *
 * This will handle creating both the mutable and immutable union member lists. Remember that
 * each union definition creates 2 types, one that is entirely immutable and one that is entirely
 * mutable. There is no in-between due to how a union shares memory
 *
 * BNF RULE: <union-member-list> ::= { {<union-member>}+ }
 */
static u_int8_t union_member_list(FILE* fl, generic_type_t* mutable_union_type, generic_type_t* immutable_union_type){
	//We must first see an L_curly
	lexitem_t lookahead = get_next_token(fl, &parser_line_num);

	//If it's not a curly we fail
	if(lookahead.tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Left curly required after union name", parser_line_num);
		num_errors++;
		return FAILURE;
	}

	//Otherwise push onto the grouping stack for matching
	push_token(&grouping_stack, lookahead);

	//Refresh the token once
	lookahead = get_next_token(fl, &parser_line_num);

	//Now we need to see union members so long as we don't hit the closing R_CURLY
	do {
		//Push the token back
		push_back_token(lookahead);

		//Call the helper union member function
		u_int8_t status = union_member(fl, mutable_union_type, immutable_union_type);

		//If one of them fails, then we're out
		if(status == FAILURE){
			print_parse_message(PARSE_ERROR, "Invalid union member defition", parser_line_num);
			num_errors++;
			return FAILURE;
		}

		//Refresh the lookahead token
		lookahead = get_next_token(fl, &parser_line_num);

		//So long as we don't hit the closing curly
	} while(lookahead.tok != R_CURLY);

	//Once we get down here then we know that we've got an R_CURLY. Let's ensure that we have a grouping
	//stack match
	if(pop_token(&grouping_stack).tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Unmatched curly braces detected", parser_line_num);
		num_errors++;
		return FAILURE;
	}

	//If we make it here then we know that it worked
	return SUCCESS;
}


/**
 * A union definer allows us to declare a discriminating union datatype
 *
 * NOTE: By the time that we get here, we have already seen the UNION keyword 
 *
 * BNF RULE: <union_definer> ::=  define union <identifier> <union-member-list> {as <identifier>}? ;
 */
static u_int8_t union_definer(FILE* fl){
	//Lookahead token for searching
	lexitem_t lookahead;
	//Dynamic string for our type name
	dynamic_string_t union_name = dynamic_string_alloc();
	
	//Add the prefix in
	dynamic_string_set(&union_name, "union ");

	//Now we need to see an identifier
	lookahead = get_next_token(fl, &parser_line_num);

	//If this is an error fail out
	if(lookahead.tok != IDENT){
		print_parse_message(PARSE_ERROR, "Invalid identifier given as union name", parser_line_num);
		return FAILURE;
	}

	//Add the ident into our overall name
	dynamic_string_concatenate(&union_name, lookahead.lexeme.string);

	//Check for type duplication
	if(do_duplicate_types_exist(union_name.string) == TRUE){
		return FAILURE;
	}

	//Creat the mutable version
	generic_type_t* mutable_union_type = create_union_type(union_name, parser_line_num, MUTABLE);

	//Insert into symtab
	insert_type(type_symtab, create_type_record(mutable_union_type));

	//And now create the immutable version
	generic_type_t* immutable_union_type = create_union_type(clone_dynamic_string(&(union_name)), parser_line_num, NOT_MUTABLE);

	//Add the immutable one in
	insert_type(type_symtab, create_type_record(immutable_union_type));

	//Once we've created it, we can begin parsing the internals. We'll call the union member list 
	//and let it handle everything else
	u_int8_t status = union_member_list(fl, mutable_union_type, immutable_union_type);

	//If this fails then we're done
	if(status == FAILURE){
		return FAILURE;
	}

	//Once we've gotten here, the union type is officially considered complete
	mutable_union_type->type_complete = TRUE;
	immutable_union_type->type_complete = TRUE;

	//Now let's see what we have at the end. We could either see a semicolon
	//or an immediate alias statement
	lookahead = get_next_token(fl, &parser_line_num);

	//Switch based on what we have
	switch(lookahead.tok){
		case SEMICOLON:
			return SUCCESS;
		//More to do
		case AS:
			break;
		default:
			print_parse_message(PARSE_ERROR, "AS keyword or semicolon expected after union definition", parser_line_num);
			num_errors++;
			return FAILURE;
	}

	//If we made it here we're aliasing. We need to now see an IDENT
	lookahead = get_next_token(fl, &parser_line_num);

	//If it's not an IDENT we're done
	if(lookahead.tok != IDENT){
		print_parse_message(PARSE_ERROR, "Identifier expected after as keyword", parser_line_num);
		num_errors++;
		return FAILURE;
	}

	//Otherwise we can now extract the name and check for duplication
	dynamic_string_t alias_name = lookahead.lexeme;

	//Before we do all of the expensive symtab checking, let's just see if the user
	//forgot a semicolon first. It's fast to check that
	lookahead = get_next_token(fl, &parser_line_num);

	//If it's not here we're out
	if(lookahead.tok != SEMICOLON){
		print_parse_message(PARSE_ERROR, "Semicolon required after union definition", parser_line_num);
		num_errors++;
		return FAILURE;
	}

	//Check for function duplication
	if(do_duplicate_functions_exist(alias_name.string) == TRUE){
		return FAILURE;
	}

	//Check for variable duplication
	if(do_duplicate_variables_exist(alias_name.string) == TRUE){
		return FAILURE;
	}

	//Check for type duplication
	if(do_duplicate_types_exist(alias_name.string) == TRUE){
		return FAILURE;
	}

	//Construct both mutable & immutable versions of the alias
	generic_type_t* mutable_alias = create_aliased_type(alias_name.string, mutable_union_type, parser_line_num, MUTABLE);

	//Add it into the type symtab
	insert_type(type_symtab, create_type_record(mutable_alias));

	//Construct both mutable & immutable versions of the alias
	generic_type_t* immutable_alias = create_aliased_type(alias_name.string, immutable_union_type, parser_line_num, NOT_MUTABLE);

	//Add it into the type symtab
	insert_type(type_symtab, create_type_record(immutable_alias));

	//If we get here it worked so
	return SUCCESS;
}


/**
 * An enumeration definition is where we see the actual definition of an enum. Since this is a compiler
 * only rule, we will only return success or failure from this node
 *
 * Important note: By the time we get here, we will have already consume the "define" and "enum" tokens
 *
 * BNF Rule: <enum-definer> ::= define enum <identifier> { <identifier> {= <constant>}? {, <identifier>{ = <constant>}?}* } {as <identifier>}?;
 *
 * Another important note - complex types like an enum have the ability to be used as mutable(mut) or immutable. To support this internally,
 * there will be two distinct versions of any given enum, the immutable(the first one that we create), and the mutable one(this will be made)
 * after the fact
 *
 * NOTE: The enum type itself may or may not be mutable, but all of the internal enum values are always *immutable* because once you define
 * the enum, it is not possible for you to change them
 */
static u_int8_t enum_definer(FILE* fl){
	//Lookahead token
	lexitem_t lookahead;
	//Reserve space for the type name
	dynamic_string_t type_name = dynamic_string_alloc();

	//Add the enum intro in
	dynamic_string_set(&type_name, "enum ");

	//We now need to see a valid identifier to round out the name
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail case here
	if(lookahead.tok != IDENT){
		print_parse_message(PARSE_ERROR, "Invalid name given to enum definition", parser_line_num);
		num_errors++;
		//Deallocate and fail
		return FAILURE;
	}

	//Now if we get here we know that we found a valid ident, so we'll add it to the name
	dynamic_string_concatenate(&type_name, lookahead.lexeme.string);

	//If these duplicates exist, we need to fail out
	if(do_duplicate_types_exist(type_name.string) == TRUE){
		return FAILURE;
	}

	//We can create the mutable & immutable versions of the enum types
	generic_type_t* immutable_enum_type = create_enumerated_type(type_name, parser_line_num, NOT_MUTABLE);
	generic_type_t* mutable_enum_type = create_enumerated_type(clone_dynamic_string(&type_name), parser_line_num, MUTABLE);

	//Insert into the type symtab
	insert_type(type_symtab, create_type_record(immutable_enum_type));
	insert_type(type_symtab, create_type_record(mutable_enum_type));

	//Now that we know we don't have a duplicate, we can now start looking for the enum list
	//We must first see an L_CURLY
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail case here
	if(lookahead.tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Left curly expected before enumerator list", parser_line_num);
		num_errors++;
		//Fail out
		return FAILURE;
	}

	//Push onto the stack for grouping
	push_token(&grouping_stack, lookahead);

	//Are we using user-defined enum values? If so, then we need to always see those
	//from the user
	u_int8_t user_defined_enum_values = FALSE;

	//If we are not using a user-defined enum, then this is the current value
	u_int32_t current_enum_value = 0;

	//What is the largest value that we see in the enum?
	u_int64_t largest_value = 0;

	//Now we will enter a do-while loop where we can continue to identifiers for our enums
	do {
		//We need to see a valid identifier
		lookahead = get_next_token(fl, &parser_line_num);

		//If it's not an identifier, we're done
		if(lookahead.tok != IDENT){
			print_parse_message(PARSE_ERROR, "Identifier expected as enum member", parser_line_num);
			num_errors++;
			return FAILURE;
		}

		//Grab this out for convenience
		char* member_name = lookahead.lexeme.string;

		//Check for duplicated functions first
		if(do_duplicate_functions_exist(member_name) == TRUE){
			return FAILURE;
		}

		//Add this variable in
		if(do_duplicate_variables_exist(member_name) == TRUE){
			return FAILURE;
		}

		//CHeck for duplicated types
		if(do_duplicate_types_exist(member_name) == TRUE){
			return FALSE;
		}

		//If we make it here, then all of our checks passed and we don't have a duplicate name. We're now good
		//to create the record and assign it a type
		symtab_variable_record_t* member_record = create_variable_record(lookahead.lexeme);

		//Store the line number
		member_record->line_number = parser_line_num;

		//By virtue of being an enum, this has been initialized 
		member_record->initialized = TRUE;

		//Now we can insert this into the symtab
		insert_variable(variable_symtab, member_record);

		//Refresh the lookahead
		lookahead = get_next_token(fl, &parser_line_num);

		//If we see an equals sign, this means that we have a user-defined enum value
		if(lookahead.tok == EQUALS){
			//If this value is 0, this is the very first iteration. That means that this
			//first element sets the rule for everything
			if(current_enum_value == 0){
				//Set this flag for all future values
				user_defined_enum_values = TRUE;
			}

			//The other case - if this is FALSE and we saw this,
			//then we have an error
			if(user_defined_enum_values == FALSE){
				sprintf(info, "%s has been set as an auto-defined enum. No enum values can be assigned with the = operator", mutable_enum_type->type_name.string);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return FAILURE;
			}

			//Now that we've caught all potential errors, we need to see a constant here
			lookahead = get_next_token(fl, &parser_line_num);

			//Something to store the current value in
			u_int64_t current = 0;

			//Switch based on what we have
			switch(lookahead.tok){
				//Just translate here
				case INT_CONST_FORCE_U:
				case INT_CONST:
					current = atoi(lookahead.lexeme.string);
					break;

				case LONG_CONST_FORCE_U:
				case LONG_CONST:
				case HEX_CONST:
					current = atol(lookahead.lexeme.string);
					break;

				//Character constants are allowed
				case CHAR_CONST:
					current = *(lookahead.lexeme.string);
					break;

				//If we see anything else, leave
				default:
					print_parse_message(PARSE_ERROR, "Integer or char constant expected after = in enum definer", parser_line_num);
					num_errors++;
					return FAILURE;
			}

			//Keep track of what our largest value is
			if(current > largest_value){
				largest_value = current;
			}

			//Assign the value in
			member_record->enum_member_value = current;

			//We need to refresh the lookahead here
			lookahead = get_next_token(fl, &parser_line_num);

		//We did not see an equals
		} else {
			//Are we using user-defined values? If so,
			//then this is wrong
			if(user_defined_enum_values == TRUE){
				sprintf(info, "%s has been set as a user-defined enum. All enum values must be assigned with the = operator", mutable_enum_type->type_name.string);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return FAILURE;
			}

			//Otherwise, this one's value is the current enum value
			member_record->enum_member_value = current_enum_value;

			//The largest value that we've seen is now also this
			largest_value = current_enum_value;
		}

		//Add this in as a member to our current enum
		u_int8_t success = add_enum_member(mutable_enum_type, member_record, user_defined_enum_values);

		//This means that the user attempted to add a duplicate value
		if(success == FAILURE){
			sprintf(info, "Duplicate enum value %d", member_record->enum_member_value);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			num_errors++;
			return FAILURE;
		}

		//Add this in as a member to our current enum
		success = add_enum_member(immutable_enum_type, member_record, user_defined_enum_values);

		//This means that the user attempted to add a duplicate value
		if(success == FAILURE){
			sprintf(info, "Duplicate enum value %d", member_record->enum_member_value);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			num_errors++;
			return FAILURE;
		}

		//This goes up by 1
		current_enum_value++;

	//So long as we keep seeing commas
	} while(lookahead.tok == COMMA);

	//If we get out here, then the lookahead must be an RCURLY. If we don't see it, then fail out
	if(lookahead.tok != R_CURLY){
		print_parse_message(PARSE_ERROR, "Closing curly brace expected after enumeration definition", parser_line_num); 
		num_errors++;
		return FAILURE;
	}

	//Ensure that the grouping stack matches
	if(pop_token(&grouping_stack).tok != L_CURLY){
		print_parse_message(PARSE_ERROR, "Unmatched curly braces detected", parser_line_num); 
		num_errors++;
		return FAILURE;
	}

	//Now, based on our largest value, we need to determine the bit-width needed for this
	//field. Does it need to be stored internally as a u8, u16, u32, or u64?
	//This will *always* be the immutable version of the type
	generic_type_t* type_needed = determine_required_minimum_unsigned_integer_type_size(largest_value, 64);

	//Store this in the enum
	mutable_enum_type->internal_values.enum_integer_type = type_needed;
	//Assign the size over as well
	mutable_enum_type->type_size = type_needed->type_size;

	//We do the exact same for the immutable version
	//Store this in the enum
	immutable_enum_type->internal_values.enum_integer_type = type_needed;
	//Assign the size over as well
	immutable_enum_type->type_size = type_needed->type_size;

	//Now once we are here, we can optionally see an alias command. These alias commands are helpful and convenient
	//for redefining variables immediately upon declaration. They are prefaced by the "As" keyword
	//However, before we do that, we can first see if we have a semicol
	lookahead = get_next_token(fl, &parser_line_num);

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
	lookahead = get_next_token(fl, &parser_line_num);

	//If it was invalid
	if(lookahead.tok != IDENT){
		print_parse_message(PARSE_ERROR, "Invalid identifier given as alias", parser_line_num);
		num_errors++;
		//Deallocate and fail
		return FAILURE;
	}

	//Extract the alias name
	dynamic_string_t alias_name = lookahead.lexeme;

	//Real quick, let's check to see if we have the semicol that we need now
	lookahead = get_next_token(fl, &parser_line_num);

	//Last chance for us to fail syntactically 
	if(lookahead.tok != SEMICOLON){
		print_parse_message(PARSE_ERROR, "Semicolon expected after enum definition",  parser_line_num);
		num_errors++;
		//Fail out
		return FAILURE;
	}

	//Check for duplicate functions first
	if(do_duplicate_functions_exist(alias_name.string) == TRUE){
		return FAILURE;
	}

	//Now duplicate vars
	if(do_duplicate_variables_exist(alias_name.string) == TRUE){
		return FAILURE;
	}

	//Now duplicate types
	if(do_duplicate_types_exist(alias_name.string) == TRUE){
		return FAILURE;
	}

	//Now we need to create both our mutable and immutable aliases
	generic_type_t* mutable_alias = create_aliased_type(alias_name.string, mutable_enum_type, parser_line_num, MUTABLE);
	//Once we've made the aliased type, we can record it in the symbol table
	insert_type(type_symtab, create_type_record(mutable_alias));

	//Now the immutable version
	generic_type_t* immutable_alias = create_aliased_type(alias_name.string, immutable_enum_type, parser_line_num, NOT_MUTABLE);
	//Once we've made the aliased type, we can record it in the symbol table
	insert_type(type_symtab, create_type_record(immutable_alias));

	//This is a successful creation
	return SUCCESS;
}


/**
 * A type name node is always a child of a type specifier. It consists
 * of all of our primitive types and any defined construct or
 * aliased types that we may have. It is important to note that any
 * non-primitive type needs to have been previously defined for it to be
 * valid. 
 * 
 * If we are using this rule, we are assuming that this type exists in the system
 *
 * This rule will NOT return a node. Instead, we just return the type record that we found.
 * If we have an error, we will return NULL
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
 * 						   | union <identifier>
 * 						   | struct <identifier>
 * 						   | <identifier>
 */
static symtab_type_record_t* type_name(FILE* fl, mutability_type_t mutability){
	//Lookahead token
	lexitem_t lookahead;
	//Hold the record we get
	symtab_type_record_t* record;

	//Create a dstring for the type name
	dynamic_string_t type_name = dynamic_string_alloc();

	//Let's see what we have
	lookahead = get_next_token(fl, &parser_line_num);

	switch(lookahead.tok){
		case VOID:
		case U8:
		case I8:
		case U16:
		case I16:
		case U32:
		case I32:
		case F32:
		case U64:
		case I64:
		case F64:
		case CHAR:
		case BOOL:
			//We will now grab this record from the symtable to make our life easier
			record = lookup_type_name_only(type_symtab, lookahead.lexeme.string, mutability);

			//Sanity check, if this is null something is very wrong
			if(record == NULL){
				print_parse_message(PARSE_ERROR, "Fatal internal compiler error. Primitive type could not be found in symtab", parser_line_num);
				//Create and give back an error node
				return NULL;
			}

			//This one is now all set to send up. We will not store any children if this is the case
			return record;
		
		//Enumerated type
		case ENUM:
			//We know that this keyword is in the name, so we'll add it in
			dynamic_string_set(&type_name, "enum ");

			//Now we need to see a valid identifier
			lookahead = get_next_token(fl, &parser_line_num);

			//If we fail, we'll bail out
			if(lookahead.tok != IDENT){
				print_parse_message(PARSE_ERROR, "Invalid identifier given as enum type name", parser_line_num);
				//It's already an error so just give it back
				return NULL;
			}

			//Otherwise it actually did work, so we'll add it's name onto the already existing type node
			dynamic_string_concatenate(&type_name, lookahead.lexeme.string);

			//Now we'll look up the record in the symtab. As a reminder, it is required that we see it here
			symtab_type_record_t* record = lookup_type_name_only(type_symtab, type_name.string, mutability);

			//If we didn't find it it's an instant fail
			if(record == NULL){
				sprintf(info, "Type %s was never defined. Types must be defined before use", type_name.string);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				//Create and return an error node
				return NULL;
			}

			//Once we make it here, we should be all set to get out
			return record;

		//Struct type
		case STRUCT:
			//First add the struct into the name
			dynamic_string_set(&type_name, "struct ");

			//We need to see an ident here
			lookahead = get_next_token(fl, &parser_line_num);

			//If it's not an ident, leave
			if(lookahead.tok != IDENT){
				print_parse_message(PARSE_ERROR, "Invalid identifier given as struct type name", parser_line_num);
				num_errors++;
				//Throw an error up
				return NULL;
			}

			//Otherwise it actually did work, so we'll add it's name onto the already existing type node
			dynamic_string_concatenate(&type_name, lookahead.lexeme.string);

			//Now we'll look up the record in the symtab. As a reminder, it is required that we see it here
			record = lookup_type_name_only(type_symtab, type_name.string, mutability);

			//If we didn't find it it's an instant fail
			if(record == NULL){
				sprintf(info, "Type %s was never defined. Types must be defined before use", type_name.string);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				//Create and return an error node
				return NULL;
			}

			//Once we make it here, we should be all set to get out
			return record;

		//Union type
		case UNION:
			//Add union into the name
			dynamic_string_set(&type_name, "union ");

			//Now we'll need to see an ident
			lookahead = get_next_token(fl, &parser_line_num);

			//If we don't have one, then we need to fail out
			if(lookahead.tok != IDENT){
				print_parse_message(PARSE_ERROR, "Invalid identifier given as union type name", parser_line_num);
				num_errors++;
				//Send an error up the chain
				return NULL;
			}

			//Add the name onto the "union" qualifier
			dynamic_string_concatenate(&type_name, lookahead.lexeme.string);

			//Now we'll look up the record in the symtab. As a reminder, it is required that we see it here
			record = lookup_type_name_only(type_symtab, type_name.string, mutability);

			//If we didn't find it it's an instant fail
			if(record == NULL){
				sprintf(info, "Type %s was never defined. Types must be defined before use", type_name.string);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				//Create and return an error node
				return NULL;
			}

			//Once we make it here, we should be all set to get out
			return record;

		//This is an identifier
		case IDENT:
			//Now we'll look up the record in the symtab. As a reminder, it is required that we see it here
			record = lookup_type_name_only(type_symtab, lookahead.lexeme.string, mutability);

			//If we didn't find it it's an instant fail
			if(record == NULL){
				sprintf(info, "Type %s was never defined. Types must be defined before use", lookahead.lexeme.string);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				//Create and return an error node
				return NULL;
			}
			
			//Dealias the type here
			generic_type_t* dealiased_type = dealias_type(record->type);

			//The true type record
			symtab_type_record_t* true_type = lookup_type_name_only(type_symtab, dealiased_type->type_name.string, mutability);

			//Once we make it here, we should be all set to get out
			return true_type;

		//If we hit down here, we have some invalid lexeme that isn't a type name at all
		default:
			print_parse_message(PARSE_ERROR, "Type name expected but not found", parser_line_num);
			num_errors++;
			return NULL;
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
 * We do not need to return any nodes here, just the type we get out
 *
 * The type specifier also contains all of the mutability information needed for a type
 *
 * BNF Rule: <type-specifier> ::= {mut}? <type-name>{<type-address-specifier>}*
 *
 * Array type mutability rules:
 *
 * mut i32[35] -> this creates a mutable array(so the actual arr var is mutable) of mutable i32's(every single member
 * is also mutable)
 */
static generic_type_t* type_specifier(FILE* fl){
	//We always assume immutability
	mutability_type_t mutability = NOT_MUTABLE;

	//Lookahead var
	lexitem_t lookahead = get_next_token(fl, &parser_line_num);

	//If we see the mut keyword, flag that we're mutable
	if(lookahead.tok == MUT){
		mutability = MUTABLE;
	} else {
		//Put it back if not
		push_back_token(lookahead);
	}

	//Now we'll hand off the rule to the <type-name> function. The type name function will
	//return a record of the node that the type name has. If the type name function could not
	//find the name, then it will send back an error that we can handle here
	symtab_type_record_t* type = type_name(fl, mutability);

	//We'll just fail here, no need for any error printing
	if(type == NULL){
		//It's already in error so just NULL out
		return NULL;
	}

	//Now once we make it here, we know that we have a name that actually exists in the symtab
	//The current type record is what we will eventually point our node to
	symtab_type_record_t* current_type_record = type;
	
	//Let's see where we go from here
	lookahead = get_next_token(fl, &parser_line_num);

	//As long as we are seeing pointer/reference specifiers
	while(lookahead.tok == STAR || lookahead.tok == SINGLE_AND){
		//Predeclare here due to switch rules
		symtab_type_record_t* found_pointer;
		symtab_type_record_t* found_reference;
		
		//Handle either a pointer or reference
		switch(lookahead.tok){
			//Pointer type(also called a raw pointer) here
			case STAR:
				//Let's see if we can find it first. We want to avoid creating memory if we're able to,
				//so this step is important
				found_pointer = lookup_pointer_type(type_symtab, current_type_record->type, mutability);

				//If we did not find it, we will add it into the symbol table
				if(found_pointer == NULL){
					//Let's create the pointer type. This pointer type will point to the current type
					generic_type_t* pointer = create_pointer_type(current_type_record->type, parser_line_num, mutability);

					//Create the type record
					symtab_type_record_t* created_pointer = create_type_record(pointer);
					//Insert it into the symbol table
					insert_type(type_symtab, created_pointer);
					//We'll also set the current type record to be this
					current_type_record = created_pointer;

				//Otherwise we've already gotten it, so just use it for our purposes here
				} else {
					//Otherwise, just set the current type record to be what we found
					current_type_record = found_pointer;
				}

				break;

			//Reference type - a pointer with more rules & restrictions
			case SINGLE_AND:
				//Let's see if we're able to find a reference type like this that already exists
				found_reference = lookup_reference_type(type_symtab, current_type_record->type, mutability);

				//If we did not find it, we will add it into the symbol table
				if(found_reference == NULL){
					//Create it using the helper. It will be pointing to our current type
					generic_type_t* reference = create_reference_type(current_type_record->type, parser_line_num, mutability);

					//Create the type record
					symtab_type_record_t* created_reference = create_type_record(reference);
					//Insert it into the symbol table
					insert_type(type_symtab, created_reference);
					//We'll also set the current type record to be this
					current_type_record = created_reference;
				} else {
					//Otherwise, just set the current type record to be what we found
					current_type_record = found_reference;
				}

				break;
				
			//Should be unreachable
			default:
				break;
		}

		//Refresh the search, keep hunting
		lookahead = get_next_token(fl, &parser_line_num);
	}

	//If we don't see an array here, we can just leave now
	if(lookahead.tok != L_BRACKET){
		//Put it back
		push_back_token(lookahead);

		/**
		 * This is a very unique case. Internally, the system needs to have
		 * a "mutable" void type in order to support things like mut void*, etc.. However,
		 * if the user attempts to do something like fn my_fn() -> mut void, we should
		 * throw an error here and disallow that. For all the user knows, there is no
		 * mut void
		 */
		if(current_type_record->type == mut_void){
			print_parse_message(PARSE_ERROR, "Void types do not contain values and therefore cannot be declared mutable. Remove the \"mut\" specificer", parser_line_num);
			num_errors++;
			return NULL;
		}

		//We're done here
		return current_type_record->type;
	}

	//Otherwise, we know we're in for the array
	//We'll use a lightstack for the bounds reversal
	lightstack_t lightstack = lightstack_initialize();

	//As long as we are seeing L_BRACKETS
	while(lookahead.tok == L_BRACKET){
		//Scan ahead to see
		lookahead = get_next_token(fl, &parser_line_num);

		//We could just see an empty one here. This tells us that we have 
		//an empty array initializer. If we do see this, we can break out here
		if(lookahead.tok == R_BRACKET){
			//Scan ahead to see
			lookahead = get_next_token(fl, &parser_line_num);

			//This is a special case where we are able to have an unitialized array for the time
			//being. This only works if we have an array initializer afterwards
			
			//We're all set, push this onto the lightstack
			lightstack_push(&lightstack, 0);

			//Onto the next iteration
			continue;
		}

		//Otherwise we need to put this token back
		push_back_token(lookahead);

		//The next thing that we absolutely must see is a constant. If we don't, we're
		//done here
		generic_ast_node_t* const_node = constant(fl, SIDE_TYPE_LEFT);

		//If it failed, then we're done here
		if(const_node->ast_node_type == AST_NODE_TYPE_ERR_NODE){
			print_parse_message(PARSE_ERROR, "Invalid constant given in array declaration", parser_line_num);
			num_errors++;
			return NULL;
		}

		//One last thing before we do expensive validation - what if there's no closing bracket? If there's not, this
		//is an easy fail case 
		lookahead = get_next_token(fl, &parser_line_num);

		//Fail case here 
		if(lookahead.tok != R_BRACKET){
			print_parse_message(PARSE_ERROR, "Unmatched brackets in array declaration", parser_line_num);
			num_errors++;
			return NULL;
		}

		//Determine if we have an illegal constant given as the array bounds
		switch(const_node->constant_type){
			case FLOAT_CONST:
			case STR_CONST:
				print_parse_message(PARSE_ERROR, "Illegal constant given as array bounds", parser_line_num);
				num_errors++;
				return NULL;
			default:
				break;
		}

		//The constant value
		int64_t constant_numeric_value = const_node->constant_value.unsigned_long_value;

		//What if this is a negative or zero?
		//If it's negative we fail like this
		if(constant_numeric_value < 0){
			print_parse_message(PARSE_ERROR, "Array bounds may not be negative", parser_line_num);
			num_errors++;
			return NULL;
		}

		//If it's zero we fail like this
		if(constant_numeric_value == 0){
			print_parse_message(PARSE_ERROR, "Array bounds may not be zero", parser_line_num);
			num_errors++;
			return NULL;
		}

		//We're all set, push this onto the lightstack
		lightstack_push(&lightstack, constant_numeric_value);

		//Refresh the search
		lookahead = get_next_token(fl, &parser_line_num);
	}

	//Since we made it down here, we need to push the token back
	push_back_token(lookahead);

	//Now we'll go back through and unwind the lightstack
	while(lightstack_is_empty(&lightstack) == FALSE){
		//Grab the member count
		u_int32_t num_members = lightstack_pop(&lightstack);

		//If we're trying to create an array out of a type that is not yet fully
		//defined, we also need to fail out. There exists a special exception here for array types, because we can
		//initially define them as blank if and only if we're using an initializer
		if(current_type_record->type->type_class != TYPE_CLASS_ARRAY && current_type_record->type->type_complete == FALSE){
			sprintf(info, "Attempt to use incomplete type %s as an array member. Array member types must be fully defined before use", current_type_record->type->type_name.string);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			num_errors++;
			return NULL;
		}

		//We can do some optimization with our allocations if we have a set array member count already
		if(num_members != 0){
			//Lookup the array type first
			symtab_type_record_t* found_array = lookup_array_type(type_symtab, current_type_record->type, num_members, mutability);

			//If we did not find it, we will add it into the symbol table
			if(found_array == NULL){
				//If we get here, we need to create an array type
				generic_type_t* array_type = create_array_type(current_type_record->type, parser_line_num, num_members, mutability);

				//Create the type record
				symtab_type_record_t* created_array = create_type_record(array_type);
				//Insert it into the symbol table
				insert_type(type_symtab, created_array);
				//We'll also set the current type record to be this
				current_type_record = created_array;

			//Otherwise we already have it, no need to do any extra creation
			} else {
				//Otherwise, just set the current type record to be what we found
				current_type_record = found_array;
			}

		//If we have 0 members, we need to create a distinct array type no matter what so we won't bother 
		//checking
		} else {
			//If we get here, we need to create an array type
			generic_type_t* array_type = create_array_type(current_type_record->type, parser_line_num, num_members, mutability);

			//Create the type record
			symtab_type_record_t* created_array = create_type_record(array_type);
			//Insert it into the symbol table
			insert_type(type_symtab, created_array);
			//We'll also set the current type record to be this
			current_type_record = created_array;
		}
	}

	//We're done with it, so deallocate
	lightstack_dealloc(&lightstack);

	//Give back whatever the current type may be
	return current_type_record->type;
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
	lexitem_t lookahead;

	//Let's see if we have a semicolon. If we do, we'll just jump right out
	lookahead = get_next_token(fl, &parser_line_num);

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
	if(expr_node->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		//It's already an error, so just send it back up
		return expr_node;
	}

	//Now to close out we must see a semicolon
	//Let's see if we have a semicolon
	lookahead = get_next_token(fl, &parser_line_num);

	//Empty expression, we're done here
	if(lookahead.tok != SEMICOLON){
		return print_and_return_error("Semicolon expected after statement", current_line);
	}

	//Otherwise we're all set
	return expr_node;
}


/**
 * A labeled statement allows the user in ollie to directly jump to where
 * they'd like to go, with some exceptions
 *
 * NOTE: By the time we get here, we have already seen & consumed the #
 *
 * <labeled-statement> ::= #<label-identifier>: 
 */
static generic_ast_node_t* labeled_statement(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	//Lookahead token
	lexitem_t lookahead;

	//Do we contain a defer at any point in here? If so, that is invalid because we could
	//have the defer block duplicated multiple times. As such, a label would become ambiguous
	if(nesting_stack_contains_level(&nesting_stack, NESTING_DEFER_STATEMENT) == TRUE){
		return print_and_return_error("Label statements cannot be placed inside of deferred blocks", parser_line_num);
	}

	//Let's create the label ident node
	generic_ast_node_t* label_stmt = ast_node_alloc(AST_NODE_TYPE_LABEL_STMT, SIDE_TYPE_LEFT);
	//Save our line number
	label_stmt->line_number = parser_line_num;

	//Let's see if we can find one
	lookahead = get_next_token(fl, &parser_line_num);

	//If it's bad we'll fail out here
	if(lookahead.tok != IDENT){
		return print_and_return_error("Invalid identifier given as label ident statement", current_line);
	}

	//Grab the name out for convenience
	dynamic_string_t label_name = lookahead.lexeme;
		
	//Let's also verify that we have the colon right now
	lookahead = get_next_token(fl, &parser_line_num);

	//If we don't see one, we need to scrap it
	if(lookahead.tok != COLON){
		return print_and_return_error("Colon required after label statement", current_line);
	}
	
	//If this function already exists, we fail out
	if(do_duplicate_functions_exist(label_name.string) == TRUE){
		return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
	}

	//We now need to make sure that it isn't a duplicate. We'll use a special search function to do this
	symtab_variable_record_t* found_variable = lookup_variable_lower_scope(variable_symtab, label_name.string);

	//If we did find it, that's bad
	if(found_variable != NULL){
		sprintf(info, "Identiifer %s has already been declared. First declared here: ", label_name.string); 
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		print_variable_name(found_variable);
		num_errors++;
		//give back an error node
		return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
	}
	
	//Check for duplicated types
	if(do_duplicate_types_exist(label_name.string)){
		return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
	}

	//Now that we know we didn't find it, we'll create it
	symtab_variable_record_t* label = create_variable_record(label_name);
	//Store the type
	label->type_defined_as = immut_u64;
	//Store the fact that it is a label
	label->membership = LABEL_VARIABLE;
	//Store the line number
	label->line_number = parser_line_num;
	//Store what function it's defined in(important for later)
	label->function_declared_in = current_function;

	//Put into the symtab
	insert_variable(variable_symtab, label);

	//We'll also associate this variable with the node
	label_stmt->variable = label;
	label_stmt->inferred_type = immut_u64;

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
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	//Lookahead tokens
	lexitem_t lookahead;
	lexitem_t lookahead2;

	//Push the if statement nesting level
	push_nesting_level(&nesting_stack, NESTING_IF_STATEMENT);

	//Let's first create our if statement. This is an overall header for the if statement as a whole. Everything
	//will be a child of this statement
	generic_ast_node_t* if_stmt = ast_node_alloc(AST_NODE_TYPE_IF_STMT, SIDE_TYPE_LEFT);

	//Remember, we've already seen the if token, so now we just need to see an L_PAREN
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail out if we don't have it
	if(lookahead.tok != L_PAREN){
		return print_and_return_error("Left parenthesis expected after if statement", current_line);
	}

	//Push onto the stack for matching later
	push_token(&grouping_stack, lookahead);
	
	//We now need to see a valid conditional expression
	generic_ast_node_t* expression_node = logical_or_expression(fl, SIDE_TYPE_RIGHT);

	//If we see an invalid one
	if(expression_node->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		return print_and_return_error("Invalid conditional expression given as if statement condition", current_line);
	}

	//If it's not of this type or a compatible type(pointer, smaller int, etc, it is out)
	if(is_type_valid_for_conditional(expression_node->inferred_type) == FALSE){
		sprintf(info, "Type %s is invalid to be used in a conditional", expression_node->inferred_type->type_name.string);
		return print_and_return_error(info, parser_line_num);
	}

	//Following the expression we need to see a closing paren
	lookahead = get_next_token(fl, &parser_line_num);

	//If we don't see the R_Paren
	if(lookahead.tok != R_PAREN){
		return print_and_return_error("Right parenthesis expected after expression in if statement", current_line);
	}

	//Now let's check the stack, we need to have matching ones here
	if(pop_token(&grouping_stack).tok != L_PAREN){
		return print_and_return_error("Unmatched parenthesis detected", current_line);
	}

	//If we make it here, we can add this in as the first child to the root node
	add_child_node(if_stmt, expression_node);

	//Now following this, we need to see a valid compound statement
	generic_ast_node_t* compound_stmt_node = compound_statement(fl);

	//If this node fails, whole thing is bad
	if(compound_stmt_node->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		num_errors++;
		//It's already an error, so just send it back up
		return compound_stmt_node;
	}

	//If we make it down here, we know that it's valid. As such, we can now add it as a child node
	add_child_node(if_stmt, compound_stmt_node);

	//Now we're at the point where we can optionally see else if statements.
	lookahead = get_next_token(fl, &parser_line_num);
	lookahead2 = get_next_token(fl, &parser_line_num);

	//So long as we see "else if's", we will keep repeating this process
	while(lookahead.tok == ELSE && lookahead2.tok == IF){
		//We've found one - let's create our fresh else if node
		generic_ast_node_t* else_if_node = ast_node_alloc(AST_NODE_TYPE_ELSE_IF_STMT, SIDE_TYPE_LEFT);

		//Remember, we've already seen the if token, so now we just need to see an L_PAREN
		lookahead = get_next_token(fl, &parser_line_num);

		//Fail out if we don't have it
		if(lookahead.tok != L_PAREN){
			return print_and_return_error("Left parenthesis expected after else if statement", current_line);
		}

		//Push onto the stack for matching later
		push_token(&grouping_stack, lookahead);
	
		//We now need to see a valid conditional expression
		generic_ast_node_t* else_if_expression_node = logical_or_expression(fl, SIDE_TYPE_RIGHT);

		//If we see an invalid one
		if(else_if_expression_node->ast_node_type == AST_NODE_TYPE_ERR_NODE){
			return print_and_return_error("Invalid conditional expression given as else if statement condition", current_line);
		}

		//If it's not of this type or a compatible type(pointer, smaller int, etc, it is out)
		if(is_type_valid_for_conditional(expression_node->inferred_type) == FALSE){
			sprintf(info, "Type %s is invalid to be used in a conditional", expression_node->inferred_type->type_name.string);
			return print_and_return_error(info, parser_line_num);
		}

		//Following the expression we need to see a closing paren
		lookahead = get_next_token(fl, &parser_line_num);

		//If we don't see the R_Paren
		if(lookahead.tok != R_PAREN){
			return print_and_return_error("Right parenthesis expected after expression in else-if statement", current_line);
		}

		//Now let's check the stack, we need to have matching ones here
		if(pop_token(&grouping_stack).tok != L_PAREN){
			return print_and_return_error("Unmatched parenthesis detected", current_line);
		}

		//If we make it here, we should be safe to add the conditional as an expression
		add_child_node(else_if_node, else_if_expression_node);

		//Now following this, we need to see a valid compound statement
		generic_ast_node_t* else_if_compound_stmt_node = compound_statement(fl);

		//If this node fails, whole thing is bad
		if(else_if_compound_stmt_node->ast_node_type == AST_NODE_TYPE_ERR_NODE){
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
		lookahead = get_next_token(fl, &parser_line_num);
		lookahead2 = get_next_token(fl, &parser_line_num);
	}

	//If we get here, at the very least we know that lookahead2 is bad, so we'll put him back
	push_back_token(lookahead2);

	//We could've still had an else block here, so we'll check and handle it if we do
	if(lookahead.tok == ELSE){
		//We'll now handle the compound statement that comes with this
		generic_ast_node_t* else_compound_stmt = compound_statement(fl);
		
		//Let's see if it worked
		if(else_compound_stmt->ast_node_type == AST_NODE_TYPE_ERR_NODE){
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

	//Now that we're done, we'll pop this off of the stack
	pop_nesting_level(&nesting_stack);

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
 * For a direct jump here, we'll kind of cook the books by internally representing it as jump <label> when(1);
 * This greatly simplifies things on our end and allows us to greatly simplify the translation of direct jumps
 * in the CFG.
 *
 * BNF Rule: <jump-statement> ::= jump <label-identifier> {when(conditional_expression)}?;
 */
static generic_ast_node_t* jump_statement(FILE* fl){
	//Lookahead token
	lexitem_t lookahead;

	//Do we contain a defer at any point in here? If so, that is invalid because we could
	//have the defer block duplicated multiple times. As such, a label would become ambiguous
	if(nesting_stack_contains_level(&nesting_stack, NESTING_DEFER_STATEMENT) == TRUE){
		return print_and_return_error("Direct jump statements cannot be placed inside of deferred blocks", parser_line_num);
	}

	//Once we've made it, we need to see a valid label identifier
	generic_ast_node_t* label_ident = identifier(fl, SIDE_TYPE_LEFT);

	//If this failed, we're done
	if(label_ident->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		return print_and_return_error("Invalid label given to jump statement", parser_line_num);
	}

	//Allocate the jump statement
	generic_ast_node_t* jump_statement = ast_node_alloc(AST_NODE_TYPE_CONDITIONAL_JUMP_STMT, SIDE_TYPE_LEFT);

	//One last tripping point befor we create the node, we do need to see a semicolon
	lookahead = get_next_token(fl, &parser_line_num);

	//We could optionally see a conditional jump statement here with the "when" keyword
	if(lookahead.tok == WHEN){
		//Add this in as a child node to the statement
		add_child_node(jump_statement, label_ident);

		//We now need to see an L_PAREN 
		lookahead = get_next_token(fl, &parser_line_num);

		//Fail out if not
		if(lookahead.tok != L_PAREN){
			return print_and_return_error("Left parenthesis required after when statement", parser_line_num);
		}

		//Otherwise we'll add this into the grouping stack
		push_token(&grouping_stack, lookahead);

		//Now we need to see a valid conditional expression
		generic_ast_node_t* conditional = logical_or_expression(fl, SIDE_TYPE_RIGHT);

		//If this is invalid, we fail out
		if(conditional->ast_node_type == AST_NODE_TYPE_ERR_NODE){
			return conditional;
		}

		//This is a conditional so the type that we have here needs to be valid for it
		if(is_type_valid_for_conditional(conditional->inferred_type) == FALSE){
			sprintf(info, "Type %s is not valid for a conditional", conditional->inferred_type->type_name.string);
			return print_and_return_error(info, parser_line_num);
		}

		//Otherwise we're all good here, so we'll add this in as a child
		add_child_node(jump_statement, conditional);

		//We'll need to see the final closing paren here
		lookahead = get_next_token(fl, &parser_line_num);

		//If it's not an R_PAREN we're done
		if(lookahead.tok != R_PAREN){
			return print_and_return_error("Closing parenthesis required after conditional in when statement", parser_line_num);
		}

		//Pop the grouping stack and validate that we match
		if(pop_token(&grouping_stack).tok != L_PAREN){
			return print_and_return_error("Unmatched parenthesis detected", parser_line_num);
		}

		//Refresh the lookahead one last time for the semicolon search
		lookahead = get_next_token(fl, &parser_line_num);

	//Otherwise it's not a conditional, just a direct jump
	} else {
		//Add this in as a child node to the statement
		add_child_node(jump_statement, label_ident);
	
		//Rig this jump here to really be a "jump when" that just always evaluates to true
		add_child_node(jump_statement, emit_direct_constant(1));
		
	}

	//If we don't see a semicolon we bail
	if(lookahead.tok != SEMICOLON){
		return print_and_return_error("Semicolon required after jump statement", parser_line_num);
	}
	
	//Store the line number
	jump_statement->line_number = parser_line_num;

	//Add this jump statement into the queue for processing
	enqueue(&current_function_jump_statements, jump_statement);

	//Finally we'll give back the root reference
	return jump_statement;
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
	lexitem_t lookahead;	

	//We need to ensure that we're in a loop here of some kind. If we aren't then this is 
	//invalid
	if(nesting_stack_contains_level(&nesting_stack, NESTING_LOOP_STATEMENT) == FALSE){
		return print_and_return_error("Continue statements must be used inside of loops", parser_line_num);
	}

	//Once we get here, we've already seen the continue keyword, so we can make the node
	generic_ast_node_t* continue_stmt = ast_node_alloc(AST_NODE_TYPE_CONTINUE_STMT, SIDE_TYPE_LEFT);
	//Store the line number
	continue_stmt->line_number = parser_line_num;

	//Let's see what comes after this. If it's a semicol, we get right out
	lookahead = get_next_token(fl, &parser_line_num);

	//If it's a semicolon we're done
	if(lookahead.tok == SEMICOLON){
		return continue_stmt;
	}

	//Otherwise, it has to have been a when keyword, so if it's not we have an error
	if(lookahead.tok != WHEN){
		return print_and_return_error("Semicolon expected after continue statement", parser_line_num);
	}
	
	//If we get down here, we know that we are seeing a continue when statement
	//We now need to see an lparen
	lookahead = get_next_token(fl, &parser_line_num);

	//If we don't have one, it's an instant fail
	if(lookahead.tok != L_PAREN){
		return print_and_return_error("Parenthesis expected after continue when keywords", parser_line_num);
	}

	//Push to the stack for grouping
	push_token(&grouping_stack, lookahead);

	//Now we need to see a valid conditional expression
	generic_ast_node_t* expr_node = ternary_expression(fl, SIDE_TYPE_RIGHT);

	//If it failed, we also fail
	if(expr_node->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		return print_and_return_error("Invalid conditional expression given to continue when statement", parser_line_num);
	}

	//If this worked, we add it under the continue node
	add_child_node(continue_stmt, expr_node);

	//We need to now see a closing paren
	lookahead = get_next_token(fl, &parser_line_num);

	//If we don't see it fail out
	if(lookahead.tok != R_PAREN){
		return print_and_return_error("Closing paren expected after when clause",  parser_line_num);
	}

	//Check for matching next
	if(pop_token(&grouping_stack).tok != L_PAREN){
		return print_and_return_error("Unmatched parenthesis detected", parser_line_num);
	}

	//Finally if we make it all the way down here, we need to see a semicolon
	lookahead = get_next_token(fl, &parser_line_num);

	if(lookahead.tok != SEMICOLON){
		return print_and_return_error("Semicolon expected after statement", parser_line_num);
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
	lexitem_t lookahead;

	//We need to ensure that we're in a loop here of some kind. If we aren't then this is 
	//invalid
	if(nesting_stack_contains_level(&nesting_stack, NESTING_LOOP_STATEMENT) == FALSE
		&& nesting_stack_contains_level(&nesting_stack, NESTING_C_STYLE_CASE_STATEMENT) == FALSE){
	
		//Fail out here
		return print_and_return_error("Break statements must be used inside of loops or c-style case/default statements", parser_line_num);
	}

	//Once we get here, we've already seen the break keyword, so we can make the node
	generic_ast_node_t* break_stmt = ast_node_alloc(AST_NODE_TYPE_BREAK_STMT, SIDE_TYPE_LEFT);
	//Store the line number
	break_stmt->line_number = parser_line_num;

	//Let's see what comes after this. If it's a semicol, we get right out
	lookahead = get_next_token(fl, &parser_line_num);

	//If it's a semicolon we're done
	if(lookahead.tok == SEMICOLON){
		return break_stmt;
	}

	//Otherwise, it has to have been a when keyword, so if it's not we have an error
	if(lookahead.tok != WHEN){
		return print_and_return_error("Semicolon expected after break statement", parser_line_num);
	}
	
	//If we get down here, we know that we are seeing a break when statement
	//We now need to see an lparen
	lookahead = get_next_token(fl, &parser_line_num);

	//If we don't have one, it's an instant fail
	if(lookahead.tok != L_PAREN){
		return print_and_return_error("Parenthesis expected after break when keywords", parser_line_num);
	}

	//Push to the stack for grouping
	push_token(&grouping_stack, lookahead);

	//Now we need to see a valid expression
	generic_ast_node_t* expr_node = logical_or_expression(fl, SIDE_TYPE_RIGHT);

	//If it failed, we also fail
	if(expr_node->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		return print_and_return_error("Invalid conditional expression given to break when statement", parser_line_num);
	}

	//This is the first child of the if statement node
	add_child_node(break_stmt, expr_node);

	//We need to now see a closing paren
	lookahead = get_next_token(fl, &parser_line_num);

	//If we don't see it fail out
	if(lookahead.tok != R_PAREN){
		return print_and_return_error("Closing paren expected after when clause",  parser_line_num);
	}

	//Check for matching next
	if(pop_token(&grouping_stack).tok != L_PAREN){
		return print_and_return_error("Unmatched parenthesis detected", parser_line_num);
	}

	//Finally if we make it all the way down here, we need to see a semicolon
	lookahead = get_next_token(fl, &parser_line_num);

	if(lookahead.tok != SEMICOLON){
		return print_and_return_error("Semicolon expected after statement", parser_line_num);
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
 * BNF Rule: <return-statement> ::= ret {<ternary-epxression>}?;
 */
static generic_ast_node_t* return_statement(FILE* fl){
	//Lookahead token
	lexitem_t lookahead;

	//Do we contain a defer at any point in here? If so, that is invalid because we already
	//have a return. If this happens, we'll need to reject it
	if(nesting_stack_contains_level(&nesting_stack, NESTING_DEFER_STATEMENT) == TRUE){
		return print_and_return_error("Ret statements cannot be placed inside of defer blocks", parser_line_num);
	}

	//We can create the node now
	generic_ast_node_t* return_stmt = ast_node_alloc(AST_NODE_TYPE_RET_STMT, SIDE_TYPE_LEFT);

	//Now we can optionally see the semicolon immediately. Let's check if we have that
	lookahead = get_next_token(fl, &parser_line_num);

	//If we see a semicolon, we can just leave
	if(lookahead.tok == SEMICOLON){
		//If this is the case, the return type had better be void
		if(current_function->signature->internal_types.function_type->returns_void == FALSE){
			sprintf(info, "Function \"%s\" expects a return type of \"%s\", not \"void\". Empty ret statements not allowed", current_function->func_name.string, current_function->return_type->type_name.string);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			//Also print the function name
			print_function_name(current_function);
			num_errors++;
			return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
		}

		//If we get out then we're fine
		return return_stmt;

	} else {
		//If we get here, but we do expect a void return, then this is an issue
		if(current_function->signature->internal_types.function_type->returns_void == TRUE){
			sprintf(info, "Function \"%s\" expects a return type of \"void\". Use \"ret;\" for return statements in this function", current_function->func_name.string);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			//Also print the function name
			print_function_name(current_function);
			num_errors++;
			return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
		}
		//Put it back if no
		push_back_token(lookahead);
	}

	//Otherwise if we get here, we need to see a valid conditional expression
	generic_ast_node_t* expr_node = ternary_expression(fl, SIDE_TYPE_RIGHT);

	//If this is bad, we fail out
	if(expr_node->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		return print_and_return_error("Invalid expression given to return statement", parser_line_num);
	}

	//Let's do some type checking here
	if(current_function == NULL){
		return print_and_return_error("Fatal internal compiler error. Saw a return statement while current function is null", parser_line_num);
	}

	//Figure out what the final type is here
	generic_type_t* final_type = types_assignable(current_function->return_type, expr_node->inferred_type);

	//If the current function's return type is not compatible with the return type here, we'll bail out
	if(final_type == NULL){
		sprintf(info, "Function \"%s\" expects a return type of \"%s\", but was given an incompatible type \"%s\"", current_function->func_name.string, current_function->return_type->type_name.string,
		  		expr_node->inferred_type->type_name.string);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the function
		print_function_name(current_function);
		num_errors++;
		return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
	}

	//Another special case - if we're trying to pass a non-reference as a reference return, we will fail
	if(current_function->return_type->type_class == TYPE_CLASS_REFERENCE && expr_node->inferred_type->type_class != TYPE_CLASS_REFERENCE){
		return print_and_return_error("Ollie does not support implicit referencing in return statements", parser_line_num);
	}

	//If this is a constant, we'll force it to be whatever the new type is
	if(expr_node->ast_node_type == AST_NODE_TYPE_CONSTANT){
		//Set the type
		expr_node->inferred_type = final_type;

		//Coerce the constant
		perform_constant_assignment_coercion(expr_node, final_type);
	}

	//Otherwise it worked, so we'll add it as a child of the other node
	add_child_node(return_stmt, expr_node);

	//After the conditional, we just need to see a semicolon
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail case
	if(lookahead.tok != SEMICOLON){
		return print_and_return_error("Semicolon expected after return statement", parser_line_num);
	}

	//Add in the line number
	return_stmt->line_number = parser_line_num;

	//This is *always* the function's return type
	return_stmt->inferred_type = final_type;
	
	//If we have deferred statements
	if(deferred_stmts_node != NULL){
		//Then we'll duplicate
		generic_ast_node_t* deferred_stmts = duplicate_subtree(deferred_stmts_node, deferred_stmts_node->side);

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
 * A switch statement allows us to to see one or more labels defined by a certain expression. It allows
 * for the use of labeled statements followed by statements in general. We will do more static analysis
 * on this later. Like all rules in the system, this function returns the root node that it creates
 *
 * NOTE: The caller has already consumed the switch keyword by the time we get here
 *
 * BNF Rule: <switch-statement> ::= switch on( <logical-or-expression> ) from(<constant>, <constant>) { {<case-statement | default-statement>}+ }
 */
static generic_ast_node_t* switch_statement(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	//Lookahead token
	lexitem_t lookahead;
	//By default we have not found one of these
	u_int8_t found_default_clause = FALSE;
	//Is this a c-style switch statement? By default, we have a -1 here for "no value"
	int8_t is_c_style = -1;

	//Once we get here, we can allocate the root level node
	//NOTE: we may actually switch the class to a c-style switch statement here if we
	//find a c-style node. All of our processing depends on what the first thing that we see
	//looks like
	generic_ast_node_t* switch_stmt_node = ast_node_alloc(AST_NODE_TYPE_SWITCH_STMT, SIDE_TYPE_LEFT);

	//We will find these throughout our search
	//Set the upper bound to be int_min
	switch_stmt_node->upper_bound = INT_MIN;
	//Set the lower bound to be int_max 
	switch_stmt_node->lower_bound = INT_MAX;

	//Now we must see an lparen
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail case
	if(lookahead.tok != L_PAREN){
		return print_and_return_error("Left parenthesis expected after on keyword", current_line);
	}

	//Push to stack for later matching
	push_token(&grouping_stack, lookahead);

	//Now we must see a valid ternary-level expression
	generic_ast_node_t* expr_node = ternary_expression(fl, SIDE_TYPE_RIGHT);

	//If we see an invalid one we fail right out
	if(expr_node->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		return print_and_return_error("Invalid conditional expression provided to switch on", current_line);
	}
	
	//For a switch statement, we need an enum or some other kind of numeric type to switch based on. We cannot switch on
	//types like floats, etc
	
	//Grab the type info out
	generic_type_t* type = expr_node->inferred_type;

	//Let's see what kind of type we have here. If it isn't a basic type, it MUST be an enum
	if(type->type_class != TYPE_CLASS_BASIC){
		//Error out here
		if(type->type_class != TYPE_CLASS_ENUMERATED){
			sprintf(info, "Type \"%s\" cannot be switched", type->type_name.string);
			return print_and_return_error(info, expr_node->line_number);
		}
	//Otherwise, it essentially needs to be an int or a char. Nothing else here is "switchable"	
	} else {
		//Grab the basic type
		ollie_token_t basic_type = type->basic_type_token;

		//It needs to be an int or char
		if(basic_type == VOID || basic_type == F32 || basic_type == F64){
			sprintf(info, "Type \"%s\" cannot be switched", type->type_name.string);
			return print_and_return_error(info, expr_node->line_number);
		}
	}

	//Since we know it's valid, we can add this in as a child
	add_child_node(switch_stmt_node, expr_node);

	//Assign this to be the switch statement's inferred type, because it's what we'll be switching on
	switch_stmt_node->inferred_type = type;

	//Now we must see a closing paren
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail case
	if(lookahead.tok != R_PAREN){
		return print_and_return_error("Right parenthesis expected after expression in switch statement", current_line);
	}

	//Check to make sure that the parenthesis match up
	if(pop_token(&grouping_stack).tok != L_PAREN){
		return print_and_return_error("Unmatched parenthesis detected", current_line);
	}

	//Now we must see an lcurly to begin the actual block
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail case
	if(lookahead.tok != L_CURLY){
		return print_and_return_error("Left curly brace expected after expression", current_line);
	}

	//We will declare a new lexical scope here
	initialize_variable_scope(variable_symtab);
	initialize_type_scope(type_symtab);

	//Push to stack for later matching
	push_token(&grouping_stack, lookahead);

	//We'll need to keep track of whether or not we have any duplicated values. As such, we'll keep an array
	//of all the values that we do have. Since we can only have 1024 values, this array need only be 1024
	//long. Every time we see a value in a case statement, we'll need to cross reference it with the
	//values in here
	int32_t values[MAX_SWITCH_RANGE];

	//Wipe the entire thing so they're all 0's(FALSE)
	memset(values, 0, MAX_SWITCH_RANGE * sizeof(int32_t));

	//Now we can see as many expressions as we'd like. We'll keep looking for expressions so long as
	//our lookahead token is not an R_CURLY. We'll use a do-while for this, because Ollie language requires
	//that switch statements have at least one thing in them

	//Seed our search here
	lookahead = get_next_token(fl, &parser_line_num);
	//Is this statement occupied? Set this flag if no
	u_int8_t is_empty = TRUE;
	//Handle our statement here
	generic_ast_node_t* stmt;

	//What is the current case statement value that we're on?. This is 
	//used in the values[] above
	int32_t values_max_index = 0;

	//So long as we don't see a right curly
	while(lookahead.tok != R_CURLY){
		//Switch by the lookahead
		switch(lookahead.tok){
			//We can see a case statement here
			case CASE:
				//Handle a case statement here. We'll need to pass
				//the node in because of the type checking that we do
				stmt = case_statement(fl, switch_stmt_node, values, &values_max_index);

				//Go based on what our class here
				switch(stmt->ast_node_type){
					//C-style case statement
					case AST_NODE_TYPE_C_STYLE_CASE_STMT:
						//The -1 would mean that it's not been declared yet.
						//As such, since this is the first thing that we're seeing,
						//we'll set this to be TRUE
						if(is_c_style == -1){
							is_c_style = TRUE;

						//Otherwise, if this has already been declared as
						//not being a c-style switch statement, we have an error
						//here
						} else if(is_c_style == FALSE){
							return print_and_return_error("C-style and Ollie-style case/default statements cannot be combined in the same switch statement", parser_line_num);
						}

						//Otherwise we should be set here, so break out
						break;

					//Regular ollie style case statement
					case AST_NODE_TYPE_CASE_STMT:
						//The -1 would mean that it's not been declared yet.
						//As such, since this is the first thing that we're seeing,
						//we'll set this to be FALSE 
						if(is_c_style == -1){
							is_c_style = FALSE;
						}

						//Otherwise, if this has already been declared to be a c-style switch statement, then we're
						//attempting to mix and match here. This is also an error
						else if(is_c_style == TRUE){
							return print_and_return_error("C-style and Ollie-style case/default statements cannot be combined in the same switch statement", parser_line_num);
						}

						//Otherwise we should be set here, so break out
						break;


					//It's already an error, just send it up
					case AST_NODE_TYPE_ERR_NODE:
						return stmt;
					//We've hit some weird error here, so we'll bail out
					default:
						return print_and_return_error("Switch statements may only be occupied by \"case\" or default statements", parser_line_num);
				}

				//No longer empty
				is_empty = FALSE;

				break;

			//Handle the default case
			case DEFAULT:
				//Double default - this is invalid and will lead to a compiler error
				if(found_default_clause == TRUE){
					return print_and_return_error("Switch statements may only have one default clause", parser_line_num);
				}

				//Handle a default statement
				stmt = default_statement(fl);

				//Go based on what our class here
				switch(stmt->ast_node_type){
					//C-style default statement
					case AST_NODE_TYPE_C_STYLE_DEFAULT_STMT:
						//The -1 would mean that it's not been declared yet.
						//As such, since this is the first thing that we're seeing,
						//we'll set this to be TRUE
						if(is_c_style == -1){
							is_c_style = TRUE;

						//Otherwise, if this has already been declared as
						//not being a c-style switch statement, we have an error
						//here
						} else if(is_c_style == FALSE){
							return print_and_return_error("C-style and Ollie-style case/default statements cannot be combined in the same switch statement", parser_line_num);
						}

						//We've found it
						found_default_clause = TRUE;

						//Otherwise we should be set here, so break out
						break;

					//Regular ollie style default statement
					case AST_NODE_TYPE_DEFAULT_STMT:
						//The -1 would mean that it's not been declared yet.
						//As such, since this is the first thing that we're seeing,
						//we'll set this to be FALSE 
						if(is_c_style == -1){
							is_c_style = FALSE;
						}

						//Otherwise, if this has already been declared to be a c-style switch statement, then we're
						//attempting to mix and match here. This is also an error
						else if(is_c_style == TRUE){
							return print_and_return_error("C-style and Ollie-style case/default statements cannot be combined in the same switch statement", parser_line_num);
						}

						//No longer empty
						is_empty = FALSE;

						//We've found it
						found_default_clause = TRUE;

						//Otherwise we should be set here, so break out
						break;

					//It's already an error, just send it up
					case AST_NODE_TYPE_ERR_NODE:
						return stmt;
					//We've hit some weird error here, so we'll bail out
					default:
						return print_and_return_error("Switch statements may only be occupied by \"case\" or default statements", parser_line_num);
				}

				break;

			//Fail out here -- something went wrong
			default:
				return print_and_return_error("\"case\" or \"default\" keywords expected", parser_line_num);
		}

		//If we get here we know it worked, so we can add it in as a child
		add_child_node(switch_stmt_node, stmt);
		//Refresh the lookahead token
		lookahead = get_next_token(fl, &parser_line_num);
	}

	//If we have an entirely empty switch statement, it's a failure
	if(is_empty == TRUE){
		return print_and_return_error("Switch statements with no cases are not allowed", current_line);
	}

	//Do we have a type that is eligible for a "exhaustive switch"? If so, this would
	//mean that we may not need a default clause at all
	if(is_exhaustive_switch_eligible(type) == TRUE){
		//Should we check for exhaustiveness here? Assume true
		//unless told otherwise
		u_int8_t check_for_exhaustive = TRUE;

		//Now go based on what kind of type we have here
		switch(type->type_class){
			//For basic types, we need to check if we're getting a full range
			case TYPE_CLASS_BASIC:
				switch(type->basic_type_token){
					case U8:
					case CHAR:
						//If we want to check for exhaustive, we'll need 
						//the low and high to be the lower and upper bounds
						if(switch_stmt_node->lower_bound != 0 
							|| switch_stmt_node->upper_bound != UINT8_MAX){

							//Don't bother checking
							check_for_exhaustive = FALSE;
						}

						break;
					
					case I8:
						//If we want to check for exhaustive, we'll need 
						//the low and high to be the lower and upper bounds
						if(switch_stmt_node->lower_bound != INT8_MIN 
							|| switch_stmt_node->upper_bound != INT8_MAX){

							//Don't bother checking
							check_for_exhaustive = FALSE;
						}
						
						break;

					//Unreachable so if we hit this get out
					default:
						printf("Fatal internal compiler error. Unreachable path hit in switch statement validator\n");
						exit(1);
				}

				//Are we going to bother checking to see if it's exhaustive?
				if(check_for_exhaustive == TRUE){
					//Did we find a gap? assume no to start
					u_int8_t gap_found = FALSE;

					//Run through the list and ensure that there are no gaps between the values
					for(int32_t i = 1; i < values_max_index; i++){
						//This is a gap, immediate exit
						if(values[i] - values[i - 1] != 1){
							gap_found = TRUE;
							break;
						}
					}

					//If there is no gap, then this is exhaustive and we do *not* need 
					//a default clause
					if(gap_found == FALSE){
						//If we haven't found a default clause, it's a failure
						if(found_default_clause == TRUE){
							return print_and_return_error("\"default\" clause in exhaustive switch is unreachable", current_line);
						}	

					//Otherwise it's not exhaustive, so we do
					} else {
						//If we haven't found a default clause, it's a failure
						if(found_default_clause == FALSE){
							return print_and_return_error("Non-exhaustive switch statements are required to have a \"default\" clause", current_line);
						}	
					}

				//If not, then this *needs* to have a default statement
				} else {
					//If we haven't found a default clause, it's a failure
					if(found_default_clause == FALSE){
						return print_and_return_error("Non-exhaustive switch statements are required to have a \"default\" clause", current_line);
					}	
				}

				break;

			//Go through our enum type here
			case TYPE_CLASS_ENUMERATED:
				//If we don't have these, then we already know we can't go further
				if(switch_stmt_node->lower_bound != type->min_enum_value
					|| switch_stmt_node->upper_bound != type->max_enum_value){
					check_for_exhaustive = FALSE;
				}

				//Are we going to bother checking to see if it's exhaustive?
				if(check_for_exhaustive == TRUE){
					//Did we find a gap? assume no to start
					u_int8_t gap_found = FALSE;

					//Run through the list and ensure that there are no gaps between the values
					for(int32_t i = 1; i < values_max_index; i++){
						//This is a gap, immediate exit
						if(values[i] - values[i - 1] != 1){
							gap_found = TRUE;
							break;
						}
					}

					//If there is no gap, then this is exhaustive and we do *not* need 
					//a default clause
					if(gap_found == FALSE){
						//If we haven't found a default clause, it's a failure
						if(found_default_clause == TRUE){
							return print_and_return_error("\"default\" clause in exhaustive switch is unreachable", current_line);
						}	

					//Otherwise it's not exhaustive, so we do
					} else {
						//If we haven't found a default clause, it's a failure
						if(found_default_clause == FALSE){
							return print_and_return_error("Non-exhaustive switch statements are required to have a \"default\" clause", current_line);
						}	
					}

				//If not, then this *needs* to have a default statement
				} else {
					//If we haven't found a default clause, it's a failure
					if(found_default_clause == FALSE){
						return print_and_return_error("Non-exhaustive switch statements are required to have a \"default\" clause", current_line);
					}	
				}

				break;
				
			//We should never hit this, so if we do get out
			default:
				printf("Fatal internal compiler error. Unreachable path hit in switch statement validator\n");
				exit(1);
		}

	//Otherwise it's not even exhaustive switch eligible, so a default clause is a must in Ollie
	} else {
		//If we haven't found a default clause, it's a failure
		if(found_default_clause == FALSE){
			return print_and_return_error("Non-exhaustive switch statements are required to have a \"default\" clause", current_line);
		}	
	}

	//If we do have a c-style switch statement here, we'll need to redefine the type
	//that the origin switch node is
	if(is_c_style == TRUE){
		switch_stmt_node->ast_node_type = AST_NODE_TYPE_C_STYLE_SWITCH_STMT; 
	}
	
	//By the time we reach this, we should have seen a right curly
	//However, we could still have matching issues, so we'll check for that here
	if(pop_token(&grouping_stack).tok != L_CURLY){
		return print_and_return_error("Unmatched curly braces detected", current_line);
	}

	//Return the line number
	switch_stmt_node->line_number = current_line;

	//Now that we're done, we will remove this variable scope
	finalize_variable_scope(variable_symtab);
	finalize_type_scope(type_symtab);

	//If we make it here, all went well
	return switch_stmt_node;
}


/**
 * A while statement simply ensures that the check is executed before the body. Like all other rules, this
 * function returns a reference to the root node of the subtree that it creates
 *
 * NOTE: By the time that we make it here, we assume that we have already seen the while keyword
 *
 * BNF Rule: <while-statement> ::= while( <logical-or-expression> ) <compound-statement> 
 */
static generic_ast_node_t* while_statement(FILE* fl){
	//The lookahead token
	lexitem_t lookahead;
	//Freeze the line number
	u_int16_t current_line = parser_line_num;

	//Push the looping statement onto here
	push_nesting_level(&nesting_stack, NESTING_LOOP_STATEMENT);

	//First create the actual node
	generic_ast_node_t* while_stmt_node = ast_node_alloc(AST_NODE_TYPE_WHILE_STMT, SIDE_TYPE_LEFT);

	//We already have seen the while keyword, so now we need to see parenthesis surrounding a conditional expression
	lookahead = get_next_token(fl, &parser_line_num);
	
	//Fail out if we don't see
	if(lookahead.tok != L_PAREN){
		return print_and_return_error("Left parenthesis expected after while keyword", parser_line_num);
	}

	//Push it to the stack for later matching
	push_token(&grouping_stack, lookahead);

	//Now we need to see a valid conditional block in here
	generic_ast_node_t* conditional_expr = logical_or_expression(fl, SIDE_TYPE_RIGHT);

	//Fail out if this happens
	if(conditional_expr->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		return print_and_return_error("Invalid expression in while statement",  parser_line_num);
	}

	//If it's not of this type or a compatible type(pointer, smaller int, etc, it is out)
	if(is_type_valid_for_conditional(conditional_expr->inferred_type) == FALSE){
		sprintf(info, "Type %s is not valid for a conditional", conditional_expr->inferred_type->type_name.string);
		return print_and_return_error(info, parser_line_num);
	}

	//Otherwise we know it's good so we can add it in as a child
	add_child_node(while_stmt_node, conditional_expr);

	//After this point we need to see a right paren
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail if we don't see it
	if(lookahead.tok != R_PAREN){
		return print_and_return_error("Expected right parenthesis after conditional expression",  parser_line_num);
	}

	//We also need to check for matching
	if(pop_token(&grouping_stack).tok != L_PAREN){
		return print_and_return_error("Unmatched parenthesis detected", parser_line_num);
	}

	//Following this, we need to see a valid compound statement, and then we're done
	generic_ast_node_t* compound_stmt_node = compound_statement(fl);

	//If this is invalid we fail
	if(compound_stmt_node->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		return print_and_return_error("Invalid compound statement in while expression", parser_line_num);
	}

	//Otherwise we'll add it in as a child
	add_child_node(while_stmt_node, compound_stmt_node);
	//Store the current line number
	while_stmt_node->line_number = current_line;

	//And now that we're done, pop this off of the nesting stack
	pop_nesting_level(&nesting_stack);

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
	//Freeze the current line number
	u_int16_t current_line = parser_line_num;
	//Lookahead token
	lexitem_t lookahead;

	//Push this nesting level onto the stack
	push_nesting_level(&nesting_stack, NESTING_LOOP_STATEMENT);

	//Let's first create the overall global root node
	generic_ast_node_t* do_while_stmt_node = ast_node_alloc(AST_NODE_TYPE_DO_WHILE_STMT, SIDE_TYPE_LEFT);

	//Remember by the time that we've gotten here, we have already seen the do keyword
	//Let's first find a valid compound statement
	generic_ast_node_t* compound_stmt = compound_statement(fl);

	//If we fail, then we are done here
	if(compound_stmt->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		return print_and_return_error("Invalid compound statement given to do-while statement", current_line);
	}

	//Otherwise we know that it was valid, so we can add it in as a child of the root
	add_child_node(do_while_stmt_node, compound_stmt);

	//Once we get past the compound statement, we need to now see the while keyword
	lookahead = get_next_token(fl, &parser_line_num);

	//If we don't see it, instant failure
	if(lookahead.tok != WHILE){
		return print_and_return_error("Expected while keyword after block in do-while statement", parser_line_num);
	}
	
	//Once we've made it here, we now need to see a left paren
	lookahead = get_next_token(fl, &parser_line_num);
	
	//Fail out if we don't see
	if(lookahead.tok != L_PAREN){
		return print_and_return_error("Left parenthesis expected after while keyword", parser_line_num);
	}

	//Push it to the stack for later matching
	push_token(&grouping_stack, lookahead);

	//Now we need to see a valid conditional block in here
	generic_ast_node_t* expr_node = logical_or_expression(fl, SIDE_TYPE_RIGHT);

	//Fail out if this happens
	if(expr_node->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		return print_and_return_error("Invalid expression in while part of do-while statement",  parser_line_num);
	}

	//If it's not of this type or a compatible type(pointer, smaller int, etc, it is out)
	if(is_type_valid_for_conditional(expr_node->inferred_type) == FALSE){
		sprintf(info, "Type %s is invalid for a conditional", expr_node->inferred_type->type_name.string);
		return print_and_return_error(info, parser_line_num);
	}

	//Otherwise we know it's good so we can add it in as a child
	add_child_node(do_while_stmt_node, expr_node);

	//After this point we need to see a right paren
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail if we don't see it
	if(lookahead.tok != R_PAREN){
		return print_and_return_error("Expected right parenthesis after conditional expression",  parser_line_num);
	}

	//We also need to check for matching
	if(pop_token(&grouping_stack).tok != L_PAREN){
		return print_and_return_error("Unmatched parenthesis detected", parser_line_num);
	}

	//Finally we need to see a semicolon
	lookahead = get_next_token(fl, &parser_line_num);

	//If we don't see one, final chance to fail
	if(lookahead.tok != SEMICOLON){
		return print_and_return_error("Semicolon expected at the end of do while statement", parser_line_num);
	}
	//Store the line number
	do_while_stmt_node->line_number = current_line;

	//Now that we're done, remove this from the stack
	pop_nesting_level(&nesting_stack);
	
	//Otherwise if we made it here, everything went well
	return do_while_stmt_node;
}


/**
 * A for statement is the classic for loop that you'd expect. Like all other rules, this rule returns
 * a reference to the root of the subtree that it creates
 * 
 * NOTE: By the the time we get here, we assume that we've already seen the "for" keyword
 *
 * BNF Rule: <for-statement> ::= for( {<assignment-expression> | <let-statement>}? ; <logical-or-expression> ; {<assignment-expression>}? ) <compound-statement>
 */
static generic_ast_node_t* for_statement(FILE* fl){
	//Freeze the current line number
	u_int16_t current_line = parser_line_num; 
	//Lookahead token
	lexitem_t lookahead;

	//Push this nesting level onto the stack
	push_nesting_level(&nesting_stack, NESTING_LOOP_STATEMENT);

	//We've already seen the for keyword, so let's create the root level node
	generic_ast_node_t* for_stmt_node = ast_node_alloc(AST_NODE_TYPE_FOR_STMT, SIDE_TYPE_LEFT);

	//We now need to first see a left paren
 	lookahead = get_next_token(fl, &parser_line_num);

	//If we don't see it, instantly fail out
	if(lookahead.tok != L_PAREN){
		return print_and_return_error("Left parenthesis expected after for keyword", parser_line_num);
	}

	//Push to the stack for later matching
	push_token(&grouping_stack, lookahead);

	/**
	 * Important note: The parenthesized area of a for statement represents a new lexical scope
	 * for variables. As such, we will initialize a new variable scope when we get here
	 */
	initialize_variable_scope(variable_symtab);

	//Now we have the option of seeing an assignment expression, a let statement, or nothing
	lookahead = get_next_token(fl, &parser_line_num);

	//We could also see the let keyword for a let_stmt
	if(lookahead.tok == LET){
		//On the contrary, the let statement rule assumes that let has already been consumed, so we won't
		//put it back here, we'll just call the rule
		generic_ast_node_t* let_stmt = let_statement(fl, FALSE);

		//If it fails, we also fail
		if(let_stmt->ast_node_type == AST_NODE_TYPE_ERR_NODE){
			return print_and_return_error("Invalid let statement given to for loop", current_line);
		}

		//Create the wrapper node for CFG creation later on
		generic_ast_node_t* for_loop_cond_node = ast_node_alloc(AST_NODE_TYPE_FOR_LOOP_CONDITION, SIDE_TYPE_LEFT);
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
		if(asn_expr->ast_node_type == AST_NODE_TYPE_ERR_NODE){
			return print_and_return_error("Invalid assignment expression given to for loop", current_line);
		}

		//This actually must be an assignment expression, so if it isn't we fail 
		if(asn_expr->ast_node_type != AST_NODE_TYPE_ASNMNT_EXPR){
			return print_and_return_error("Invalid assignment expression given to for loop", current_line);
		}

		//Create the wrapper node for CFG creation later on
		generic_ast_node_t* for_loop_cond_node = ast_node_alloc(AST_NODE_TYPE_FOR_LOOP_CONDITION, SIDE_TYPE_LEFT);
		//Add this in as a child
		add_child_node(for_loop_cond_node, asn_expr);

		//Otherwise it worked, so we'll add it in as a child
		add_child_node(for_stmt_node, for_loop_cond_node);

		//We'll refresh the lookahead for the eventual next step
		lookahead = get_next_token(fl, &parser_line_num);

		//The assignment expression won't check semicols for us, so we'll do it here
		if(lookahead.tok != SEMICOLON){
			return print_and_return_error("Semicolon expected in for statement declaration", parser_line_num);
		}

	//Just add in a blank node as a placeholder
	} else {
		generic_ast_node_t* for_loop_cond_node = ast_node_alloc(AST_NODE_TYPE_FOR_LOOP_CONDITION, SIDE_TYPE_LEFT);
		add_child_node(for_stmt_node, for_loop_cond_node);
	}

	//Now we're in the middle of the for statement. We can optionally see a conditional expression here
	lookahead = get_next_token(fl, &parser_line_num);

	//If it's not a semicolon, we need to see a valid conditional expression
	if(lookahead.tok != SEMICOLON){
		//Push whatever it is back
		push_back_token(lookahead);

		//Let this rule handle it
		generic_ast_node_t* expr_node = logical_or_expression(fl, SIDE_TYPE_RIGHT);

		//If it fails, we fail too
		if(expr_node->ast_node_type == AST_NODE_TYPE_ERR_NODE){
			return print_and_return_error("Invalid conditional expression in for loop middle", parser_line_num);
		}

		//Create the wrapper node for CFG creation later on
		generic_ast_node_t* for_loop_cond_node = ast_node_alloc(AST_NODE_TYPE_FOR_LOOP_CONDITION, SIDE_TYPE_LEFT);
		//Add this in as a child
		add_child_node(for_loop_cond_node, expr_node);

		//Otherwise it did work, so we'll add it as a child node
		add_child_node(for_stmt_node, for_loop_cond_node);

		//Now once we get here, we need to see a valid semicolon
		lookahead = get_next_token(fl, &parser_line_num);
	
		//If it isn't one, we fail out
		if(lookahead.tok != SEMICOLON){
			return print_and_return_error("Semicolon expected after conditional expression in for loop", parser_line_num);
		}

	//Create a blank node as a placeholder
	} else {
		return print_and_return_error("For loops must have the second condition occupied", parser_line_num);
	}

	//Once we make it here, we know that either the inside was blank and we saw a semicolon or it wasn't and we saw a valid conditional 
	
	//As our last step, we can see another conditional expression. If the lookahead isn't a rparen, we must see one
	lookahead = get_next_token(fl, &parser_line_num);

	//If it isn't an R_PAREN
	if(lookahead.tok != R_PAREN){
		//Put it back
		push_back_token(lookahead);

		//We now must see a valid conditional
		//Let this rule handle it
		generic_ast_node_t* expr_node = assignment_expression(fl);

		//If it fails, we fail too
		if(expr_node->ast_node_type == AST_NODE_TYPE_ERR_NODE){
			return print_and_return_error("Invalid conditional expression in for loop", parser_line_num);
		}

		//Create the wrapper node for CFG creation later on
		generic_ast_node_t* for_loop_cond_node = ast_node_alloc(AST_NODE_TYPE_FOR_LOOP_CONDITION, SIDE_TYPE_LEFT);
		//Add this in as a child
		add_child_node(for_loop_cond_node, expr_node);

		//Otherwise it did work, so we'll add it as a child node
		add_child_node(for_stmt_node, for_loop_cond_node);

		//We'll refresh the lookahead for our search here
		lookahead = get_next_token(fl, &parser_line_num);
	//Create a blank node here as a placeholder
	} else {
		generic_ast_node_t* for_loop_cond_node = ast_node_alloc(AST_NODE_TYPE_FOR_LOOP_CONDITION, SIDE_TYPE_LEFT);
		add_child_node(for_stmt_node, for_loop_cond_node);
	}

	//Now if we make it down here no matter what it must be an R_Paren
	if(lookahead.tok != R_PAREN){
		return print_and_return_error("Right parenthesis expected after for loop declaration", parser_line_num);
	}

	//Now check for matching
	if(pop_token(&grouping_stack).tok != L_PAREN){
		return print_and_return_error("Unmatched parenthesis detected", parser_line_num);
	}
	
	//Now that we're all done, we need to see a valid compound statement
	generic_ast_node_t* compound_stmt_node = compound_statement(fl);

	//If it's invalid, we'll fail out here
	if(compound_stmt_node->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		//No error message, just pass the failure up
		return compound_stmt_node;
	}

	//Otherwise if we make it here, we know that it worked so we'll add it as a child
	add_child_node(for_stmt_node, compound_stmt_node);

	//At the end, we'll finalize the lexical scope
	finalize_variable_scope(variable_symtab);
	//Store the line number
	for_stmt_node->line_number = current_line;

	//Now that we're done, pop this off of the stack
	pop_nesting_level(&nesting_stack);

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
	lexitem_t lookahead;

	//We must first see a left curly
	lookahead = get_next_token(fl, &parser_line_num);
	
	//If we don't see one, we fail out
	if(lookahead.tok != L_CURLY){
		return print_and_return_error("Left curly brace required at beginning of compound statement", parser_line_num);
	}

	//Push onto the grouping stack so we can check matching
	push_token(&grouping_stack, lookahead);

	//Now if we make it here, we're safe to create the actual node
	generic_ast_node_t* compound_stmt_node = ast_node_alloc(AST_NODE_TYPE_COMPOUND_STMT, SIDE_TYPE_LEFT);
	//Store the line number here
	compound_stmt_node->line_number = parser_line_num;

	//Begin a new lexical scope for types and variables
	initialize_type_scope(type_symtab);
	initialize_variable_scope(variable_symtab);

	//Now we can keep going until we see a closing curly
	//We'll seed the search
	lookahead = get_next_token(fl, &parser_line_num);

	//So long as we don't reach the end
	while(lookahead.tok != R_CURLY){
		//Put whatever we saw back
		push_back_token(lookahead);
		
		//We now need to see a valid statement that is allowed inside of a case block
		generic_ast_node_t* stmt_node = statement(fl);

		//If this is null, which is possible, we'll just move along
		//to the next one
		if(stmt_node == NULL){
			//Refresh the lookahead
			lookahead = get_next_token(fl, &parser_line_num);
			continue;
		}

		//If it's invalid we'll pass right through, no error printing
		if(stmt_node->ast_node_type == AST_NODE_TYPE_ERR_NODE){
			//Send it right back
			return stmt_node;
		}

		//add it as a child node
		add_child_node(compound_stmt_node, stmt_node);

		//Refresh the lookahead
		lookahead = get_next_token(fl, &parser_line_num);
	}

	//Once we've escaped out of the while loop, we know that the token we currently have
	//is an R_CURLY
	//We still must check for matching
	if(pop_token(&grouping_stack).tok != L_CURLY){
		return print_and_return_error("Unmatched curly braces detected", parser_line_num);
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
	lexitem_t lookahead;

	//We must first see an opening curly
	lookahead = get_next_token(fl, &parser_line_num);

	//If we don't see one, we fail
	if(lookahead.tok != L_CURLY){
		return print_and_return_error("Assembly insertion statements must be wrapped in curly braces({})", parser_line_num);
	}

	//Let's warn the user. Assembly inline statements are a great way to shoot yourself in the foot
	print_parse_message(INFO, "Assembly inline statements are not analyzed by OC. Whatever is written will be executed verbatim. Please double check your assembly statements.", parser_line_num);

	//Otherwise we're presumably good, so we can start hunting for assembly statements
	generic_ast_node_t* assembly_node = ast_node_alloc(AST_NODE_TYPE_ASM_INLINE_STMT, SIDE_TYPE_LEFT);

	//Create the memory for the assembly
	assembly_node->string_value = dynamic_string_alloc();

	//Store this too
	assembly_node->line_number = parser_line_num;

	//We keep going here as long as we don't see the closing curly brace
	lookahead = get_next_token(fl, &parser_line_num);

	//So long as we don't see this
	while(lookahead.tok != R_CURLY){
		//Put it back
		push_back_token(lookahead);

		//We'll now need to consume an assembly statement
		lookahead = get_next_assembly_statement(fl);

		//If it's an error, we'll fail out here
		if(lookahead.tok == ERROR){
			return print_and_return_error("Unable to parse assembly statement. Did you enclose the whole block in curly braces({})?", parser_line_num);
		}

		//Concatenate this in
		dynamic_string_concatenate(&(assembly_node->string_value), lookahead.lexeme.string);

		//Add the newline character for readability
		dynamic_string_add_char_to_back(&(assembly_node->string_value), '\n');

		//Now we'll refresh the lookahead token
		lookahead = get_next_token(fl, &parser_line_num);
	}

	//Now we just need to see one last thing -- the closing semicolon
	lookahead = get_next_token(fl, &parser_line_num);

	if(lookahead.tok != SEMICOLON){
		return print_and_return_error("Expected semicolon after assembly statement", parser_line_num);
	}

	//Once we escape out here, we've seen the whole thing, so we're done
	return assembly_node;
}


/**
 * A defer statement allows users to defer execution until after a function occurs
 *
 * Remember: By the time that we get here, we will have already seen the defer keyword
 *
 * <defer-statement> ::= defer <compound statement>
 */
static generic_ast_node_t* defer_statement(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;

	//If we see any kind of invalid nesting here, we'll need to fail out. Defer
	//statements can only be nested inside of a function, and nothing else. So, if
	//the very first token that we see here is not a function, we're immediately
	//failing out of this
	if(peek_nesting_level(&nesting_stack) != NESTING_FUNCTION){
		return print_and_return_error("Defer statements must be in the top lexical scope of a function", parser_line_num);
	}

	//Push this on as a nesting level
	push_nesting_level(&nesting_stack, NESTING_DEFER_STATEMENT);

	//Now if we see that this is NULL, we'll allocate here
	if(deferred_stmts_node == NULL){
		deferred_stmts_node = ast_node_alloc(AST_NODE_TYPE_DEFER_STMT, SIDE_TYPE_LEFT);
	}

	//We now expect to see a compound statement
	generic_ast_node_t* compound_stmt_node = compound_statement(fl);

	//If this fails, we bail
	if(compound_stmt_node->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		return print_and_return_error("Invalid compound statement given to defer statement", current_line);
	}

	//Otherwise it was valid, so we have another child for this overall deferred statement
	add_child_node(deferred_stmts_node, compound_stmt_node);

	//And pop it off now that we're done
	pop_nesting_level(&nesting_stack);

	//And give back nothing, we're all set
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
	lexitem_t lookahead;

	//We just need to see a semicolon now
	lookahead = get_next_token(fl, &parser_line_num);

	//If it isn't a semicolon, we error out
	if(lookahead.tok != SEMICOLON){
		return print_and_return_error("Semicolon required after idle keyword", parser_line_num);
	}

	//Create and populate the node
	generic_ast_node_t* idle_statement = ast_node_alloc(AST_NODE_TYPE_IDLE_STMT, SIDE_TYPE_LEFT);
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
	lexitem_t lookahead = get_next_token(fl, &parser_line_num);
	//Status variable for certain rules
	u_int8_t status;


	//Switch based on the token
	switch(lookahead.tok){
		//Declare or let, we'll let that rule handle it
		case DECLARE:
		case LET:
			//We'll let the actual rule handle it, so push the token back
			push_back_token(lookahead);

			//We now need to see a valid version
			return declaration(fl, FALSE);

		//Type definition
		case DEFINE:
			//Call the helper
			status = definition(fl);

			//If it's bad, we'll return an error node
			if(status == FAILURE){
				return print_and_return_error("Invalid definition statement", parser_line_num);
			}

			//Otherwise we'll just return null, the caller will know what to do with it
			return NULL;
			
		//Type aliasing
		case ALIAS:
			//Call the helper
			status = alias_statement(fl);

			//If it's bad, we'll return an error node
			if(status == FAILURE){
				return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
			}

			//Otherwise we'll just return null, the caller will know what to do with it
			return NULL;
		
		//If we see the pound symbol, we know that we are declaring a label
		case POUND:
			//Just return whatever the rule gives us
			return labeled_statement(fl);

		//We're seeing a compound statement
		case L_CURLY:
			//The rule relies on it, so put it back
			push_back_token(lookahead);

			//Return whatever the rule gives us
			return compound_statement(fl);
	
		//If we see for, we are seeing a for statement
		case FOR:
			//This rule relies on for already being consumed, so we won't put it back
			return for_statement(fl);

		//We can see a defer statement
		case DEFER:
			//This rule relies on the defer keyword already being consumed, so we won't put it back
			return defer_statement(fl);

		//While statement
		case WHILE:
			//This rule relies on while already being consumed, so we won't put it back
			return while_statement(fl);

		//Idle statement
		case IDLE:
			//This rule just gives back an idle statement
			return idle_statement(fl);

		//Do while statement
		case DO:
			//This rule relies on do already being consumed, so we won't put it back
			return do_while_statement(fl);

		//Switch statement
		case SWITCH:
			//This rule relies on switch already being consumed, so we won't put it back
			return switch_statement(fl);

		//If statement
		case IF:
			//This rule relies on if already being consumed, so we won't put it back
			return if_statement(fl);

		//Handle a direct jump statement
		case JUMP:
			return jump_statement(fl);

		//Handle a return statement
		case RETURN:
			return return_statement(fl);

		//Handle a break statement
		case BREAK:
			return break_statement(fl);

		//And a continue statement
		case CONTINUE:
			return continue_statement(fl);

		//Inline assembly statement
		case ASM:
			return assembly_inline_statement(fl);
		
		//Replace statement is an error here
		case REPLACE:
			return print_and_return_error("Replace statements have global effects, and therefore must be declared in the global scope", parser_line_num);

		//By default push this back and return an expression statement
		default:
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
	lexitem_t lookahead;
	//Root level variable for the default compound statement
	generic_ast_node_t* default_compound_statement;

	//If we see default, we can just make the default node. We may change this class later, but this
	//will do for now
	generic_ast_node_t* default_stmt = ast_node_alloc(AST_NODE_TYPE_DEFAULT_STMT, SIDE_TYPE_LEFT);

	//All that we need to see now is a colon
	lookahead = get_next_token(fl, &parser_line_num);

	//Here is the area where we're able to differentiate between an ollie style case
	//statement(-> {}) and a C-style case statement with fallthrough, etc.
	switch(lookahead.tok){
		case ARROW:
			//Record that we're in a default statement in here
			push_nesting_level(&nesting_stack, NESTING_CASE_STATEMENT);

			//We'll let the helper deal with it
			default_compound_statement = compound_statement(fl);

			//If this is an error, we fail out
			if(default_compound_statement != NULL && default_compound_statement->ast_node_type == AST_NODE_TYPE_ERR_NODE){
				//Send it back up
				return default_compound_statement;
			}

			//Otherwise, we add this in as a child
			add_child_node(default_stmt, default_compound_statement);
			
			//Head out of here
			break;

		//This now means that we're in a c-style default statement
		case COLON:
			//Record that we're in a case statement in here
			push_nesting_level(&nesting_stack, NESTING_C_STYLE_CASE_STATEMENT);

			//We'll need to reassign the value of the original default statement
			default_stmt->ast_node_type = AST_NODE_TYPE_C_STYLE_DEFAULT_STMT;
			
			//Grab the next token to do our search
			lookahead = get_next_token(fl, &parser_line_num);

			//So long as we don't see another case, another default, or an R-curly, we keep
			//processing statements and adding them as children
			while(lookahead.tok != CASE && lookahead.tok != DEFAULT && lookahead.tok != R_CURLY){
				//Put the token back
				push_back_token(lookahead);

				//Process the next statement
				generic_ast_node_t* child = statement(fl);

				//If this is not null and in an error, we'll return that
				if(child != NULL && child->ast_node_type == AST_NODE_TYPE_ERR_NODE){
					return child;
				}

				//Otherwise, we'll add this as a child
				add_child_node(default_stmt, child);

				//And refresh the token to keep processing
				lookahead = get_next_token(fl, &parser_line_num);
			}

			//Push it back for something else to process
			push_back_token(lookahead);

			break;

		//We've hit some kind of issue here
		default:
			return print_and_return_error("-> or : required after case statement", parser_line_num);
	}

	//And pop it off now that we're done
	pop_nesting_level(&nesting_stack);

	//Otherwise it all worked, so we'll just return
	return default_stmt;
}


/**
 * Handle a case statement. A case statement does not terminate until we see another or default statement or the closing
 * curly of a switch statement
 *
 * NOTE: We assume that we have already seen and consumed the first case token here
 */
static generic_ast_node_t* case_statement(FILE* fl, generic_ast_node_t* switch_stmt_node, int32_t* values, int32_t* values_max_index){
	//Freeze the current line number
	u_int16_t current_line = parser_line_num;
	//Lookahead token
	lexitem_t lookahead;
	//Switch compound statement node for later on
	generic_ast_node_t* switch_compound_statement;

	//Create the node. This could change later on based on whether we have a c-style switch
	//statement or not
	generic_ast_node_t* case_stmt = ast_node_alloc(AST_NODE_TYPE_CASE_STMT, SIDE_TYPE_LEFT);
	
	//Push the case statement nesting level here
	push_nesting_level(&nesting_stack, NESTING_CASE_CONDITION);

	//Let the binary expression helper deal with this. We know that ultimately, the only
	//valid solution here is one where we end up with a constant in the end
	generic_ast_node_t* constant_node = logical_or_expression(fl, SIDE_TYPE_RIGHT);

	//There is only one valid result here - and that is a constant node
	switch(constant_node->ast_node_type){
		//The one and only valid case
		case AST_NODE_TYPE_CONSTANT:
			break;

		case AST_NODE_TYPE_ERR_NODE:
			return print_and_return_error("Invalid constant found in switch statment", current_line);

		default:
			printf("NODE TYPE IS %d\n", constant_node->ast_node_type);
			return print_and_return_error("Case statements must be values that expand to constants", current_line);
	}

	//Once we're done we can remove this
	pop_nesting_level(&nesting_stack);

	//Otherwise we know that it is good, but is it the right type
	//Are the types here compatible?
	case_stmt->inferred_type = types_assignable(switch_stmt_node->inferred_type, constant_node->inferred_type);

	//If this fails, they're incompatible
	if(case_stmt->inferred_type == NULL){
		sprintf(info, "Switch statement switches on type \"%s\", but case statement has incompatible type \"%s\"", 
					  switch_stmt_node->inferred_type->type_name.string, constant_node->inferred_type->type_name.string);
		return print_and_return_error(info, parser_line_num);
	}

	//
	//
	//
	//TODO - let's check to see if a given constant is inside of
	//an enum type's list of current values. We will throw a warning
	//if it isn't
	//
	//
	//
	//

	//Ultimately the constant type here is assigned over
	constant_node->inferred_type = case_stmt->inferred_type;

	//Once we have the constant node, it's done it's work. We will copy over whatever the resultant signed
	//integer value is from it
	case_stmt->constant_value.signed_int_value = constant_node->constant_value.signed_int_value;
	
	//If it's higher than the upper bound, it now is the upper bound
	if(case_stmt->constant_value.signed_int_value > switch_stmt_node->upper_bound){
		switch_stmt_node->upper_bound = case_stmt->constant_value.signed_int_value;
	}

	//If it's lower than the lower bound, it is now the lower bound
	if(case_stmt->constant_value.signed_int_value < switch_stmt_node->lower_bound){
		switch_stmt_node->lower_bound = case_stmt->constant_value.signed_int_value;
	}

	//If these are too far apart, we won't go for it. We'll check here, because once
	//we hit this, there's no point in going on
	if(switch_stmt_node->upper_bound - switch_stmt_node->lower_bound >= MAX_SWITCH_RANGE){
		sprintf(info, "Range from %d to %d exceeds %d, too large for a switch statement. Use a compound if statement instead", switch_stmt_node->lower_bound, switch_stmt_node->upper_bound, MAX_SWITCH_RANGE);
		return print_and_return_error(info, current_line);
	}

	//Let the helper deal with this. If we get a false here, then we bail out. This ensures that we have a nice sorted list
	//of values to deal with, which makes completeness validations in the parent method easier
	u_int8_t uniqueness_worked = sorted_list_insert_unique(values, values_max_index, case_stmt->constant_value.signed_int_value);

	//This means that a duplicate value was detected
	if(uniqueness_worked == FALSE){
		sprintf(info, "Value %d is duplicated in the switch statement", case_stmt->constant_value.signed_int_value);
		return print_and_return_error(info, parser_line_num);
	}

	//One last thing to check -- we need a colon
	lookahead = get_next_token(fl, &parser_line_num);

	//Here is the area where we're able to differentiate between an ollie style case
	//statement(-> {}) and a C-style case statement with fallthrough, etc.
	switch(lookahead.tok){
		case ARROW:
			//Push this onto the stack as a nesting level
			push_nesting_level(&nesting_stack, NESTING_CASE_STATEMENT);

			//We'll let the helper deal with it
			switch_compound_statement = compound_statement(fl);

			//If this is an error, we fail out
			if(switch_compound_statement != NULL && switch_compound_statement->ast_node_type == AST_NODE_TYPE_ERR_NODE){
				//Send it back up
				return switch_compound_statement;
			}

			//Otherwise, we add this in as a child
			add_child_node(case_stmt, switch_compound_statement);
			
			//Head out of here
			break;

		//This now means that we're in a c-style case statement
		case COLON:
			//Push the c-style version on, to differentiate from the other type
			push_nesting_level(&nesting_stack, NESTING_C_STYLE_CASE_STATEMENT);

			//We'll need to reassign the value of the original case statement
			case_stmt->ast_node_type = AST_NODE_TYPE_C_STYLE_CASE_STMT;
			
			//Grab the next token to do our search
			lookahead = get_next_token(fl, &parser_line_num);

			//So long as we don't see another case, another default, or an R-curly, we keep
			//processing statements and adding them as children
			while(lookahead.tok != CASE && lookahead.tok != DEFAULT && lookahead.tok != R_CURLY){
				//Put the token back
				push_back_token(lookahead);

				//Process the next statement
				generic_ast_node_t* child = statement(fl);

				//If this is not null and in an error, we'll return that
				if(child != NULL && child->ast_node_type == AST_NODE_TYPE_ERR_NODE){
					return child;
				}

				//Otherwise, we'll add this as a child
				add_child_node(case_stmt, child);

				//And refresh the token to keep processing
				lookahead = get_next_token(fl, &parser_line_num);
			}

			//Push it back for something else to process
			push_back_token(lookahead);

			break;

		//We've hit some kind of issue here
		default:
			return print_and_return_error("-> or : required after case statement", parser_line_num);
	}

	//And now that we're done, pop this off of the stack
	pop_nesting_level(&nesting_stack);

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
 * BNF Rule: <declare-statement> ::= declare {<function_predeclaration> | {static}? {mut}? <identifier> : <type-specifier>} ;
 */
static generic_ast_node_t* declare_statement(FILE* fl, u_int8_t is_global){
	//Freeze the current line number
	u_int16_t current_line = parser_line_num;
	//Lookahead token
	lexitem_t lookahead;

	//Let's see if we have a storage class
	lookahead = get_next_token(fl, &parser_line_num);

	//Go based on what we see here
	switch(lookahead.tok){
		//If we see either of these tokens, it means that the user is predeclaring a function.
		//In this case, we push the token back and let the function predeclaration rule
		//handle it
		case PUB:
		case FN:
			//If this is now global, then we cannot do this
			if(is_global == FALSE){
				return print_and_return_error("Function predeclarations must occur in global scope", parser_line_num);
			}

			push_back_token(lookahead);
			//Let this rule handle it
			return function_predeclaration(fl);
	
		//By default just leave
		default:
			break;
	}

	//If we get here and it's not an identifier, there is an issue
	if(lookahead.tok != IDENT){
		return print_and_return_error("Invalid identifier given in declaration", parser_line_num);
	}

	//Let's get a pointer to the name for convenience
	dynamic_string_t name = lookahead.lexeme;

	//Check for function duplciates
	if(do_duplicate_functions_exist(name.string) == TRUE){
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
	}

	//Check for type duplicates
	if(do_duplicate_types_exist(name.string) == TRUE){
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
	}

	//Check that it isn't some duplicated variable name. We will only check in the
	//local scope for this one
	symtab_variable_record_t* found_var = lookup_variable_local_scope(variable_symtab, name.string);

	//Fail out here
	if(found_var != NULL){
		sprintf(info, "Attempt to redefine variable \"%s\". First defined here:", name.string);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_variable_name(found_var);
		num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
	}

	//Let's see if we've already named a constant this
	symtab_constant_record_t* found_const = lookup_constant(constant_symtab, name.string);

	//Fail out if this isn't null
	if(found_const != NULL){
		sprintf(info, "Attempt to redefine constant \"%s\". First defined here:", name.string);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_constant_name(found_const);
		num_errors++;
		return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
	}

	
	//Now we need to see a colon
	lookahead = get_next_token(fl, &parser_line_num);

	if(lookahead.tok != COLON){
		return print_and_return_error("Colon required between identifier and type specifier in declare statement", parser_line_num);
	}
	
	//Now we are required to see a valid type specifier
	generic_type_t* type_spec = type_specifier(fl);

	//If this fails, the whole thing is bunk
	if(type_spec == NULL){
		return print_and_return_error("Invalid type specifier given in declaration", parser_line_num);
	}

	//If the type is incomplete, we do not allow it
	if(type_spec->type_complete == FALSE){
		sprintf(info, "Type %s is incomplete and may not be used as a declared variable's type", type_spec->type_name.string);
		return print_and_return_error(info, parser_line_num);
	}

	//Special restriction here - references must be initialized
	//upon declaration. In other words, the user is mandated to
	//use "let" to declare & initialize all at once
	if(type_spec->type_class == TYPE_CLASS_REFERENCE){
		//First potential issue - you can't use references
		//in the global scope
		if(is_global == TRUE){
			sprintf(info, "Variable %s is of type %s. Reference types cannot be used in the global scope", name.string, type_spec->type_name.string);
			return print_and_return_error(info, parser_line_num);
		}

		//Otherwise - another issue here. You can never declare a reference
		sprintf(info, "Variable %s is of type %s. Reference types must be declared and intialized in the same step using the \"let\" keyword", name.string, type_spec->type_name.string);
		return print_and_return_error(info, parser_line_num);
	}

	//One thing here, we aren't allowed to see void
	if(strcmp(type_spec->type_name.string, "void") == 0){
		return print_and_return_error("\"void\" type is only valid for function returns, not variable declarations", parser_line_num);
	}

	//The last thing that we are required to see before final assembly is a semicolon
	lookahead = get_next_token(fl, &parser_line_num);

	//No semicolon, we fail out
	if(lookahead.tok != SEMICOLON){
		return print_and_return_error("Semicolon required at the end of declaration statement", parser_line_num);
	}

	//Now that we've made it down here, we know that we have valid syntax and no duplicates. We can
	//now create the variable record for this function
	//Initialize the record
	symtab_variable_record_t* declared_var = create_variable_record(name);
	//Store the type--make sure that we strip any aliasing off of it first
	declared_var->type_defined_as = dealias_type(type_spec);
	//It was declared
	declared_var->declare_or_let = 0;
	//What function are we in?
	declared_var->function_declared_in = current_function;
	//The line_number
	declared_var->line_number = current_line;
	//Is it global? This speeds up optimization down the line
	declared_var->membership = is_global == TRUE ? GLOBAL_VARIABLE : NO_MEMBERSHIP;
	//Now that we're all good, we can add it into the symbol table
	insert_variable(variable_symtab, declared_var);

	//By default this is NULL
	generic_ast_node_t* declaration_node = NULL;

	//Based on what type we have, we may or may not even need a declaration here
	switch(declared_var->type_defined_as->type_class){
		//These all require a stack allocation, so a node is required
		case TYPE_CLASS_ARRAY:
		case TYPE_CLASS_UNION:
			/**
			 * Special warning here - if the user is declaring an array or a union as
			 * immutable, they actually never initialize these memory regions. We should
			 * throw a warning up for this
			 */
			if(declared_var->type_defined_as->mutability == NOT_MUTABLE){
				//Throw up the warning here
				sprintf(info, "Type \"%s\" is immutable. If you declare variable \"%s\" with this type, you may never be able to initialize it",
								declared_var->type_defined_as->type_name.string,
								declared_var->var_name.string);
				print_parse_message(WARNING, info, parser_line_num);
			}

			//Fall through
		case TYPE_CLASS_STRUCT:
			//Actually create the node now
			declaration_node = ast_node_alloc(AST_NODE_TYPE_DECL_STMT, SIDE_TYPE_LEFT);

			//Also store this record with the root node
			declaration_node->variable = declared_var;
			//Store the type as well
			declaration_node->inferred_type = declared_var->type_defined_as;
			//Store the line number
			declaration_node->line_number = current_line;

			//Since this is a memory region, it counts as being initialized by default
			declared_var->initialized = TRUE;

			break;

		//Otherwise just leave
		default:
			//If this is a global variable, then we also must ensure that a declaration exists
			if(is_global == TRUE){
				//Actually create the node now
				declaration_node = ast_node_alloc(AST_NODE_TYPE_DECL_STMT, SIDE_TYPE_LEFT);

				//Also store this record with the root node
				declaration_node->variable = declared_var;
				//Store the type as well
				declaration_node->inferred_type = declared_var->type_defined_as;
				//Store the line number
				declaration_node->line_number = current_line;
			}

			//Since this is not a memory region, is does not count as being initialized
			declared_var->initialized = FALSE;

			break;
	}

	//Return this. It will either be NULL or a generic node based on whether or not
	//a stack allocation was required
	return declaration_node;
}


/**
 * Crawl the array initializer list and validate that we have a compatible type for each entry in the list
 */
static u_int8_t validate_types_for_array_initializer_list(generic_type_t* array_type, generic_ast_node_t* initializer_list_node, u_int8_t is_global){
	//Grab the member type here out as well
	generic_type_t* member_type = array_type->internal_types.member_type;

	//Let's extract the number of records that we expect. It could either be 0(implicitly initialized) or it could be a nonzero value
	u_int32_t num_members = array_type->internal_values.num_members;

	//Let's also keep a record of the number of members that we've seen in total
	u_int32_t initializer_list_members = 0;

	//Grab a cursor to iterate over the children of the initializer list
	generic_ast_node_t* cursor = initializer_list_node->first_child;

	//Now for each value in the initializer node, we need to verify that it matches the array type. In otherwords, is it assignable
	//to the given array type
	while(cursor != NULL){
		//We'll use the same top level initialization check for this rule as well
		generic_type_t* final_type = validate_intializer_types(member_type, cursor, is_global);

		//If these fail, then we're done here. No need for an error message, they'll have already been
		//printed
		if(final_type == NULL){
			return FALSE;
		}

		//Increment the member count by 1
		initializer_list_members++;

		//Push this up to the next sibling
		cursor = cursor->next_sibling;
	}

	//The final check down here has 2 options:
	// 1.) The node's length was 0, in which case, we set the length based on the number of members we saw
	// 2.) The length was set, in which case, we validate the length here
	if(num_members != 0){
		//Validate that they match here
		if(num_members != initializer_list_members){
			sprintf(info, "Attempt to assign %d members to an array of size %d", initializer_list_members, num_members);
			print_parse_message(PARSE_ERROR, info, initializer_list_node->line_number);
			return FALSE;
		}
	//Otherwise, we'll need to set the number of members accordingly here
	} else {
		array_type->internal_values.num_members = initializer_list_members;

		//Reup the acutal size here
		array_type->type_size = initializer_list_members * array_type->internal_types.member_type->type_size;

		//Flag that this is now a complete type
		array_type->type_complete = TRUE;
	}

	//If we make it here, then we can set the type of the initializer list to match the array
	initializer_list_node->inferred_type = array_type;

	//If we made it here, then we know that we're good
	return TRUE;
}


/**
 * Struct initializers, unlike array intializers, only have one way of working. The user needs to properly define all of the
 * fields in the struct in the initializer. Unlike in C or other languages, we will not allows users to partially fill a struct
 * up
 */
static u_int8_t validate_types_for_struct_initializer_list(generic_type_t* struct_type, generic_ast_node_t* initializer_list_node, u_int8_t is_global){
	//We'll need to extract the struct table and that max index that it holds
	dynamic_array_t struct_table = struct_type->internal_types.struct_table;

	//The number of fields that were defined in the type is here
	u_int32_t num_fields = struct_table.current_index;

	//Initialize a cursor to the initializer list node itself
	generic_ast_node_t* cursor = initializer_list_node->first_child;

	//Keep a count of how many fields we've seen
	u_int32_t seen_count = 0;

	//Run through every node in here
	while(cursor != NULL){
		//If we exceed the number of fields given, we error out
		if(seen_count > num_fields){
			sprintf(info, "Type %s expects %d fields, was given at least %d in initializer", struct_type->type_name.string, num_fields, seen_count);
			print_parse_message(PARSE_ERROR, info, initializer_list_node->line_number);
			return FALSE;
		}

		//Grab the variable out
		symtab_variable_record_t* variable = dynamic_array_get_at(&struct_table, seen_count);

		//Recursively call the initializer processor rule. This allows us to handle nested initializations
		generic_type_t* final_type = validate_intializer_types(variable->type_defined_as, cursor, is_global);

		//Let's check to see if the types are assignable
		if(final_type == NULL){
			return FALSE;
		}

		//Increment this counter
		seen_count++;

		//Advance to the next sibling
		cursor = cursor->next_sibling;
	}

	//One final validation - we need to check if the field counts match
	if(num_fields != seen_count){
		sprintf(info, "Type %s expects %d fields, was given %d in initializer", struct_type->type_name.string, num_fields, seen_count);
		print_parse_message(PARSE_ERROR, info, initializer_list_node->line_number);
		return FALSE;
	}

	//Set the struct type here accordingly
	initializer_list_node->inferred_type = struct_type; 

	//If we made it here, then we know that we're good
	return TRUE;
}


/**
 * There are two options that we could see for a string initializer:
 *
 * 1.) let a:char[] := "hello"; //We auto set the bounds to be 6 here
 * 2.) let a:char[6] := "hello"; //This is also valid, we just need to ensure that things match
 *
 * Returns an error node if bad. If good, we return a string initializer node with the string constant
 * node as its child
 */
static generic_ast_node_t* validate_or_set_bounds_for_string_initializer(generic_type_t* array_type, generic_ast_node_t* string_constant){
	//Let's first validate that this array actually is a char[]
	if(array_type->internal_types.member_type->type_class != TYPE_CLASS_BASIC || array_type->internal_types.member_type->basic_type_token != CHAR){
		//Print out the full error message
		sprintf(info, "Attempt to use a string initializer for an array of type: %s. String initializers are only valid for type: char[]", array_type->type_name.string);

		//Fail out here
		return print_and_return_error(info, parser_line_num);
	}

	//Now we have two possible options here. We could either be seeing a completely "raw" array type(where the length is set to 0) or
	//we could be seeing an array type where the length is already set. Either way, we'll need to get the string length of the constant
	
	//A dynamic string stores a string lenght, it does not account for the null terminator. As such, we'll need to have the null terminator
	//accounted for by adding 1 to it
	u_int32_t length = string_constant->string_value.current_length + 1;
	
	//Now we have two options - if the length is 0, then we'll need to validate the length. Otherwise, we'll need set the 
	//lenght of the array to be whatever we have in here
	if(array_type->internal_values.num_members == 0){
		//Set the number of members
		array_type->internal_values.num_members = length;

		//Since these are all chars, the size of the array is just the length
		array_type->type_size = length;
	} else {
		//If these are different, then we fail out
		if(array_type->internal_values.num_members != length){
			sprintf(info, "String initializer length mismatch: array length is %d but string length is %d", array_type->internal_values.num_members, length);
			return print_and_return_error(info, parser_line_num);
		}

		//Otherwise we're all set
	}

	//Reassign the class here from a constant to a string initializer
	string_constant->ast_node_type = AST_NODE_TYPE_STRING_INITIALIZER;

	//Reassign the type to match what was sent in
	string_constant->inferred_type = array_type;

	//And give this node back
	return string_constant;
}


/**
 * Top level initializer value for type validation
 */
static generic_type_t* validate_intializer_types(generic_type_t* target_type, generic_ast_node_t* initializer_node, u_int8_t is_global){
	//Dealias this just to be safe
	target_type = dealias_type(target_type);

	//What's the return type of our node?
	generic_type_t* return_type = target_type;

	//By default, we assume we will fail. The validation step will need to prove us wrong
	u_int8_t validation_succeeded = FALSE;

	//Based on what the class of this initializer node is, there are several different
	//paths that we can take
	switch(initializer_node->ast_node_type){
		//If it's in error itself, we just leave
		case AST_NODE_TYPE_ERR_NODE:
			//Throw an error here
			print_parse_message(PARSE_ERROR, "Invalid expression given as intializer", parser_line_num);
			//Return null to mean failure
			return NULL;

		//An array initializer list has a special checking function
		//that we must use
		case AST_NODE_TYPE_ARRAY_INITIALIZER_LIST:
			//What if the user is trying to use an array initializer on a non-array type? If so, this should fail
			if(target_type->type_class != TYPE_CLASS_ARRAY){
				sprintf(info, "Type \"%s\" is not an array and therefore may not be initialized with the [] syntax", target_type->type_name.string);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				//Null signifies failure
				return NULL;
			}

			//Run the validation step for the intializer list
			validation_succeeded = validate_types_for_array_initializer_list(target_type, initializer_node, is_global);

			//If this didn't work we fail out
			if(validation_succeeded == FALSE){
				print_parse_message(PARSE_ERROR, "Invalid array intializer given", initializer_node->line_number);
				return NULL;
			}

			//Give back the return type
			return return_type;
			
		//A struct initializer list also has it's own special checking function that we must use
		case AST_NODE_TYPE_STRUCT_INITIALIZER_LIST:
			//What if the user is trying to use an array initializer on a non-array type? If so, this should fail
			if(target_type->type_class != TYPE_CLASS_STRUCT){
				sprintf(info, "Type \"%s\" is not a struct and therefore may not be initialized with the {} syntax", target_type->type_name.string);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				//Null signifies failure
				return NULL;
			}

			//Run the validation step for a struct
			validation_succeeded = validate_types_for_struct_initializer_list(target_type, initializer_node, is_global);

			//If this didn't work we fail out
			if(validation_succeeded == FALSE){
				print_parse_message(PARSE_ERROR, "Invalid struct intializer given", initializer_node->line_number);
				return NULL;
			}

			//Give back the return type
			return return_type;
			
		//Otherwise we'll just take the standard path
		default:
			//If we have a string constant, there's a chance that we could be seeing a string
			//initializer of the form let a:char[] := "Hi";. If that's the case, we'll let
			//the helper deal with it
			if(initializer_node->ast_node_type == AST_NODE_TYPE_CONSTANT && initializer_node->constant_type == STR_CONST
				&& target_type->type_class == TYPE_CLASS_ARRAY){
				
				//Dynamically set the initializer node here in the helper function
				initializer_node = validate_or_set_bounds_for_string_initializer(target_type, initializer_node);

				//If it's an error, we need to fail out now
				if(initializer_node->ast_node_type == AST_NODE_TYPE_ERR_NODE){
					//Throw it up the chain by return null
					return NULL;
				}

				//Otherwise we'll just break out. The initializer node will have been properly
				//set by the function above
				return return_type;
			}

			//If it's a global VAR, the initialization here must be a constant
			if(is_global == TRUE && initializer_node->ast_node_type != AST_NODE_TYPE_CONSTANT){
				//Fail out if we hit this
				print_parse_message(PARSE_ERROR, "Initializer value is not a compile-time constant", parser_line_num);
				return NULL;
			}

			/**
			 * If we somehow get here and we have either an array, struct
			 * or union type, this is incorrect. These types can only be initialized using
			 * the initializer strategy
			 */
			if(is_memory_region(target_type) == TRUE){
				sprintf(info, "Type \"%s\" may only be initialized using the appropriate initializer list syntax", target_type->type_name.string);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				return NULL;
			}

			//Use the helper to determine if the types are assignable
			generic_type_t* final_type = types_assignable(return_type, initializer_node->inferred_type);

			//Will be null if we have a failure
			if(final_type == NULL){
				generate_types_assignable_failure_message(info, initializer_node->inferred_type, return_type);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				return NULL;
			}

			//Additional validation here - it is not possible to assign a reference
			//to another reference. The types_assignable will let that go because
			//of our need to do it inside of function calls. But, if we catch that here,
			//we can't have it. However, we can have something like: assigning a reference
			//to another reference that's returned from a function call. It all depends on
			//what the return node type is here
			if(return_type->type_class == TYPE_CLASS_REFERENCE && initializer_node->ast_node_type == AST_NODE_TYPE_IDENTIFIER){
				//This is a fail case
				if(initializer_node->inferred_type->type_class == TYPE_CLASS_REFERENCE){
					//Detailed error message
					sprintf(info, "Reference of type %s%s may not be assigned to another %s%s reference type",
								return_type->mutability == MUTABLE ? "mut " : "",
								return_type->type_name.string, 
								initializer_node->inferred_type->mutability == MUTABLE ? "mut " : "",
								initializer_node->inferred_type->type_name.string);

					print_parse_message(PARSE_ERROR, info, parser_line_num);
					//NULL signifies failure
					return NULL;
				}
			}

			//If we have a constant node, we need to perform any needed type coercion here
			if(initializer_node->ast_node_type == AST_NODE_TYPE_CONSTANT){
				//Set the final type
				initializer_node->inferred_type = final_type;

				//Let the helper do whatever we need
				perform_constant_assignment_coercion(initializer_node, final_type);
			}
			
			//Give back the return type
			return final_type;
	}
}


/**
 * A let statement is always the child of an overall declaration statement. Like a declare statement, it also
 * performs type checking and inference and all needed symbol table manipulation
 *
 * NOTE: By the time we get here, we've already consumed the let keyword
 *
 * BNF Rule: <let-statement> ::= let {register | static}? <identifier> : <type-specifier> := <ternary_expression>;
 */
static generic_ast_node_t* let_statement(FILE* fl, u_int8_t is_global){
	//The line number
	u_int16_t current_line = parser_line_num;
	//Lookahead token
	lexitem_t lookahead;

	//Let's first declare the root node
	generic_ast_node_t* let_stmt_node = ast_node_alloc(AST_NODE_TYPE_LET_STMT, SIDE_TYPE_LEFT);

	//Grab the next token -- we could potentially see a storage class specifier
	lookahead = get_next_token(fl, &parser_line_num);

	//If it's not an identifier, we fail
	if(lookahead.tok != IDENT){
		return print_and_return_error("Invalid identifier given in let statement", parser_line_num);
	}

	//Let's get a pointer to the name for convenience
	dynamic_string_t name = lookahead.lexeme;

	//Check for function duplicates
	if(do_duplicate_functions_exist(name.string) == TRUE){
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
	}

	//Check for duplicate types
	if(do_duplicate_types_exist(name.string) == TRUE){
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
	}

	//Check that it isn't some duplicated variable name. We will only check in the
	//local scope for this one
	symtab_variable_record_t* found_var = lookup_variable_local_scope(variable_symtab, name.string);

	//Fail out here
	if(found_var != NULL){
		sprintf(info, "Attempt to redefine variable \"%s\". First defined here:", name.string);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_variable_name(found_var); num_errors++;
		//Return a fresh error node
		return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
	}

	//Let's see if we've already named a constant this
	symtab_constant_record_t* found_const = lookup_constant(constant_symtab, name.string);

	//Fail out if this isn't null
	if(found_const != NULL){
		sprintf(info, "Attempt to redefine constant \"%s\". First defined here:", name.string);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_constant_name(found_const);
		num_errors++;
		return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
	}

	//Now we need to see a colon
	lookahead = get_next_token(fl, &parser_line_num);

	if(lookahead.tok != COLON){
		return print_and_return_error("Expected colon between identifier and type specifier in let statement", parser_line_num);
	}

	//Now we are required to see a valid type specifier
	generic_type_t* type_spec = type_specifier(fl);

	//If this fails, the whole thing is bunk
	if(type_spec == NULL){
		return print_and_return_error("Invalid type specifier given in let statement", parser_line_num);
	}
	
	//One thing here, we aren't allowed to see void
	if(IS_VOID_TYPE(type_spec) == TRUE){
		return print_and_return_error("\"void\" type is only valid for function returns, not variable declarations", parser_line_num);
	}

	//We cannot have reference types in the global scope
	if(type_spec->type_class == TYPE_CLASS_REFERENCE){
		if(is_global == TRUE){
			sprintf(info, "Variable %s is of type %s. Reference types cannot be used in the global scope", name.string, type_spec->type_name.string);
			return print_and_return_error(info, parser_line_num);
		}
	}

	//Now we know that it wasn't a duplicate, so we must see a valid assignment operator
	lookahead = get_next_token(fl, &parser_line_num);

	//Assop is mandatory here
	if(lookahead.tok != EQUALS){
		return print_and_return_error("Assignment operator(=) required after identifier in let statement", parser_line_num);
	}

	//Now we need to see a valid initializer
	generic_ast_node_t* initializer_node = initializer(fl, SIDE_TYPE_RIGHT);
	
	//Store the return type here after we do all needed validations. This rule allows 
	//for recursive validation, so that we can handle recursive initialization
	generic_type_t* return_type = validate_intializer_types(type_spec, initializer_node, is_global);

	//If the return type is NULL, we fail out here
	if(return_type == NULL){
		return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
	}

	//If the return type of the logical or expression is an address, is it an address of a mutable variable?
	if(initializer_node->inferred_type->type_class == TYPE_CLASS_POINTER){
		if(initializer_node->variable != NULL && initializer_node->variable->type_defined_as->mutability == NOT_MUTABLE && type_spec->mutability == MUTABLE){
			return print_and_return_error("Mutable references to immutable variables are forbidden", parser_line_num);
		}
	}

	//Store this just in case--most likely won't use
	let_stmt_node->inferred_type = return_type;

	//Otherwise it worked, so we'll add it in as a child
	add_child_node(let_stmt_node, initializer_node);

	//The last thing that we are required to see before final assembly is a semicolon
	lookahead = get_next_token(fl, &parser_line_num);

	//Last possible tripping point
	if(lookahead.tok != SEMICOLON){
		return print_and_return_error("Semicolon required at the end of let statement", parser_line_num);
	}

	//Now that we've made it down here, we know that we have valid syntax and no duplicates. We can
	//now create the variable record for this function
	//Initialize the record
	symtab_variable_record_t* declared_var = create_variable_record(name);
	//Store the type
	declared_var->type_defined_as = type_spec;
	//It was initialized
	declared_var->initialized = TRUE;
	//Mark where it was declared
	declared_var->function_declared_in = current_function;
	//It was "letted" 
	declared_var->declare_or_let = 1;
	//Is it a global var or not? This speeds up optimization
	declared_var->membership = is_global == TRUE ? GLOBAL_VARIABLE : NO_MEMBERSHIP;
	//Save the line num
	declared_var->line_number = current_line;

	//Now that we're all good, we can add it into the symbol table
	insert_variable(variable_symtab, declared_var);

	/**
	 * If we have a reference type, then we know off the bat that whatever
	 * variable we are assigning here will be stored/used by reference, even
	 * if it is implicitly. As such, we will flag that our variable from above
	 * is a "stack variable". This will tell the compiler to skip trying to keep
	 * it in a register and throw it in the stack immediately. Luckily, setting
	 * this flag is all that we need to do in the parser
	 */
	if(type_spec->type_class == TYPE_CLASS_REFERENCE){
		//If the initialized node's variable is a thing and it's not a stack variable, we'll
		//need to make it one
		if(initializer_node->variable != NULL){
			//If it's not already a stack variable, then make it
			//one
			if(initializer_node->variable->stack_region == NULL){
				//Otherwise, we need to flag that the variable that is being referenced here *must* be stored
				//on the stack going forward, because it is being referenced
				initializer_node->variable->stack_variable = TRUE;

				//Make the stack region right now while we're at it
				initializer_node->variable->stack_region = create_stack_region_for_type(&(current_function->data_area), initializer_node->inferred_type);
			}

			//This is a stack variable. We need to load to & from memory whenever we use it
			declared_var->stack_variable = TRUE;

			//This variable's stack region just points to the one that the referenced variable has. This may
			//not always be the case, but it usually is
			declared_var->stack_region = initializer_node->variable->stack_region;
		}
	}

	//Add the reference into the root node
	let_stmt_node->variable = declared_var;
	//Store the line number
	let_stmt_node->line_number = current_line;

	//In special cases, we'll store this variable in the "node" section
	switch(initializer_node->ast_node_type){
		case AST_NODE_TYPE_ARRAY_INITIALIZER_LIST:
		case AST_NODE_TYPE_STRING_INITIALIZER:
		case AST_NODE_TYPE_STRUCT_INITIALIZER_LIST:
			initializer_node->variable = declared_var;
			break;

		//Otherwise not
		default:
			break;
	}

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
	//Our lookahead token
	lexitem_t lookahead;

	//We need to first see a valid type specifier
	generic_type_t* type_spec= type_specifier(fl);

	//If it is bad, we'll bail out
	if(type_spec == NULL){
		print_parse_message(PARSE_ERROR, "Invalid type specifier given to alias statement", parser_line_num);
		num_errors++;
		//Fail out
		return FAILURE;
	}

	//Once we have the reference, the actual node is useless so we'll free it

	//We now need to see the as keyword
	lookahead = get_next_token(fl, &parser_line_num);

	//If we don't see it we're out
	if(lookahead.tok != AS){
		print_parse_message(PARSE_ERROR, "As keyword expected in alias statement", parser_line_num);
		num_errors++;
		//Fail out
		return FAILURE;
	}

	//Now we need to see an identifier here
	lookahead = get_next_token(fl, &parser_line_num);

	//If it's bad, we're also done here
	if(lookahead.tok != IDENT){
		print_parse_message(PARSE_ERROR, "Invalid identifier given to alias statement", parser_line_num);
		num_errors++;
		return FAILURE;
	}

	//Grab this out for convenience
	dynamic_string_t name = lookahead.lexeme;

	//Let's do our last syntax check--the semicolon
	lookahead = get_next_token(fl, &parser_line_num);

	//If we don't see a semicolon we're out
	if(lookahead.tok != SEMICOLON){
		print_parse_message(PARSE_ERROR, "Semicolon expected at the end of alias statement",  parser_line_num);
		num_errors++;
		//Fail out here
		return FAILURE;
	}

	//First check for duplicate functions
	if(do_duplicate_functions_exist(name.string) == TRUE){
		return FAILURE;
	}

	//Now check for duplicate variables
	if(do_duplicate_variables_exist(name.string) == TRUE){
		return FAILURE;
	}

	//Check for duplicate types
	if(do_duplicate_types_exist(name.string) == TRUE){
		return FAILURE;
	}

	//If we get here, we know that it actually worked, so we can create the alias
	//The alias type's mutability is that of the type specifier's mutability
	generic_type_t* aliased_type = create_aliased_type(name.string, type_spec, parser_line_num, type_spec->mutability);

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
	//We can now see construct or enum
	lexitem_t lookahead = get_next_token(fl, &parser_line_num);

	//Go based on the lookahead
	switch(lookahead.tok){
		case STRUCT:
			return struct_definer(fl);
		case UNION:
			return union_definer(fl);
		case ENUM:
			return enum_definer(fl);
		case FN:
			return function_pointer_definer(fl);

		//Some failure here
		default:
			print_parse_message(PARSE_ERROR, "Expected \"union\", \"struct\", \"fn\" or \"enum\" definer keywords", parser_line_num);
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
static generic_ast_node_t* declaration(FILE* fl, u_int8_t is_global){
	//Lookahead token
	lexitem_t lookahead;

	//We will multiplex based on what we see with the lookahead
	//This rule also consumes the first token that it sees, so all downstream
	//rules must account for that
	lookahead = get_next_token(fl, &parser_line_num);

	switch(lookahead.tok){
		case DECLARE:
			return declare_statement(fl, is_global);
		case LET:	
			return let_statement(fl, is_global);
		default:
			sprintf(info, "Saw \"%s\" when let or declare was expected", lookahead.lexeme.string);
			return print_and_return_error(info, parser_line_num);
	}
}


/**
 * We need to go through and check all of the jump statements that we have in the function. If any
 * one of these jump statements is trying to jump to a label that does not exist, then we need to fail out
 */
static int8_t check_jump_labels(){
	//Grab a reference to our current jump statement
	generic_ast_node_t* current_jump_statement;

	//So long as there are jump statements in the queue
	while(queue_is_empty(&current_function_jump_statements) == FALSE){
		//Grab the jump statement
		current_jump_statement = dequeue(&current_function_jump_statements);

		//Grab the label ident node
		generic_ast_node_t* label_ident_node = current_jump_statement->first_child;

		//Let's grab out the name for convenience
		char* name = label_ident_node->string_value.string;

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

		//This can also happen - where we have a user trying to jump to a non-label
		if(label->membership != LABEL_VARIABLE){
			sprintf(info, "Variable %s exists but is not a label, so it cannot be jumped to", label->var_name.string);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			return FAILURE;
		}

		//We can also have a case where this is not null, but it isn't in the correct function scope(also bad)
		if(strcmp(current_function->func_name.string, label->function_declared_in->func_name.string) != 0){
			sprintf(info, "Label \"%s\" was declared in function \"%s\". You cannot jump outside of a function" , name, label->function_declared_in->func_name.string);
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
 * Perform validation on the parameter & return type & order
 * for the main function
 */
static u_int8_t validate_main_function(generic_type_t* type){
	//Let's extract the signature first for convenience
	function_type_t* signature = type->internal_types.function_type;

	//If the main function is not public, then we fail
	if(signature->is_public == FALSE){
		print_parse_message(PARSE_ERROR, "The main function must be prefixed with the \"pub\" keyword", parser_line_num);
		return FALSE;
	}

	//For storing parameter types
	generic_type_t* parameter_type;

	//Let's first validate the parameter count. The main function can
	//either have 0 or 2 parameters
	
	switch(signature->num_params){
		//This is allowed
		case 0:
			break;

		//If we have two, we need to validate the type of each parameter
		case 2:
			//Extract the first parameter
			parameter_type = signature->parameters[0];
			
			//If it isn't a basic type and it isn't an i32, we fail
			if(parameter_type->type_class != TYPE_CLASS_BASIC || parameter_type->basic_type_token != I32){
				sprintf(info, "The first parameter of the main function must be an i32. Instead given: %s", type->type_name.string);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				return FALSE;
			}

			//Now let's grab the second parameter
			parameter_type = signature->parameters[1];

			//This must be a char** type. If it's not, we fail out
			if(is_type_string_array(parameter_type) == FALSE){
				sprintf(info, "The second parameter of the main function must be of type char**. Instead given: %s", type->type_name.string);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				return FALSE;
			}

			//If we make it all the way down here, then we know that we're set
			break;

		//We'll print an error and leave if this is the case
		default:
			sprintf(info, "The main function can have 0 or 2 parameters, but instead was given: %s", type->type_name.string);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			return FALSE;
	}

	//Finally, we'll validate the return type of the main function. It must also always be an i32
	if(signature->return_type->type_class != TYPE_CLASS_BASIC || signature->return_type->basic_type_token != I32){
		sprintf(info, "The main function must return a value of type i32, instead was given: %s", type->type_name.string);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		return FALSE;
	}

	//If we make it here, then we know it's true
	return TRUE;
}


/**
 * A parameter declaration is a fancy kind of variable. It is stored in the symtable at the 
 * top lexical scope for the function itself. Like all rules, it returns a reference to the
 * root of the subtree that it creates
 *
 * This rule will return a symtab variable record that represents the parameter it made. If will return
 * NULL if an error occurs
 *
 * BNF Rule: <parameter-declaration> ::= <identifier> : <type-specifier>
 */
static symtab_variable_record_t* parameter_declaration(FILE* fl, u_int16_t* current_gen_purpose_param, u_int16_t* current_sse_param){
	//Lookahead token
	lexitem_t lookahead;

	//Now we can optionally see the constant keyword here
	lookahead = get_next_token(fl, &parser_line_num);

	//If it didn't work we fail immediately
	if(lookahead.tok != IDENT){
		print_parse_message(PARSE_ERROR, "Expected identifier in function parameter declaration", parser_line_num);
		num_errors++;
		return NULL;
	}

	//Now we must perform all needed duplication checks for the name
	//Grab this for convenience
	dynamic_string_t name = lookahead.lexeme;

	//Check that it isn't some duplicated variable name
	symtab_variable_record_t* found_var = lookup_variable_local_scope(variable_symtab, name.string);

	//Fail out here
	if(found_var != NULL){
		sprintf(info, "Attempt to redefine variable \"%s\". First defined here:", name.string);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_variable_name(found_var);
		num_errors++;
		//Return NULL to signify failure
		return NULL;
	}

	//Check for a duplicated type
	if(do_duplicate_types_exist(name.string) == TRUE){
		return NULL;
	}

	//Now we need to see a colon
	lookahead = get_next_token(fl, &parser_line_num);

	//If it isn't a colon, we're out
	if(lookahead.tok != COLON){
		print_parse_message(PARSE_ERROR, "Colon required between type specifier and identifier in paramter declaration", parser_line_num);
		num_errors++;
		//Return NULL to signify failure
		return NULL;
	}

	//We are now required to see a valid type specifier node
	generic_type_t* type = type_specifier(fl);
	
	//If the node fails, we'll just send the error up the chain
	if(type == NULL){
		print_parse_message(PARSE_ERROR, "Invalid type specifier given to function parameter", parser_line_num);
		num_errors++;
		//It's already an error, just propogate it up
		return NULL;
	}

	//If this is an incomplete type, then we also fail
	if(type->type_complete == FALSE){
		sprintf(info, "Type %s is incomplete and therefore invalid for a function parameter", type->type_name.string);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		//It's already an error, just propogate it up
		return NULL;
	}

	//Once we get here, we have actually seen an entire valid parameter 
	//declaration. It is now incumbent on us to store it in the variable 
	//symbol table
	
	//Let's first construct the variable record
	symtab_variable_record_t* param_record = create_variable_record(name);
	//It is a function parameter
	param_record->membership = FUNCTION_PARAMETER;
	//We assume that it was initialized
	param_record->initialized = TRUE;
	//Add the line number
	param_record->line_number = parser_line_num;
	//Store the type as well, very important
	param_record->type_defined_as = type;

	//Most common case, not a floating point so
	//it counts as general-purpose
	if(IS_FLOATING_POINT(type) == FALSE){
		param_record->class_relative_function_parameter_order = *current_gen_purpose_param;

		//Bump it for the next go about
		(*current_gen_purpose_param)++;
	} else {
		param_record->class_relative_function_parameter_order = *current_sse_param;

		//Bump it for the next go about
		(*current_sse_param)++;
	}

	//This parameter was declared in whatever function we're currently in
	param_record->function_declared_in = current_function;

	//If we have a reference type, we need to flag that this is
	//a "stack variable" and needs to be dereferenced as we go
	if(type->type_class == TYPE_CLASS_REFERENCE){
		param_record->stack_variable = TRUE;
	}

	//We've now built up our param record, so we'll give add it to the symtab
	insert_variable(variable_symtab, param_record);

	//Give the variable back
	return param_record;
}


/**
 * A paramater list will handle all of the parameters in a function definition. It is important
 * to note that a parameter list may very well be empty, and that this rule will handle that case.
 * Regardless of the number of parameters(maximum of 6), a paramter list node will always be returned
 *
 * <parameter-list> ::= (<parameter-declaration> { ,<parameter-declaration>}*)
 */
static u_int8_t parameter_list(FILE* fl, symtab_function_record_t* function_record, u_int8_t defining_predeclared_function){
	//Lookahead token
	lexitem_t lookahead;

	//This is the internal function type, extracted for convenience
	generic_type_t* function_type = function_record->signature;
	function_type_t* internal_function_type = function_type->internal_types.function_type;
	
	//Now we need to see a valid parentheis
	lookahead = get_next_token(fl, &parser_line_num);

	//If we didn't find it, no point in going further
	if(lookahead.tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Left parenthesis expected before parameter list", parser_line_num);
		num_errors++;
		return FAILURE;
	}

	//Otherwise, we'll push this onto the list to check for later
	push_token(&grouping_stack, lookahead);

	//Now let's see what we have as the token. If it's an R_PAREN, we know that we're
	//done here and we'll just return an empty list
	lookahead = get_next_token(fl, &parser_line_num);

	switch(lookahead.tok){
		//If we see an R_PAREN immediately, we can check and leave
		case R_PAREN:
			//If we have a mismatch, we can return these
			if(pop_token(&grouping_stack).tok != L_PAREN){
				print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected", parser_line_num);
				num_errors++;
				return FAILURE;
			}

			//If we're validating, let's check and ensure that the defined type also
			//has no params
			if(defining_predeclared_function == TRUE){
				//If we have a mismatch, we fail out
				if(internal_function_type->num_params != 0){
					sprintf(info, "Predeclared function %s has %d parameters, not 0", function_record->func_name.string, internal_function_type->num_params);
					print_parse_message(PARSE_ERROR, info, parser_line_num);
					num_errors++;
					return FAILURE;
				}
			}

			//Otherwise we're fine, so return the list node
			return SUCCESS;

		//This is a possibility, we could see (void) as a valid declaration of no parameters
		case VOID:
			//We now need to see a closing R_PAREN
			lookahead = get_next_token(fl, &parser_line_num);

			//Fail out if we don't see this
			if(lookahead.tok != R_PAREN){
				print_parse_message(PARSE_ERROR, "Closing parenthesis expected after void parameter list declaration", parser_line_num);
				num_errors++;
				return FAILURE;
			}

			//Also check for grouping
			if(pop_token(&grouping_stack).tok != L_PAREN){
				print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected", parser_line_num);
				num_errors++;
				return FAILURE;
			}

			//If we're validating, let's check and ensure that the defined type also
			//has no params
			if(defining_predeclared_function == TRUE){
				//If we have a mismatch, we fail out
				if(internal_function_type->num_params != 0){
					sprintf(info, "Predeclared function %s has %d parameters, not 0", function_record->func_name.string, internal_function_type->num_params);
					print_parse_message(PARSE_ERROR, info, parser_line_num);
					num_errors++;
					return FAILURE;
				}
			}

			//Give back the parameter list node
			return SUCCESS;
			
		//By default just put it back and get out
		default:
			push_back_token(lookahead);
			break;
	}

	//Start off at 1 for both of these
	u_int16_t general_purpose_parameter_number = 1;
	u_int16_t sse_parameter_number = 1;
	//We also maintain one with no split, just the absolute number
	u_int16_t absolute_parameter_number = 1;

	//We'll keep going as long as we see more commas
	do{
		//We must first see a valid parameter declaration
		symtab_variable_record_t* parameter = parameter_declaration(fl, &general_purpose_parameter_number, &sse_parameter_number);

		//It's invalid, we'll just send it up the chain
		if(parameter == NULL){
			print_parse_message(PARSE_ERROR, "Invalid parameter declaration found in parameter list", parser_line_num);
			num_errors++;
			return FAILURE;;
		}

		//We will also store the "absolute" parameter number as well
		parameter->absolute_function_parameter_order = absolute_parameter_number;

		//Status tracker
		u_int8_t status;

		//If we're not defining a predeclared function, we need to add this parameter in
		if(defining_predeclared_function == FALSE){
			//Let the helper do it
			status = add_parameter_to_function_type(function_type, parameter->type_defined_as);

			//This means that we exceeded the number of parameters
			if(status == FAILURE){
				print_parse_message(PARSE_ERROR, "Functions may have a maximum of 6 parameters", parser_line_num);
				num_errors++;
				return FAILURE;
			}
			//Otherwise we're fine

		//If we get here, we need to validate that the type that was declared is
		//the same as the one originally given
		} else {
			//Check if we've got too many parameters
			if(absolute_parameter_number > internal_function_type->num_params){
				sprintf(info, "Function %s was defined with only %d parameters", function_record->func_name.string, internal_function_type->num_params);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return FAILURE;
			}

			//We need to ensure that the mutability levels match here
			if(internal_function_type->parameters[absolute_parameter_number - 1]->mutability == MUTABLE && parameter->type_defined_as->mutability == NOT_MUTABLE){
				sprintf(info, "Parameter %s was defined as immutable, but predeclared as mutable", parameter->var_name.string);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return FAILURE;

			//The other option for a mismatch
			} else if(internal_function_type->parameters[absolute_parameter_number - 1]->mutability == NOT_MUTABLE && parameter->type_defined_as->mutability == MUTABLE){
				sprintf(info, "Parameter %s was defined as mutable, but predeclared as immutable", parameter->var_name.string);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return FAILURE;
			}

			//If the mutability levels are off, we fail out
			if(internal_function_type->parameters[absolute_parameter_number - 1]->mutability != parameter->type_defined_as->mutability){
				sprintf(info, "Mutability mismatch for parameter %d", absolute_parameter_number);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return FAILURE;
			}

			//Grab the defined type out
			generic_type_t* declared_type = dealias_type(internal_function_type->parameters[absolute_parameter_number - 1]);
			//And this type
			generic_type_t* defined_type = dealias_type(parameter->type_defined_as);

			//If these 2 don't match, we fail
			if(defined_type != declared_type){
				sprintf(info, "Parameter %d was defined with type %s, but declared with type %s",  absolute_parameter_number, defined_type->type_name.string, declared_type->type_name.string);
				print_parse_message(PARSE_ERROR, info, parser_line_num);
				num_errors++;
				return FAILURE;
			}

			//Otherwise if we survive to here, then we're good
		}

		//Once we're here, we can add the function parameter in
		status = add_function_parameter(function_record, parameter);

		//If this fails that means we exceeded the maximum number of parameters
		//This means that we exceeded the number of parameters
		if(status == FAILURE){
			print_parse_message(PARSE_ERROR, "Functions may have a maximum of 6 parameters", parser_line_num);
			num_errors++;
			return FAILURE;
		}


		//We made it here, so we've seen one more absolute number
		absolute_parameter_number++;

		//Refresh the lookahead token
		lookahead = get_next_token(fl, &parser_line_num);

	//We keep going as long as we see commas
	} while(lookahead.tok == COMMA);

	//If we're predeclaring, we need to check that the parameter count matches
	if(defining_predeclared_function == TRUE && function_record->number_of_params != internal_function_type->num_params){
		sprintf(info, "Function %s was declared with %d parameters, but was only defined with %d", function_record->func_name.string, internal_function_type->num_params, function_record->number_of_params);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		num_errors++;
		return FAILURE;
	}

	//Once we reach here, we need to check for the R_PAREN
	if(lookahead.tok != R_PAREN){
		print_parse_message(PARSE_ERROR, "Closing parenthesis expected after parameter list", parser_line_num);
		num_errors++;
		return FAILURE;
	}

	//Otherwise it worked, so we need to check matching
	if(pop_token(&grouping_stack).tok != L_PAREN){
		print_parse_message(PARSE_ERROR, "Unmatched parenthesis detected", parser_line_num);
		num_errors++;
		return FAILURE;
	}

	//If we make it down here then this all worked, so
	return SUCCESS;
}


/**
 * A function predeclaration allows the user to basically
 * promise that a function of this signature will exist at 
 * some point
 *
 * <function_predeclaration> ::= declare {pub}? fn <identifier>({param_declaration | void} {, <param_declaration}*) -> <type-specifier> ;
 *
 * NOTE: by the time we get here, we've already seen the declare keyword
 */
static generic_ast_node_t* function_predeclaration(FILE* fl){
	//Lookahead token
	lexitem_t lookahead = get_next_token(fl, &parser_line_num);
	//Is this a public function?
	u_int8_t is_public = FALSE;

	//If we see the PUB keyword, that means we have a public function
	if(lookahead.tok == PUB){
		//Set the flag
		is_public = TRUE;
		//Refresh the lookahead
		lookahead = get_next_token(fl, &parser_line_num);
	}

	//Now we need to see the "fn" keyword. If we don't, we leave
	if(lookahead.tok != FN){
		return print_and_return_error("fn keyword required in function predeclaration", parser_line_num);
	}

	//Following this, we need to see an identifier
	lookahead = get_next_token(fl, &parser_line_num);

	//If it's not an ident, we leave
	if(lookahead.tok != IDENT){
		return print_and_return_error("Identifier required after fn keyword in function predeclaration", parser_line_num);
	}

	//Now we need to check for duplicated names. We'll do this for
	dynamic_string_t function_name = lookahead.lexeme;

	//Check for duplicated functions
	if(do_duplicate_functions_exist(function_name.string) == TRUE){
		return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
	}

	//Now duplicated variables
	if(do_duplicate_variables_exist(function_name.string) == TRUE){
		return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
	} 

	//Check for duplicate types
	if(do_duplicate_types_exist(function_name.string) == TRUE){
		//Create and return an error node
		return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
	}

	//Let's see if we've already named a constant this
	symtab_constant_record_t* found_const = lookup_constant(constant_symtab, function_name.string);

	//Fail out if this isn't null
	if(found_const != NULL){
		sprintf(info, "Attempt to redefine constant \"%s\". First defined here:", function_name.string);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_constant_name(found_const);
		num_errors++;
		return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
	}

	//The main function may not be predeclared
	if(strcmp(function_name.string, "main") == 0){
		return print_and_return_error("The main function may not be predeclared", parser_line_num);
	}

	//Now that we've survived up to here, we can make the actual record
	symtab_function_record_t* function_record = create_function_record(function_name, is_public, parser_line_num);

	//Now we need to see an lparen to begin the parameters
	lookahead = get_next_token(fl, &parser_line_num);

	//If we don't see it, we fail out
	if(lookahead.tok != L_PAREN){
		return print_and_return_error("Left parenthesis expected after function name", parser_line_num);
	}

	//Add this onto the grouping stack
	push_token(&grouping_stack, lookahead);

	//Now we can begin processing our parameters
	//Grab the next token
	lookahead = get_next_token(fl, &parser_line_num);

	//We must check some edge cases here
	switch(lookahead.tok){
		case VOID:
			//We now need to see an RPAREn
			lookahead = get_next_token(fl, &parser_line_num);
			
			//We now need to see an R_PAREN
			if(lookahead.tok != R_PAREN){
				return print_and_return_error("Right parenthesis required after void parameter declaration", parser_line_num);
			}

			//Otherwise just go to after r_paren
			goto after_rparen;

		//We'll just hop out
		case R_PAREN:
			goto after_rparen;

		//By default we can just leave
		default:
			push_back_token(lookahead);
			break;
	}

	//Keep processing so long as we keep seeing commas
	do{
		//Now we need to see a valid type
		generic_type_t* type = type_specifier(fl);

		//If this is NULL, we'll error out
		if(type == NULL){
			print_and_return_error("Invalid parameter type given", parser_line_num);
		}

		//Let the helper add the type in
		u_int8_t status = add_parameter_to_function_type(function_record->signature, type);

		//This means that we have been given too many parameters
		if(status == FAILURE){
			return print_and_return_error("Maximum function parameter count of 6 exceeded", parser_line_num);
		}

		//Refresh the lookahead token
		lookahead = get_next_token(fl, &parser_line_num);

	} while(lookahead.tok == COMMA);

	//Now that we're done processing the list, we need to ensure that we have a right paren
	if(lookahead.tok != R_PAREN){
		return print_and_return_error("Right parenthesis required after parameter list declaration", parser_line_num);
	}

after_rparen:
	//Make sure that we can pop the grouping stack and get a match
	if(pop_token(&grouping_stack).tok != L_PAREN){
		return print_and_return_error("Unmatched parenthesis detected", parser_line_num);
	}

	//Following this, we need to see the -> symbol
	lookahead = get_next_token(fl, &parser_line_num);

	//If we don't see it, we fail
	if(lookahead.tok != ARROW){
		return print_and_return_error("-> expected after function parameter list", parser_line_num);
	}

	//Now we need to see a valid type specifier
	generic_type_t* return_type = type_specifier(fl);

	//Fail out if bad
	if(return_type == NULL){
		return print_and_return_error("Invalid return type given", parser_line_num);
	}

	//Otherwise, this is the return type
	function_record->return_type = return_type;
	function_record->signature->internal_types.function_type->return_type = return_type;

	//One last thing, we need to see a semicolon
	lookahead = get_next_token(fl, &parser_line_num);

	//Fail out
	if(lookahead.tok != SEMICOLON){
		return print_and_return_error("Semicolon required at the end of function predeclaration", parser_line_num);
	}

	//Otherwise this all worked. We can add this function to the symtab
	insert_function(function_symtab, function_record);
	
	//A null return means that we succeeded
	return NULL;
}



/**
 * Handle the case where we declare a function. A function will always be one of the children of a declaration
 * partition
 *
 * NOTE: We have already consumed the FUNC keyword by the time we arrive here, so we will not look for it in this function
 *
 * BNF Rule: <function-definition> ::= {pub}? fn <identifer> {<parameter-list> -> <type-specifier> <compound-statement>
 */
static generic_ast_node_t* function_definition(FILE* fl){
	//Freeze the line number
	u_int16_t current_line = parser_line_num;
	//Lookahead token
	lexitem_t lookahead;
	//Have we predeclared this function
	u_int8_t definining_predeclared_function = FALSE;
	//Is it the main function?
	u_int8_t is_main_function = FALSE;
	//Is this function public or private? Unless explicitly stated, all functions are private
	u_int8_t is_public = FALSE;

	//Grab the token
	lookahead = get_next_token(fl, &parser_line_num);

	//We could see pub fn or fn here, so we need to process both cases
	switch(lookahead.tok){
		//Explicit declaration that this function is visible to other partial programs
		case PUB:
			//Flag that it is public
			is_public = TRUE;

			//Refresh the lookahead token
			lookahead = get_next_token(fl, &parser_line_num);

			//Now we need to ensure that this is the FN keyword, if it isn't, we fail out
			if(lookahead.tok != FN){
				return print_and_return_error("\"fn\" keyword is required after \"pub\" keyword", parser_line_num);
			}

			//Otherwise we're all good if we get here, so break out
			break;

		//Nothing more to do here, just leave
		case FN:
			break;
		
		//It would be bizarre if we got here, but just in case
		default:
			sprintf(info, "Expected \"pub\" or \"fn\" keywords, but got: %s\n", lookahead.lexeme.string);
			return print_and_return_error(info, parser_line_num);
	}

	//We also need to mark that we're in a function using the nesting stack
	push_nesting_level(&nesting_stack, NESTING_FUNCTION);

	//We need a stack for storing jump statements. We need to check these later because if
	//we check them as we go, we don't get full jump functionality
	current_function_jump_statements = heap_queue_alloc();

	//We also have the AST function node, this will be intialized immediately
	//It also requires a symtab record of the function, but this will be assigned
	//later once we have it
	generic_ast_node_t* function_node = ast_node_alloc(AST_NODE_TYPE_FUNC_DEF, SIDE_TYPE_LEFT);

	//Now we must see a valid identifier as the name
	lookahead = get_next_token(fl, &parser_line_num);

	//If we have a failure here, we're done for
	if(lookahead.tok != IDENT){
		return print_and_return_error("Invalid name given as function name", current_line);
	}

	//Otherwise, we could still have a failure here if this is any kind of duplicate
	//Grab a reference for convenience
	dynamic_string_t function_name = lookahead.lexeme;

	//Now we must perform all of our symtable checks. Parameters may not share names with types, functions or variables
	symtab_function_record_t* function_record = lookup_function(function_symtab, function_name.string);

	//Fail out if found and it's already been defined
	if(function_record != NULL && function_record->defined == TRUE){
		sprintf(info, "A function with name \"%s\" has already been defined. First defined here:", function_record->func_name.string);
		print_parse_message(PARSE_ERROR, info, current_line);
		print_function_name(function_record);
		num_errors++;
		//Create and return an error node
		return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
	}

	//If the function record is NULL, that means we're defining completely fresh
	if(function_record == NULL){
		//Check for duplicate variables here
		if(do_duplicate_variables_exist(function_name.string) == TRUE){
			//Create and return an error node
			return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
		}

		//Check for duplicate types
		if(do_duplicate_types_exist(function_name.string) == TRUE){
			//Create and return an error node
			return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
		}

		//Let's see if we've already named a constant this
		symtab_constant_record_t* found_const = lookup_constant(constant_symtab, function_name.string);

		//Fail out if this isn't null
		if(found_const != NULL){
			sprintf(info, "Attempt to redefine constant \"%s\". First defined here:", function_name.string);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			//Also print out the original declaration
			print_constant_name(found_const);
			num_errors++;
			return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
		}

		//Now that we know it's fine, we can first create the record. There is still more to add in here, but we can at least start it
		function_record = create_function_record(function_name, is_public, parser_line_num);

		//We'll put the function into the symbol table
		//since we now know that everything worked
		insert_function(function_symtab, function_record);

		//We'll also flag that this is the current function
		current_function = function_record;

		/**
		 * If this is the main function, we will record it as having been called by the operating 
		 * system
		 */
		if(strcmp("main", function_name.string) == 0){
			//It is the main function
			is_main_function = TRUE;
		}

	//If we get here, we know that we're defining a predeclared function
	} else {
		//Flag this
		definining_predeclared_function = TRUE;
		//Set this as well
		current_function = function_record;

		//Let's now check - if the is_public's don't match here, we can fail already
		if(function_record->signature->internal_types.function_type->is_public == TRUE && is_public == FALSE){
			sprintf(info, "Function %s was predeclared as public, but defined as private", function_record->func_name.string);
			return print_and_return_error(info, parser_line_num);

		//Other case, still a failure
		} else if(function_record->signature->internal_types.function_type->is_public == TRUE && is_public == TRUE){
			sprintf(info, "Function %s was predeclared as private, but defined as public", function_record->func_name.string);
			return print_and_return_error(info, parser_line_num);
		}
	}

	//Associate this with the function node
	function_node->func_record = function_record;

	//We'll need to initialize a new variable scope here. This variable scope is designed
	//so that we include the function parameters in it. We need to remember to close
	//this once we leave
	initialize_variable_scope(variable_symtab);

	//Now we must ensure that we see a valid parameter list. It is important to note that
	//parameter lists can be empty, but whatever we have here we'll have to add in
	//Parameter list parent is the function node
	u_int8_t status = parameter_list(fl, function_record, definining_predeclared_function);

	//We have a bad parameter list, we just fail out
	if(status == FAILURE){
		return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
	}

	//Semantics here, we now must see a valid arrow symbol
	lookahead = get_next_token(fl, &parser_line_num);

	//If it isn't an arrow, we're out of here
	if(lookahead.tok != ARROW){
		return print_and_return_error("Arrow(->) required after parameter-list in function", parser_line_num);
	}

	//Now if we get here, we must see a valid type specifier
	//The type specifier rule already does existence checking for us
	generic_type_t* return_type = type_specifier(fl);

	//If we failed, bail out
	if(return_type == NULL){
		return print_and_return_error("Invalid return type given to function. All functions, even void ones, must have an explicit return type", parser_line_num);
	}

	//Grab the type record. A reference to this will be stored in the function symbol table. Make sure
	//that we first dealias it
	generic_type_t* type = dealias_type(return_type);

	//If we're defining a function that was previously implicit, the types have to match exactly
	if(definining_predeclared_function == TRUE){
		if(strcmp(type->type_name.string, function_record->return_type->type_name.string) != 0){
			sprintf(info, "Function \"%s\" was predeclared with a return type of \"%s\", this may not be altered. First defined here:", function_name.string, function_record->return_type->type_name.string);
			print_parse_message(PARSE_ERROR, info, parser_line_num);
			print_function_name(function_record);
			num_errors++;
			return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
		}
	}

	//Store the return type
	function_record->return_type = type;

	//Record whether or not it's a void type
	function_record->signature->internal_types.function_type->returns_void = IS_VOID_TYPE(type);

	//Store the return type as well
	function_record->signature->internal_types.function_type->return_type = type;

	//Now that the function record has been finalized, we'll need to produce the type name
	generate_function_pointer_type_name(function_record->signature);

	//If we're dealing with the main function, we need to validate that the parameter order, visibility
	//of the function, and return type are valid
	if(is_main_function == TRUE && validate_main_function(function_record->signature) == FALSE){
		//Error out here
		return print_and_return_error("Invalid definition for main() function", parser_line_num);
	}

	//Some housekeeping, if there were previously deferred statements, we want them out
	deferred_stmts_node = NULL;

	//We are finally required to see a valid compound statement
	generic_ast_node_t* compound_stmt_node = compound_statement(fl);

	//If this fails we'll just pass it through
	if(compound_stmt_node->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		return compound_stmt_node;
	}

	//This function was defined
	function_record->defined = TRUE;

	//Where was this function defined
	function_record->line_number = current_line;

	//If this function is a void return type, we need to manually insert
	//a ret statement at the very end, if there isn't one already
	//Let's drill down to the very end
	generic_ast_node_t* cursor = compound_stmt_node->first_child;

	//We could have an entirely null function body
	if(cursor != NULL){
		//So long as we don't see ret statements here, we keep going
		while(cursor->next_sibling != NULL && cursor->ast_node_type != AST_NODE_TYPE_RET_STMT){
			//Advance
			cursor = cursor->next_sibling;
		}

		//If we get here we know that it worked, so we'll add it in as a child
		add_child_node(function_node, compound_stmt_node);
	
		//We now need to check and see if our jump statements are actually valid
		if(check_jump_labels() == FAILURE){
			//If this fails, we fail out here too
			return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
		}

	} else {
		sprintf(info, "Function %s has no body", function_record->func_name.string);
		print_parse_message(WARNING, info, parser_line_num);
	}

	//If this is the main funcition, it has been called implicitly
	if(is_main_function == TRUE){
		//Mark that it's been called
		function_record->called = TRUE;
		//Call it
		call_function(os, function_record->call_graph_node);
	}
	
	//We're done with this, so destroy it
	heap_queue_dealloc(&current_function_jump_statements);

	//Store the line number
	function_node->line_number = current_line;

	//Close the variable scope that we opened for the parameter list
	finalize_variable_scope(variable_symtab);

	//Remove the nesting level now that we're not in a function
	pop_nesting_level(&nesting_stack);

	//All good so we can get out
	return function_node;
}


/**
 * Handle a replace statement. A replace statement allows the programmer to eliminate any/all
 * magic numbers in the program. A replace statement is the only kind of statement that 
 *
 * Example:
 * #replace MY_INT with 2;
 */
static u_int8_t replace_statement(FILE* fl){
	//Lookahead token
	lexitem_t lookahead = get_next_token(fl, &parser_line_num);

	//If we failed, we're done here
	if(lookahead.tok != IDENT){
		print_parse_message(PARSE_ERROR, "Invalid identifier given to replace statement", parser_line_num);
		num_errors++;
		return FAILURE;
	}
	
	//Now that we have the ident, we need to make sure that it's not a duplicate
	//Let's get a pointer to the name for convenience
	dynamic_string_t name = lookahead.lexeme;

	//Check for function duplicates
	if(do_duplicate_functions_exist(name.string) == TRUE){
		return FAILURE;
	}

	//Check for type duplicates
	if(do_duplicate_types_exist(name.string) == TRUE){
		return FAILURE;
	}

	//Check that it isn't some duplicated variable name. We will only check in the
	//local scope for this one
	symtab_variable_record_t* found_var = lookup_variable_local_scope(variable_symtab, name.string);

	//Fail out here
	if(found_var != NULL){
		sprintf(info, "Attempt to redefine variable \"%s\". First defined here:", name.string);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_variable_name(found_var); num_errors++;
		return FAILURE;
	}

	//Let's see if we've already named a constant this
	symtab_constant_record_t* found_const = lookup_constant(constant_symtab, name.string);

	//Fail out if this isn't null
	if(found_const != NULL){
		sprintf(info, "Attempt to redefine constant \"%s\". First defined here:", name.string);
		print_parse_message(PARSE_ERROR, info, parser_line_num);
		//Also print out the original declaration
		print_constant_name(found_const);
		num_errors++;
		return FAILURE;
	}

	//We now need to see the with keyword
	lookahead = get_next_token(fl, &parser_line_num);

	//If we don't see it, then we're done here
	if(lookahead.tok != WITH){
		print_parse_message(PARSE_ERROR, "With keyword required in replace statement", parser_line_num);
		num_errors++;
		return FAILURE;
	}

	/**
	 * We now need to see a constant expression, but we will allow for some leniency
	 * by the logical or expression parsing. If a user types something like typesize(int)
	 * in, then that should still work for our replace statement
	 */
	generic_ast_node_t* constant_node = logical_or_expression(fl, SIDE_TYPE_RIGHT);

	switch(constant_node->ast_node_type){
		//THis is a straight failure, error has already happened
		case AST_NODE_TYPE_ERR_NODE:
			return FAILURE;

		//The one good case here
		case AST_NODE_TYPE_CONSTANT:
			break;

		//Anything else is invalid, we'll fail out if that's the case
		default:
			print_parse_message(PARSE_ERROR, "Replace statements must have an expression that simplifies to a single constant", parser_line_num);
			return FAILURE;
	}

	//One last thing, we need to see a semicolon
	lookahead = get_next_token(fl, &parser_line_num);

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
	lexitem_t lookahead;
	//The status
	u_int8_t status;

	//Grab the next token
	lookahead = get_next_token(fl, &parser_line_num);

	if(lookahead.tok == ERROR){
		print_parse_message(PARSE_ERROR, "Fatal error. Found error token\n", lookahead.line_num);
		num_errors++;
		return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
	}

	//Switch based on the token
	switch(lookahead.tok){
		//We can either see the "pub"(public) keyword or we can see a straight fn keyword
		case PUB:
		case FN:
			//Put the token back, we'll let the rule handle it
			push_back_token(lookahead);

			//We'll just let the function definition rule handle this. If it fails, 
			//that will be caught above
			return function_definition(fl);
	
		//Type definition
		case DEFINE:
			//Call the helper
			status = definition(fl);

			//If it's bad, we'll return an error node
			if(status == FAILURE){
				return print_and_return_error("Invalid definition statement", parser_line_num);
			}

			//Otherwise we'll just return null, the caller will know what to do with it
			return NULL;
			
		//Type aliasing
		case ALIAS:
			//Call the helper
			status = alias_statement(fl);

			//If it's bad, we'll return an error node
			if(status == FAILURE){
				return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
			}

			//Otherwise we'll just return null, the caller will know what to do with it
			return NULL;

		//Let the replace rule handle this
		case REPLACE:	
			//We don't need to put it back
			status = replace_statement(fl);

			//If it's bad, we'll return an error node
			if(status == FAILURE){
				return ast_node_alloc(AST_NODE_TYPE_ERR_NODE, SIDE_TYPE_LEFT);
			}

			//Otherwise we'll just return null, the caller will know what to do with it
			return NULL;

		//This is an error. The #dependencies directive must be the very first thing in a file
		case DEPENDENCIES:
			return print_and_return_error("The #dependencies section must be the very first thing in a file", parser_line_num);

		//This is out of place if we see it here
		case REQUIRE:
			return print_and_return_error("Any require statements must be nested in a top level #dependencies block", parser_line_num);

		default:
			//Put the token back
			push_back_token(lookahead);

			//We'll simply return whatever the product of the declaration function is
			//Do note: these variables will all be global
			return declaration(fl, TRUE);
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
	lexitem_t lookahead;

	//If prog is null we make it here
	if(prog == NULL){
		//Create the ROOT of the tree
		prog = ast_node_alloc(AST_NODE_TYPE_PROG, SIDE_TYPE_LEFT);
	}

	//Let's lookahead to see what we have
	lookahead = get_next_token(fl, &parser_line_num);

	//If we've actually found the comptime section, we'll
	//go through it until we don't have it anymore. The preprocessor
	//will have already consumed these tokens, so we need to get past them
	if(lookahead.tok == DEPENDENCIES){
		//Just run through here until we see the end of the comptime section
		lookahead = get_next_token(fl, &parser_line_num);

		//So long as we don't hit the end or the end of the region, keep going
		while(lookahead.tok != DEPENDENCIES && lookahead.tok != DONE){
			//Refresh the token
			lookahead = get_next_token(fl, &parser_line_num);
		}

		//If we get to the DONE, we error out
		if(lookahead.tok == DONE){
			return print_and_return_error("Unmatched #dependencies region detected", parser_line_num);
		}

	} else {
		//Put it back
		push_back_token(lookahead);
	}
	
	//As long as we aren't done
	while((lookahead = get_next_token(fl, &parser_line_num)).tok != DONE){
		//Put the token back
		push_back_token(lookahead);

		//Call declaration partition
		generic_ast_node_t* current = declaration_partition(fl);

		//If it was NULL, we had a define or alias statement or implicit function declaration, so we'll move along
		if(current == NULL){
			continue;
		}

		//It failed, we'll bail right out if this is the case
		if(current->ast_node_type == AST_NODE_TYPE_ERR_NODE){
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
front_end_results_package_t* parse(compiler_options_t* options){
	//Initialize our results package here
	front_end_results_package_t* results = calloc(1, sizeof(front_end_results_package_t));

	//Initialize the lexer first
	initialize_lexer();

	//Initialize the AST system as well
	initialize_ast_system();

	//Set the number of errors here
	num_errors = 0;
	num_warnings = 0;

	//Store the current file name
	current_file_name = options->file_name;
	//Store whether or not we want to do any debug printing
	enable_debug_printing = options->enable_debug_printing;

	//Open the file up
	FILE* fl = fopen(options->file_name, "r");

	//Error out if it's null
	if(fl == NULL){
		sprintf(info, "The file %s could not be found or opened", options->file_name);
		results->root = print_and_return_error(info, 0);
		//Give back the results structure
		return results;
	}

	function_symtab = function_symtab_alloc();
	variable_symtab = variable_symtab_alloc();
	type_symtab = type_symtab_alloc();
	constant_symtab = constants_symtab_alloc(); 

	//Initialize the OS call graph. This is because the OS always calls the main function
	os = calloc(1, sizeof(call_graph_node_t));
	
	//For the type and variable symtabs, their scope needs to be initialized before
	//anything else happens
	
	//Initialize the variable scope
	initialize_variable_scope(variable_symtab);
	//Global variable scope here
	initialize_type_scope(type_symtab);
	//Functions only have one scope, need no initialization

	//Add all basic types into the type symtab
	add_all_basic_types(type_symtab);

	//Keep these at hand because we use them so frequently, that repeatedly 
	//searching is needlessly expensive
	immut_char = lookup_type_name_only(type_symtab, "char", NOT_MUTABLE)->type;
	immut_u8 = lookup_type_name_only(type_symtab, "u8", NOT_MUTABLE)->type;
	immut_i8 = lookup_type_name_only(type_symtab, "i8", NOT_MUTABLE)->type;
	immut_u16 = lookup_type_name_only(type_symtab, "u16", NOT_MUTABLE)->type;
	immut_i16 = lookup_type_name_only(type_symtab, "i16", NOT_MUTABLE)->type;
	immut_u32 = lookup_type_name_only(type_symtab, "u32", NOT_MUTABLE)->type;
	immut_i32 = lookup_type_name_only(type_symtab, "i32", NOT_MUTABLE)->type;
	immut_u64 = lookup_type_name_only(type_symtab, "u64", NOT_MUTABLE)->type;
	immut_i64 = lookup_type_name_only(type_symtab, "i64", NOT_MUTABLE)->type;
	immut_f32 = lookup_type_name_only(type_symtab, "f32", NOT_MUTABLE)->type;
	immut_f64 = lookup_type_name_only(type_symtab, "f64", NOT_MUTABLE)->type;
	immut_void = lookup_type_name_only(type_symtab, "void", NOT_MUTABLE)->type;
	mut_void = lookup_type_name_only(type_symtab, "void", MUTABLE)->type;
	immut_char_ptr = lookup_type_name_only(type_symtab, "char*", NOT_MUTABLE)->type;

	//Also create a stack for our matching uses(curlies, parens, etc.)
	grouping_stack = lex_stack_alloc();
	//Create a stack for recording our depth/nesting levels
	nesting_stack = nesting_stack_alloc();

	//Global entry/run point, will give us a tree with
	//the root being here
	prog = program(fl);

	//We'll only perform these tests if we want debug printing enabled
	if(prog->ast_node_type != AST_NODE_TYPE_ERR_NODE){
		//Check for any unused functions
		check_for_unused_functions(function_symtab, &num_warnings);
		//Check for any bad variable declarations
		check_for_var_errors(variable_symtab, &num_warnings);
	}

	//Package up everything that we need
	results->function_symtab = function_symtab;
	results->variable_symtab = variable_symtab;
	results->type_symtab = type_symtab;
	results->constant_symtab = constant_symtab;
	results->grouping_stack = grouping_stack;
	//AST root
	results->root = prog;
	//Call graph OS root
	results->os = os;
	//Record how many errors that we had
	results->num_errors = num_errors;
	results->num_warnings = num_warnings;
	//How many lines did we process?
	results->lines_processed = parser_line_num;

	//Deallocate these when done
	lex_stack_dealloc(&grouping_stack);
	nesting_stack_dealloc(&nesting_stack);

	//Close the file out
	fclose(fl);

	//Once we're done, deinitialize the lexer
	deinitialize_lexer();

	return results;
}
