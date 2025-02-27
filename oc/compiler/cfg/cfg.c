/**
 * The implementation file for all CFG related operations
 *
 * The CFG will translate the higher level code into something referred to as 
 * "Ollie Intermediate Representation Language"(OIR). This intermediary form 
 * is a hybrid of abstract machine code and assembly. Some operations, like 
 * jump commands, are able to be deciphered at this stage, and as such we do
 * so in the OIR
*/

#include "cfg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>


//Our atomically incrementing integer
//If at any point a block has an ID of (-1), that means that it is in error and can be dealt with as such
static int32_t current_block_id = 0;
//Keep global references to the number of errors and warnings
u_int32_t* num_errors_ref;
u_int32_t* num_warnings_ref;
//Keep a stack of deferred statements for each function
heap_stack_t* deferred_stmts;
//Keep a variable symtab of temporary variables
variable_symtab_t* temp_vars;
//Keep the type symtab up and running
type_symtab_t* type_symtab;
//The CFG that we're working with
cfg_t* cfg_ref;

//A package of values that each visit function uses
typedef struct {
	//The initial node
	generic_ast_node_t* initial_node;
	basic_block_t* function_end_block;
	//For continue statements
	basic_block_t* loop_stmt_start;
	//For break statements
	basic_block_t* loop_stmt_end;
	basic_block_t* if_stmt_end_block;
	basic_block_t* for_loop_update_block;
} values_package_t;


//Define a package return struct that is used by the binary op expression code
typedef struct{
	three_addr_var_t* assignee;
	Token operator;
} expr_ret_package_t;


//We predeclare up here to avoid needing any rearrangements
static basic_block_t* visit_declaration_statement(values_package_t* values);
static basic_block_t* visit_compound_statement(values_package_t* values);
static basic_block_t* visit_let_statement(values_package_t* values);
static basic_block_t* visit_if_statement(values_package_t* values);
static basic_block_t* visit_while_statement(values_package_t* values);
static basic_block_t* visit_do_while_statement(values_package_t* values);
static basic_block_t* visit_for_statement(values_package_t* values);
static basic_block_t* visit_case_statement(values_package_t* values);
static basic_block_t* visit_default_statement(values_package_t* values);
static basic_block_t* visit_switch_statement(values_package_t* values);

//Return a three address code variable
static expr_ret_package_t emit_binary_op_expr_code(basic_block_t* basic_block, generic_ast_node_t* logical_or_expr);
static three_addr_var_t* emit_function_call_code(basic_block_t* basic_block, generic_ast_node_t* function_call_node);

//An enum for jump types
typedef enum{
	JUMP_CATEGORY_INVERSE,
	JUMP_CATEGORY_NORMAL,
} jump_category_t;

//A type for which side we're on
typedef enum{
	SIDE_TYPE_LEFT,
	SIDE_TYPE_RIGHT,
} side_type_t;

//An enum for temp variable selection
typedef enum{
	USE_TEMP_VAR,
	PRESERVE_ORIG_VAR,
} temp_selection_t;

/**
 * Select the appropriate jump type to use. We can either use
 * inverse jumps or direct jumps
 */
static jump_type_t select_appropriate_jump_stmt(Token operator, jump_category_t jump_type){
	//Let's see what we have here
	switch(operator){
		case G_THAN:
			if(jump_type == JUMP_CATEGORY_INVERSE){
				return JUMP_TYPE_JLE;
			} else {
				return JUMP_TYPE_JG;
			}
		case L_THAN:
			if(jump_type == JUMP_CATEGORY_INVERSE){
				return JUMP_TYPE_JGE;
			} else {
				return JUMP_TYPE_JL;
			}
		case L_THAN_OR_EQ:
			if(jump_type == JUMP_CATEGORY_INVERSE){
				return JUMP_TYPE_JG;
			} else {
				return JUMP_TYPE_JLE;
			}
		case G_THAN_OR_EQ:
			if(jump_type == JUMP_CATEGORY_INVERSE){
				return JUMP_TYPE_JL;
			} else {
				return JUMP_TYPE_JGE;
			}
		case D_EQUALS:
			if(jump_type == JUMP_CATEGORY_INVERSE){
				return JUMP_TYPE_JNE;
			} else {
				return JUMP_TYPE_JE;
			}
		case NOT_EQUALS:
			if(jump_type == JUMP_CATEGORY_INVERSE){
				return JUMP_TYPE_JE;
			} else {
				return JUMP_TYPE_JE;
			}


		//If we get here, it was some kind of
		//non relational operator. In this case,
		//we default to 0 = false non zero = true
		default:
			if(jump_type == JUMP_CATEGORY_INVERSE){
				return JUMP_TYPE_JZ;
			} else {
				return JUMP_TYPE_JNZ;
			}
	}
}


/**
 * Simply prints a parse message in a nice formatted way. For the CFG, there
 * are no parser line numbers
*/
static void print_cfg_message(parse_message_type_t message_type, char* info, u_int16_t line_number){
	//Build and populate the message
	parse_message_t parse_message;
	parse_message.message = message_type;
	parse_message.info = info;

	//Fatal if error
	if(message_type == PARSE_ERROR){
		parse_message.fatal = 1;
	}

	//Now print it
	//Mapped by index to the enum values
	char* type[] = {"WARNING", "ERROR", "INFO"};

	//Print this out on a single line
	fprintf(stderr, "\n[LINE %d: COMPILER %s]: %s\n", line_number, type[parse_message.message], parse_message.info);
}


/**
 * A simple helper function that allows us to add a live variable into the block's
 * header. It is important to note that only actual variables(not temp variables) count
 * as live
 */
static void add_live_variable(basic_block_t* basic_block, three_addr_var_t* var){
	//Just check this for safety--for dev use only
	if(basic_block->active_var_count == MAX_LIVE_VARS){
		fprintf(stderr, "MAXIMUM VARIABLE COUNT EXCEEDED\n");
		exit(0);
	}

	//Add the given variable in
	basic_block->active_vars[basic_block->active_var_count] = var;
	//We've seen one more
	(basic_block->active_var_count)++;
}


/**
 * Print a block our for reading
*/
static void pretty_print_block(basic_block_t* block){
	//If this is empty, don't print anything
	if(block->leader_statement == NULL){
		return;
	}

	//Print the block's ID or the function name
	if(block->is_func_entry == 1){
		printf("%s:\n", block->func_record->func_name);
	} else {
		printf(".L%d:\n", block->block_id);
	}

	//Now grab a cursor and print out every statement that we 
	//have
	three_addr_code_stmt_t* cursor = block->leader_statement;

	//So long as it isn't null
	while(cursor != NULL){
		//Hand off to printing method
		print_three_addr_code_stmt(cursor);
		//Move along to the next one
		cursor = cursor->next_statement;
	}

	//Some spacing
	printf("\n");
}


/**
 * Add a statement to the target block, following all standard linked-list protocol
 */
static void add_statement(basic_block_t* target, three_addr_code_stmt_t* statement_node){
	//Generic fail case
	if(target == NULL){
		print_parse_message(PARSE_ERROR, "NULL BASIC BLOCK FOUND", 0);
		exit(1);
	}

	//Special case--we're adding the head
	if(target->leader_statement == NULL || target->exit_statement == NULL){
		//Assign this to be the head and the tail
		target->leader_statement = statement_node;
		target->exit_statement = statement_node;
		return;
	}

	//Otherwise, we are not dealing with the head. We'll simply tack this on to the tail
	target->exit_statement->next_statement = statement_node;
	//Update the tail reference
	target->exit_statement = statement_node;
}



/**
 * if(x0 == 0){
 * 	asn x1 := 2;
 * } else {
 * 	asn x2 := 3;
 * }
 * 
 * x3 <- phi(x1, x2)
 *
 * This means that x3 is x1 if it comes from the first branch and x2 if it comes
 * from the second branch
 *
 * To insert phi functions, we take the following approach:
 * 	For each variable
 * 		Find all basic blocks that define this variable
 * 		For each of these basic blocks
 * 			Find its dominance frontiers
 * 			Insert phi for each of the dominance frontiers
 *
 */
static void insert_phi_functions(basic_block_t* starting_block, variable_symtab_t* var_symtab){

}


/**
 * Directly emit the assembly code for an inlined statement. Users who write assembly inline
 * want it directly inserted in order, nothing more, nothing less
 */
static void emit_asm_inline_stmt(basic_block_t* basic_block, generic_ast_node_t* asm_inline_node){
	//First we allocate the whole thing
	three_addr_code_stmt_t* asm_inline_stmt = emit_asm_statement_three_addr_code(asm_inline_node->node); 
	
	//Once done we add it into the block
	add_statement(basic_block, asm_inline_stmt);
	
	//And that's all
}


/**
 * Emit the abstract machine code for a return statement
 */
static void emit_ret_stmt(basic_block_t* basic_block, generic_ast_node_t* ret_node){
	//For holding our temporary return variable
	expr_ret_package_t package;

	//Is null by default
	package.assignee = NULL;

	//If the ret node's first child is not null, we'll let the expression rule
	//handle it
	if(ret_node->first_child != NULL){
		package = emit_binary_op_expr_code(basic_block, ret_node->first_child);

	}

	//We'll use the ret stmt feature here
	three_addr_code_stmt_t* ret_stmt = emit_ret_stmt_three_addr_code(package.assignee);

	//Once it's been emitted, we'll add it in as a statement
	add_statement(basic_block, ret_stmt);
}


/**
 * Emit the abstract machine code for a label statement
 */
static void emit_label_stmt_code(basic_block_t* basic_block, generic_ast_node_t* label_node){
	//Emit the appropriate variable
	three_addr_var_t* label_var = emit_var(label_node->variable, 0, 1);

	//We'll just use the helper to emit this
	three_addr_code_stmt_t* stmt = emit_label_stmt_three_addr_code(label_var);

	//Add this statement into the block
	add_statement(basic_block, stmt);
}


/**
 * Emit the abstract machine code for a jump statement
 */
static void emit_jump_stmt_code(basic_block_t* basic_block, generic_ast_node_t* jump_statement){
	//Emit the appropriate variable
	three_addr_var_t* label_var = emit_var(jump_statement->variable, 0, 1);

	//We'll just use the helper to do this
	three_addr_code_stmt_t* stmt = emit_dir_jmp_stmt_three_addr_code(label_var);

	//Add this statement into the block
	add_statement(basic_block, stmt);
}


/**
 * Emit a jump statement jumping to the destination block, using the jump type that we
 * provide
 */
static void emit_jmp_stmt(basic_block_t* basic_block, basic_block_t* dest_block, jump_type_t type){
	//Use the helper function to emit the statement
	three_addr_code_stmt_t* stmt = emit_jmp_stmt_three_addr_code(dest_block->block_id, type);

	//Add this into the first block
	add_statement(basic_block, stmt);
}


/**
 * Emit an unconditional jump statement to a label as opposed to a block ID. This is used for
 * when the user wishes to jump directly
 */
static void emit_jmp_stmt_direct(basic_block_t* basic_block, generic_ast_node_t* jump_node){

}


/**
 * Emit the abstract machine code for a constant to variable assignment. 
 */
static three_addr_var_t* emit_constant_code(basic_block_t* basic_block, generic_ast_node_t* constant_node){
	//We'll use the constant var feature here
	three_addr_code_stmt_t* const_var = emit_assn_const_stmt_three_addr_code(emit_temp_var(constant_node->inferred_type), emit_constant(constant_node));
	
	//Add this into the basic block
	add_statement(basic_block, const_var);

	//Now give back the assignee variable
	return const_var->assignee;
}


/**
 * Emit the abstract machine code for a constant to variable assignment. 
 */
static three_addr_var_t* emit_constant_code_direct(basic_block_t* basic_block, three_addr_const_t* constant, generic_type_t* inferred_type){
	//We'll use the constant var feature here
	three_addr_code_stmt_t* const_var = emit_assn_const_stmt_three_addr_code(emit_temp_var(inferred_type), constant);
	
	//Add this into the basic block
	add_statement(basic_block, const_var);

	//Now give back the assignee variable
	return const_var->assignee;
}


/**
 * Emit the identifier machine code. This function is to be used in the instance where we want
 * to move an identifier to some temporary location
 */
static three_addr_var_t* emit_ident_expr_code(basic_block_t* basic_block, generic_ast_node_t* ident_node, temp_selection_t use_temp, side_type_t side){
	//Just give back the name
	if(use_temp == PRESERVE_ORIG_VAR || side == SIDE_TYPE_RIGHT){
		//If it's an enum constant
		if(ident_node->variable->is_enumeration_member == 1){
			return emit_constant_code_direct(basic_block, emit_int_constant_direct(ident_node->variable->enum_member_value), lookup_type(type_symtab, "u32")->type);
		}

		//Emit the variable
		three_addr_var_t* var = emit_var(ident_node->variable, use_temp, 0);
		
		//Add it as a live variable to the block
		add_live_variable(basic_block, var);

		//Give it back
		return var;

	//We will do an on-the-fly conversion to a number
	} else if(ident_node->inferred_type->type_class == TYPE_CLASS_ENUMERATED) {
		symtab_type_record_t* type_record = lookup_type(type_symtab, "u32");
		generic_type_t* type = type_record->type;
		return emit_constant_code_direct(basic_block, emit_int_constant_direct(ident_node->variable->enum_member_value), type);

	} else {
		//First we'll create the non-temp var here
		three_addr_var_t* non_temp_var = emit_var(ident_node->variable, 0, 0);

		//Add it into the block
		add_live_variable(basic_block, non_temp_var);

		//Let's first create the assignment statement
		three_addr_code_stmt_t* temp_assnment = emit_assn_stmt_three_addr_code(emit_temp_var(ident_node->inferred_type), non_temp_var);

		//Add the statement in
		add_statement(basic_block, temp_assnment);

		//Just give back the temp var here
		return temp_assnment->assignee;
	}
}


/**
 * Emit increment three adress code
 */
static three_addr_var_t* emit_inc_code(basic_block_t* basic_block, three_addr_var_t* incrementee){
	//Create the code
	three_addr_code_stmt_t* inc_code = emit_inc_stmt_three_addr_code(incrementee);

	//Add it into the block
	add_statement(basic_block, inc_code);

	//Return the incrementee
	return incrementee;
}


/**
 * Emit decrement three address code
 */
static three_addr_var_t* emit_dec_code(basic_block_t* basic_block, three_addr_var_t* decrementee){
	//Create the code
	three_addr_code_stmt_t* dec_code = emit_inc_stmt_three_addr_code(decrementee);

	//Add it into the block
	add_statement(basic_block, dec_code);

	//Return the incrementee
	return decrementee;
}


/**
 * Emit memory indirection three address code
 */
static three_addr_var_t* emit_mem_code(basic_block_t* basic_block, three_addr_var_t* assignee){
	//No actual code here, we are just accessing this guy's memory
	//Create a new variable with an indirection level
	three_addr_var_t* indirect_var = emit_var_copy(assignee);

	//Increment the indirection
	indirect_var->indirection_level = assignee->indirection_level++;
	//Temp or not same deal
	indirect_var->is_temporary = assignee->is_temporary;
	
	//We'll wrap with parens
	char indirect_var_name[115];

	sprintf(indirect_var_name, "(%s)", indirect_var->var_name);

	//Now we'll overwrite the old name
	strcpy(indirect_var->var_name, indirect_var_name);

	//And get out
	return indirect_var;
}


/**
 * Emit a bitwise not statement 
 */
static three_addr_var_t* emit_bitwise_not_expr_code(basic_block_t* basic_block, three_addr_var_t* var, temp_selection_t use_temp){
	//First we'll create it here
	three_addr_code_stmt_t* not_stmt = emit_not_stmt_three_addr_code(var);

	//Now if we need to use a temp, we'll make one here
	if(use_temp == USE_TEMP_VAR){
		//Emit a temp var
		three_addr_var_t* temp = emit_temp_var(var->type);

		//The assignee is the temp
		not_stmt->assignee = temp;
	}
	//Otherwise nothing else needed here

	//Add this into the block
	add_statement(basic_block, not_stmt);

	//Give back the assignee
	return not_stmt->assignee;
}


/**
 * Emit a binary operation statement with a constant built in
 */
static three_addr_var_t* emit_binary_op_with_constant_code(basic_block_t* basic_block, three_addr_var_t* assignee, three_addr_var_t* op1, Token op, three_addr_const_t* constant){
	//First let's create it
	three_addr_code_stmt_t* stmt = emit_bin_op_with_const_three_addr_code(assignee, op1, op, constant);

	//Then we'll add it into the block
	add_statement(basic_block, stmt);

	//Finally we'll return it
	return assignee;
}


/**
 * Emit a bitwise negation statement
 */
static three_addr_var_t* emit_neg_stmt_code(basic_block_t* basic_block, three_addr_var_t* negated, temp_selection_t use_temp){
	three_addr_var_t* var;

	//We make our temp selection based on this
	if(use_temp == USE_TEMP_VAR){
		var = emit_temp_var(negated->type);
	} else {
		var = negated;
	}

	//Now let's create it
	three_addr_code_stmt_t* stmt = emit_neg_stmt_three_addr_code(var, negated);
	
	//Add it into the block
	add_statement(basic_block, stmt);

	//We always return the assignee
	return var;
}


/**
 * Emit a logical negation statement
 */
static three_addr_var_t* emit_logical_neg_stmt_code(basic_block_t* basic_block, three_addr_var_t* negated){
	//We ALWAYS use a temp var here
	three_addr_code_stmt_t* stmt = emit_logical_not_stmt_three_addr_code(emit_temp_var(negated->type), negated);

	//From here, we'll add the statement in
	add_statement(basic_block, stmt);

	//We'll give back the assignee temp variable
	return stmt->assignee;
}


/**
 * Emit the abstract machine code for a unary expression
 * Unary expressions come in the following forms:
 * 	
 * 	<postfix-expression> | <unary-operator> <cast-expression> | typesize(<type-specifier>) | sizeof(<logical-or-expression>) 
 */
static three_addr_var_t* emit_unary_expr_code(basic_block_t* basic_block, generic_ast_node_t* unary_expr_parent, temp_selection_t use_temp, side_type_t side){
	//The last two instances return a constant node. If that's the case, we'll just emit a constant
	//node here
	if(unary_expr_parent->CLASS == AST_NODE_CLASS_CONSTANT){
		//Let the helper deal with it
		return emit_constant_code(basic_block, unary_expr_parent);
	}

	//If it isn't a constant, then this node should have children
	generic_ast_node_t* first_child = unary_expr_parent->first_child;

	//This could be a postfix expression
	if(first_child->CLASS == AST_NODE_CLASS_POSTFIX_EXPR){
		
	//If we have some kind of unary operator here
	} else if(first_child->CLASS == AST_NODE_CLASS_UNARY_OPERATOR){
		//Grab this internal reference for ease
		generic_ast_node_t* unary_operator = first_child;

		//No matter what here, the next sibling will also be some kind of unary expression.
		//We'll need to handle that first before going forward
		three_addr_var_t* assignee = emit_unary_expr_code(basic_block, first_child->next_sibling, use_temp, side);

		//What kind of unary operator do we have?
		//Handle plus plus case
		if(unary_operator->unary_operator == PLUSPLUS){
			//What if the assignee is a complex type(pointer, array, etc)
			if(assignee->type->type_class != TYPE_CLASS_BASIC){
				//Emit the constant size
				three_addr_const_t* constant = emit_int_constant_direct(assignee->type->type_size);
				//Now we'll make the statement
				return emit_binary_op_with_constant_code(basic_block, assignee, assignee, PLUS, constant);
			} else {
				//We really just have an "inc" instruction here
				return emit_inc_code(basic_block, assignee);
			}
		} else if(unary_operator->unary_operator == MINUSMINUS){
			//What if the assignee is a complex type(pointer, array, etc)
			if(assignee->type->type_class != TYPE_CLASS_BASIC){
				//Emit the constant size
				three_addr_const_t* constant = emit_int_constant_direct(assignee->type->type_size);
				//Now we'll make the statement
				return emit_binary_op_with_constant_code(basic_block, assignee, assignee, MINUS, constant);
			} else {
				//We really just have an "inc" instruction here
				return emit_dec_code(basic_block, assignee);
			}
		} else if (unary_operator->unary_operator == STAR){
			//Memory address
			return emit_mem_code(basic_block, assignee);
		} else if (unary_operator->unary_operator == B_NOT){
			//Bitwise not -- this does need to be assigned from
			return emit_bitwise_not_expr_code(basic_block, assignee, use_temp);

		/**
		 * Uses strategy of:
		 * 	test rdx, rdx
		 * 	sete rdx
		 * 	mov rdx, rdx //this specifically exists to set flags
		 * for implementation
		 */
		} else if(unary_operator->unary_operator == L_NOT){
			return emit_logical_neg_stmt_code(basic_block, assignee);

		/**
		 * x = -a;
		 * t <- a;
		 * negl t;
		 * x <- t;
		 *
		 * Uses strategy of: negl rdx
		 */
		} else if(unary_operator->unary_operator == MINUS){
			//We will emit the negation code here
			return emit_neg_stmt_code(basic_block, assignee, use_temp);
		}

		//FOR NOW ONLY
		return assignee;

	//OR it could be a primary expression, which has a whole host of options
	} else if(first_child->CLASS == AST_NODE_CLASS_IDENTIFIER){
		//If it's an identifier, emit this and leave
		 return emit_ident_expr_code(basic_block, first_child, use_temp, side);
	//If it's a constant, emit this and leave
	} else if(first_child->CLASS == AST_NODE_CLASS_CONSTANT){
		return emit_constant_code(basic_block, first_child);
	} else if(first_child->CLASS == AST_NODE_CLASS_BINARY_EXPR){
		return emit_binary_op_expr_code(basic_block, first_child).assignee;
	//Handle a function call
	} else if(first_child->CLASS == AST_NODE_CLASS_FUNCTION_CALL){
		return emit_function_call_code(basic_block, first_child);
	}

	//FOR NOW
	return NULL;
}


/**
 * Emit the abstract machine code needed for a binary expression. The lowest possible
 * thing that we could have here is a unary expression. If we have that, we just emit the
 * unary expression
 *
 * We need to convert these into straight line binary expression code(two operands, one operator) each.
 * For each binary expression, we compute
 *
 */
static expr_ret_package_t emit_binary_op_expr_code(basic_block_t* basic_block, generic_ast_node_t* logical_or_expr){
	//The return package here
	expr_ret_package_t package;
	//Operator is blank by default
	package.operator = BLANK;

	//Is the cursor a unary expression? If so just emit that. This is our base case 
	//essentially
	if(logical_or_expr->CLASS == AST_NODE_CLASS_UNARY_EXPR){
		//Return the temporary character from here
		package.assignee = emit_unary_expr_code(basic_block, logical_or_expr, USE_TEMP_VAR, SIDE_TYPE_RIGHT);
		return package;
	}

	//Otherwise we actually have a binary operation of some kind
	//Grab a cursor
	generic_ast_node_t* cursor = logical_or_expr->first_child;
	
	//Emit the binary expression on the left first
	expr_ret_package_t left_hand_temp = emit_binary_op_expr_code(basic_block, cursor);

	//Advance up here
	cursor = cursor->next_sibling;

	//Then grab the right hand temp
	expr_ret_package_t right_hand_temp = emit_binary_op_expr_code(basic_block, cursor);

	//Let's see what binary operator that we have
	Token binary_operator = logical_or_expr->binary_operator;
	//Store this binary operator
	package.operator = binary_operator;

	//Emit the binary operator expression using our helper
	three_addr_code_stmt_t* bin_op_stmt = emit_bin_op_three_addr_code(emit_temp_var(logical_or_expr->inferred_type), left_hand_temp.assignee, binary_operator, right_hand_temp.assignee);

	//Add this statement to the block
	add_statement(basic_block, bin_op_stmt);

	//Store the temporary var as the assignee
	package.assignee = bin_op_stmt->assignee;
	
	//Return the temp variable that we assigned to
	return package;
}


/**
 * Emit abstract machine code for an expression. This is a top level statement.
 * These statements almost always involve some kind of assignment "<-" and generate temporary
 * variables
 */
static expr_ret_package_t emit_expr_code(basic_block_t* basic_block, generic_ast_node_t* expr_node){
	//A cursor for tree traversal
	generic_ast_node_t* cursor;
	symtab_variable_record_t* assigned_var;
	//The return package
	expr_ret_package_t ret_package;
	//By default, last seen op is blank
	ret_package.operator = BLANK;

	//If we have a declare statement,
	if(expr_node->CLASS == AST_NODE_CLASS_DECL_STMT){
		//What kind of declarative statement do we have here?
		//TODO


	//Convert our let statement into abstract machine code 
	} else if(expr_node->CLASS == AST_NODE_CLASS_LET_STMT){
		//Let's grab the associated variable record here
		symtab_variable_record_t* var =  expr_node->variable;

		//Create the variable associated with this
	 	three_addr_var_t* left_hand_var = emit_var(var, 1, 0);

		//Add it in as a live variable
		add_live_variable(basic_block, left_hand_var);

		//Now emit whatever binary expression code that we have
		expr_ret_package_t package = emit_binary_op_expr_code(basic_block, expr_node->first_child);

		//The actual statement is the assignment of right to left
		three_addr_code_stmt_t* assn_stmt = emit_assn_stmt_three_addr_code(left_hand_var, package.assignee);

		//Finally we'll add this into the overall block
		add_statement(basic_block, assn_stmt);
	
	//An assignment statement
	} else if(expr_node->CLASS == AST_NODE_CLASS_ASNMNT_EXPR) {
		//In our tree, an assignment statement decays into a unary expression
		//on the left and a binary op expr on the right
		
		//This should always be a unary expression
		cursor = expr_node->first_child;

		//If it is not one, we fail out
		if(cursor->CLASS != AST_NODE_CLASS_UNARY_EXPR){
			print_parse_message(PARSE_ERROR, "Expected unary expression as first child to assignment expression", cursor->line_number);
			exit(0);
		}

		//Emit the left hand unary expression
		three_addr_var_t* left_hand_var = emit_unary_expr_code(basic_block, cursor, PRESERVE_ORIG_VAR, SIDE_TYPE_LEFT);

		//Advance the cursor up
		cursor = cursor->next_sibling;

		//Now emit the right hand expression
		expr_ret_package_t package = emit_binary_op_expr_code(basic_block, cursor);

		//Finally we'll construct the whole thing
		three_addr_code_stmt_t* stmt = emit_assn_stmt_three_addr_code(left_hand_var, package.assignee);
		
		//Now add this statement in here
		add_statement(basic_block, stmt);

		//Now pack the return value here
		ret_package.operator = BLANK;
		ret_package.assignee = left_hand_var;
		
		//Return what we had
		return ret_package;

	} else if(expr_node->CLASS == AST_NODE_CLASS_BINARY_EXPR){
		//Emit the binary expression node
		return emit_binary_op_expr_code(basic_block, expr_node);
	} else if(expr_node->CLASS == AST_NODE_CLASS_FUNCTION_CALL){
		//Emit the function call statement
		ret_package.assignee = emit_function_call_code(basic_block, expr_node);
		return ret_package;
	} else if(expr_node->CLASS == AST_NODE_CLASS_UNARY_EXPR){
		//Let this rule handle it
		ret_package.assignee = emit_unary_expr_code(basic_block, expr_node, PRESERVE_ORIG_VAR, SIDE_TYPE_RIGHT);
		return ret_package;
	} else {
		return ret_package;

	}

	return ret_package;
}


/**
 * Emit a function call node. In this iteration of a function call, we will still be parameterized, so the actual 
 * node will record what needs to be passed into the function
 */
static three_addr_var_t* emit_function_call_code(basic_block_t* basic_block, generic_ast_node_t* function_call_node){
	//Grab this out first
	symtab_function_record_t* func_record = ((function_call_ast_node_t*)(function_call_node->node))->func_record;

	//May be NULL or not based on what we have as the return type
	three_addr_var_t* assignee = NULL;

	//If the function does not return void, we will be assigning it to a temporary variable
	if(strcmp(func_record->return_type->type_name, "void") != 0){
		//This means that we have a temp variable here
		assignee = emit_temp_var(func_record->return_type);
	}

	//Once we get here we can create the function statement
	three_addr_code_stmt_t* func_call_stmt = emit_func_call_three_addr_code(func_record, assignee);

	//Let's grab a param cursor for ourselves
	generic_ast_node_t* param_cursor = function_call_node->first_child;

	//The current param of the index
	u_int8_t current_func_param_idx = 0;

	//So long as this isn't NULL
	while(param_cursor != NULL){
		//Emit whatever we have here into the basic block
		expr_ret_package_t package = emit_expr_code(basic_block, param_cursor);
		
		//The temporary variable that we get will be our parameter
		func_call_stmt->params[current_func_param_idx] = package.assignee;
		current_func_param_idx++;

		//And move up
		param_cursor = param_cursor->next_sibling;
	}

	//Once we make it here, we should have all of the params stored in temp vars
	//We can now add the function call statement in
	add_statement(basic_block, func_call_stmt);

	//Give back what we assigned to
	return assignee;
}


/**
 * A helper function that makes a new block id. This ensures we have an atomically
 * increasing block ID
 */
static int32_t increment_and_get(){
	current_block_id++;
	return current_block_id;
}

/**
 * Allocate a basic block using calloc. NO data assignment
 * happens in this function
*/
static basic_block_t* basic_block_alloc(){
	//Allocate the block
	basic_block_t* created = calloc(1, sizeof(basic_block_t));

	//Put the block ID in
	created->block_id = increment_and_get();

	//Attach this to the memory management structure
	created->next_created = cfg_ref->last_attached;
	cfg_ref->last_attached = created;

	return created;
}


/**
 * Print out the whole program in order. This is done using an
 * iterative DFS
 */
static void emit_blocks_dfs(cfg_t* cfg){
	//We'll need a stack for our DFS
	heap_stack_t* stack = create_stack();

	//The idea here is very simple. If we can walk the function tree and every control path leads 
	//to a return statement, we return null from every control path
	
	//We'll need a cursor to walk the tree
	basic_block_t* block_cursor;

	//Push the source node
	push(stack, cfg->root);

	//So long as the stack is not empty
	while(is_empty(stack) == 0){
		//Grab the current one off of the stack
		block_cursor = pop(stack);

		//If this wasn't visited
		if(block_cursor->visited != 2){
			//Mark this one as seen
			block_cursor->visited = 2;

			pretty_print_block(block_cursor);
		}

		//We'll now add in all of the childen
		for(int8_t i = block_cursor->num_successors-1; i > -1; i--){
			//If we haven't seen it yet, add it to the list
			if(block_cursor->successors[i]->visited != 2){
				push(stack, block_cursor->successors[i]);
			}
		}
	}

	//Deallocate our stack once done
	destroy_stack(stack);
}


/**
 * Deallocate a basic block
*/
static void basic_block_dealloc(basic_block_t* block){
	//Just in case
	if(block == NULL){
		printf("ERROR: Attempt to deallocate a null block");
		exit(1);
	}

	//Grab a statement cursor here
	three_addr_code_stmt_t* cursor = block->leader_statement;
	//We'll need a temp block too
	three_addr_code_stmt_t* temp = cursor;

	//So long as the cursor is not NULL
	while(cursor != NULL){
		temp = cursor;
		cursor = cursor->next_statement;
		//Destroy temp
		deallocate_three_addr_stmt(temp);
	}
	

	//Otherwise its fine so
	free(block);
}


/**
 * Memory management code that allows us to deallocate the entire CFG
 */
void dealloc_cfg(cfg_t* cfg){
	//Hold a cursor here
	basic_block_t* cursor = cfg->last_attached;
	//Have a temp too
	basic_block_t* temp;

	//So long as there is stuff to free
	while(cursor != NULL){
		//Hold onto this
		temp = cursor;
		//Advance this one up
		cursor = cursor->next_created;
		//Destroy the block
		basic_block_dealloc(temp);
	}

	//Destroy all variables
	deallocate_all_vars();
	//Destroy all constants
	deallocate_all_consts();

	//At the very end, be sure to destroy this too
	free(cfg);
}


/**
 * Helper for returning error blocks. Error blocks always have an ID of -1
 */
static basic_block_t* create_and_return_err(){
	//Create the error
	basic_block_t* err_block = basic_block_alloc();
	//Set the ID to -1
	err_block->block_id = -1;

	return err_block;
}


/**
 * Add a successor to the target block
 */
static void add_successor(basic_block_t* target, basic_block_t* successor){
	//Let's check this
	if(target->num_successors == MAX_SUCCESSORS){
		//Internal error for the programmer
		printf("CFG ERROR. YOU MUST INCREASE THE NUMBER OF SUCCESSORS");
		exit(1);
	}

	//If there are no successors here, add it in as the direct one
	if(target->num_successors == 0){
		target->direct_successor = successor;
	}

	//Otherwise we're set here
	//Add this in
	target->successors[target->num_successors] = successor;
	//Increment how many we have
	(target->num_successors)++;

	//Let's check this
	if(successor->num_predecessors == MAX_PREDECESSORS){
		//Internal error for the programmer
		printf("CFG ERROR. YOU MUST INCREASE THE NUMBER OF PREDECESSORS");
		exit(1);
	}

	//Otherwise we're set here
	//Add this in
	successor->predecessors[successor->num_predecessors] = target;
	//Increment how many we have
	(successor->num_predecessors)++;
}


/**
 * Merge two basic blocks. We always return a pointer to a, b will be deallocated
 *
 * IMPORTANT NOTE: ONCE BLOCKS ARE MERGED, BLOCK B IS GONE
 */
static basic_block_t* merge_blocks(basic_block_t* a, basic_block_t* b){
	//Just to double check
	if(a == NULL){
		print_cfg_message(PARSE_ERROR, "Fatal error. Attempting to merge null block", 0);
		exit(1);
	}

	//If b is null, we just return a
	if(b == NULL){
		return a;
	}

	//What if a was never even assigned?
	if(a->exit_statement == NULL){
		a->leader_statement = b->leader_statement;
		a->exit_statement = b->exit_statement;
	} else {
		//Otherwise it's a "true merge"
		//The leader statement in b will be connected to a's tail
		a->exit_statement->next_statement = b->leader_statement;
		//Now once they're connected we'll set a's exit to be b's exit
		a->exit_statement = b->exit_statement;
	}

	//If we're gonna merge two blocks, then they'll share all the same successors and predecessors
	//Let's merge predecessors first
	for(u_int8_t i = 0; i < b->num_predecessors; i++){
		//Tie it in
		a->predecessors[a->num_predecessors] = b->predecessors[i];
		//Increment how many predecessors a has
		(a->num_predecessors)++;
	}

	//Now merge successors
	for(u_int8_t i = 0; i < b->num_successors; i++){
		//Tie it in
		a->successors[a->num_successors] = b->successors[i];
		//Increment how many successors a has
		(a->num_successors)++;
	}

	//Also make note of any direct succession
	a->direct_successor = b->direct_successor;
	a->is_exit_block = b->is_exit_block;
	//If we're merging return statements
	a->is_return_stmt = b->is_return_stmt;

	//IMPORTANT--wipe b's statements out
	b->leader_statement = NULL;
	b->exit_statement = NULL;

	//Give back the pointer to a
	return a;
}


/**
 * We will perform reachability analysis on the function CFG. We wish to know if the function
 * returns from every control path
 */
static void perform_function_reachability_analysis(generic_ast_node_t* function_node, basic_block_t* entry_block){
	//For error printing
	char info[1000];
	//The number of dead-ends
	u_int32_t dead_ends = 0;

	//If the function returns void, there is no need for any reachability analysis, it will return when 
	//the function runs off anyway
	if(strcmp(((func_def_ast_node_t*)(function_node->node))->func_record->return_type->type_name, "void") == 0){
		return;
	}

	//We'll need a stack for our DFS
	heap_stack_t* stack = create_stack();

	//The idea here is very simple. If we can walk the function tree and every control path leads 
	//to a return statement, we return null from every control path
	
	//We'll need a cursor to walk the tree
	basic_block_t* block_cursor;

	//Push the source node
	push(stack, entry_block);

	//So long as the stack is not empty
	while(is_empty(stack) == 0){
		//Grab the current one off of the stack
		block_cursor = pop(stack);

		//If this wasn't visited
		if(block_cursor->visited == 0){
			//Mark this one as seen
			block_cursor->visited = 1;

			/**
			 * Now we can perform our checks. 
			 */
			//If the direct successor is the exit, but it's not a return statement
			if(block_cursor->direct_successor != NULL && block_cursor->direct_successor->is_exit_block == 1
			  && block_cursor->is_return_stmt == 0){
				//One more dead end
				dead_ends++;
				//Go to the next iteration
				continue;
			}

			//If it is a return statement none of its children are relevant
			if(block_cursor->is_return_stmt == 1){
				continue;
			}
		}
		//We'll now add in all of the childen
		for(u_int8_t i = 0; i < block_cursor->num_successors; i++){
			//If we haven't seen it yet, add it to the list
			if(block_cursor->successors[i]->visited == 0){
				push(stack, block_cursor->successors[i]);
			}
		}
	}
	
	//Once we escape our while loop, we can actually see what the analysis said
	if(dead_ends > 0){
		//Extract the function name
		char* func_name = ((func_def_ast_node_t*)(function_node->node))->func_record->func_name;
		sprintf(info, "Non-void function \"%s\" does not return a value in all control paths", func_name);
		print_cfg_message(WARNING, info, function_node->line_number);
		(*num_warnings_ref)+=dead_ends;
	}

	//Destroy the stack once we're done
	destroy_stack(stack);
}


/**
 * A for-statement is another kind of control flow construct. As always the direct successor is the path that reliably
 * leads us down and out
 */
static basic_block_t* visit_for_statement(values_package_t* values){
	//Create our entry block
	basic_block_t* for_stmt_entry_block = basic_block_alloc();
	//Create our exit block
	basic_block_t* for_stmt_exit_block = basic_block_alloc();
	
	//Grab the reference to the for statement node
	generic_ast_node_t* for_stmt_node = values->initial_node;

	//Grab a cursor for walking the sub-tree
	generic_ast_node_t* ast_cursor = for_stmt_node->first_child;

	//We will always see 3 nodes here to start out with, of the type for_loop_cond_ast_node_t. These
	//nodes contain an "is_blank" field that will alert us if this is just a placeholder. The first 2 parts of a for

	//If the very first one is not blank
	if(ast_cursor->first_child != NULL){
		//Add it's child in as a statement to the entry block
		emit_expr_code(for_stmt_entry_block, ast_cursor->first_child);
	}

	//We'll now need to create our repeating node. This is the node that will actually repeat from the for loop.
	//The second and third condition in the for loop are the ones that execute continously. The third condition
	//always executes at the end of each iteration
	basic_block_t* condition_block = basic_block_alloc();

	//The condition block is always a successor to the entry block
	add_successor(for_stmt_entry_block, condition_block);

	//Move along to the next node
	ast_cursor = ast_cursor->next_sibling;

	//The condition block values package
	expr_ret_package_t condition_block_vals;
	//By default, make this blank
	condition_block_vals.operator = BLANK;
	
	//If the second one is not blank
	if(ast_cursor->first_child != NULL){
		//This is always the first part of the repeating block
		condition_block_vals = emit_expr_code(condition_block, ast_cursor->first_child);

	//It is impossible for the second one to be blank
	} else {
		print_parse_message(PARSE_ERROR, "Fatal internal compiler error. Should not have gotten here if blank", for_stmt_node->line_number);
		exit(0);
	}

	//Now move it along to the third condition
	ast_cursor = ast_cursor->next_sibling;

	//Create the update block
	basic_block_t* for_stmt_update_block = basic_block_alloc();

	//If the third one is not blank
	if(ast_cursor->first_child != NULL){
		//Emit the update expression
		emit_expr_code(for_stmt_update_block, ast_cursor->first_child);
	}

	//This node will always jump right back to the start
	add_successor(for_stmt_update_block, condition_block);
	//Unconditional jump to condition block
	emit_jmp_stmt(for_stmt_update_block, condition_block, JUMP_TYPE_JMP);


	//Advance to the next sibling
	ast_cursor = ast_cursor->next_sibling;
	
	//If this is not a compound statement, we have a serious error
	if(ast_cursor->CLASS != AST_NODE_CLASS_COMPOUND_STMT){
		print_parse_message(PARSE_ERROR, "Fatal internal compiler error. Expected compound statement in for loop, but did not find one.", for_stmt_node->line_number);
		//Immediate failure here
		exit(0);
	}
	
	//Create a copy of our values here
	values_package_t compound_stmt_values;
	compound_stmt_values.initial_node = ast_cursor;
	//The loop starts at the condition block
	compound_stmt_values.loop_stmt_start = condition_block;
	//Store the end block
	compound_stmt_values.function_end_block = values->function_end_block;
	//Store the update block here -- may or may not be NULL
	compound_stmt_values.for_loop_update_block = for_stmt_update_block;
	//Store this as well
	compound_stmt_values.loop_stmt_end = for_stmt_exit_block;

	//Otherwise, we will allow the subsidiary to handle that. The loop statement here is the condition block,
	//because that is what repeats on continue
	basic_block_t* compound_stmt_start = visit_compound_statement(&compound_stmt_values);

	//For our eventual token
	expr_ret_package_t expression_package;

	//If it's null, that's actually ok here
	if(compound_stmt_start == NULL){
		//We'll make sure that the start points to this block
		add_successor(condition_block, for_stmt_update_block);

		//And we're done
		return for_stmt_entry_block;
	}

	//This will always be a successor to the conditional statement
	add_successor(condition_block, compound_stmt_start);
	//The condition block also has another direct successor, the exit block
	add_successor(condition_block, for_stmt_exit_block);
	//Ensure it is the direct successor
	condition_block->direct_successor = for_stmt_exit_block;

	//We'll use our inverse jumping("jump out") strategy here
	jump_type_t jump_type = select_appropriate_jump_stmt(condition_block_vals.operator, JUMP_CATEGORY_INVERSE);

	//Make the condition block jump to the compound stmt start
	emit_jmp_stmt(condition_block, for_stmt_exit_block, jump_type);

	//However if it isn't NULL, we'll need to find the end of this compound statement
	basic_block_t* compound_stmt_end = compound_stmt_start;

	//So long as we don't see the end or a return
	while(compound_stmt_end->direct_successor != NULL && compound_stmt_end->is_return_stmt == 0
		  && compound_stmt_end->is_cont_stmt == 0 && compound_stmt_end->is_break_stmt == 0){
		compound_stmt_end = compound_stmt_end->direct_successor;
	}

	//Once we get here, if it is a return statement, that means that we always return
	if(compound_stmt_end->is_return_stmt == 1){
		//We should warn here
		print_cfg_message(WARNING, "For loop internal returns through every control block, will only execute once", for_stmt_node->line_number);
		(*num_warnings_ref)++;
		//There's nothing to add here, we just return
		return for_stmt_entry_block;
	}

	//The successor to the end block is the update block
	add_successor(compound_stmt_end, for_stmt_update_block);
	//We also need an uncoditional jump right to the update block
	emit_jmp_stmt(compound_stmt_end, for_stmt_update_block, JUMP_TYPE_JMP);

	//Give back the entry block
	return for_stmt_entry_block;
}


/**
 * A do-while statement is a simple control flow construct. As always, the direct successor path is the path that reliably
 * leads us down and out
 */
static basic_block_t* visit_do_while_statement(values_package_t* values){
	//Create our entry block. This in reality will be the compound statement
	basic_block_t* do_while_stmt_entry_block = basic_block_alloc();
	//The true ending block
	basic_block_t* do_while_stmt_exit_block = basic_block_alloc();

	//Grab the initial node
	generic_ast_node_t* do_while_stmt_node = values->initial_node;

	//Grab a cursor for walking the subtree
	generic_ast_node_t* ast_cursor = do_while_stmt_node->first_child;

	//If this is not a compound statement, something here is very wrong
	if(ast_cursor->CLASS != AST_NODE_CLASS_COMPOUND_STMT){
		print_cfg_message(PARSE_ERROR, "Expected compound statement in do-while, but did not find one", do_while_stmt_node->line_number);
		exit(0);
	}

	//Create and populate all needed values
	values_package_t compound_stmt_values;
	compound_stmt_values.initial_node = ast_cursor;
	compound_stmt_values.function_end_block = values->function_end_block;
	compound_stmt_values.loop_stmt_start = do_while_stmt_entry_block;
	compound_stmt_values.loop_stmt_end = do_while_stmt_exit_block;
	compound_stmt_values.for_loop_update_block = NULL;

	//We go right into the compound statement here
	basic_block_t* do_while_compound_stmt_entry = visit_compound_statement(&compound_stmt_values);

	//If this is NULL, it means that we really don't have a compound statement there
	if(do_while_compound_stmt_entry == NULL){
		print_parse_message(PARSE_ERROR, "Do-while statement has empty clause, statement has no effect", do_while_stmt_node->line_number);
		(*num_warnings_ref)++;
	}

	//No matter what, this will get merged into the top statement
	do_while_stmt_entry_block = merge_blocks(do_while_stmt_entry_block, do_while_compound_stmt_entry);

	//We will drill to the bottom of the compound statement
	basic_block_t* compound_stmt_end = do_while_stmt_entry_block;

	//So long as we don't see NULL or return
	while(compound_stmt_end->direct_successor != NULL && compound_stmt_end->is_return_stmt == 0
		  && compound_stmt_end->is_cont_stmt == 0 && compound_stmt_end->is_break_stmt == 0){
		compound_stmt_end = compound_stmt_end->direct_successor;
	}

	//Once we get here, if it's a return statement, everything below is unreachable
	if(compound_stmt_end->is_return_stmt == 1){
		print_cfg_message(WARNING, "Do-while returns through all internal control paths. All following code is unreachable", do_while_stmt_node->line_number);
		(*num_warnings_ref)++;
		//Just return the block here
		return do_while_stmt_entry_block;
	}

	//Add this in to the ending block
	expr_ret_package_t package = emit_expr_code(compound_stmt_end, ast_cursor->next_sibling);

	//Now we'll make do our necessary connnections. The direct successor of this end block is the true
	//exit block
	add_successor(compound_stmt_end, do_while_stmt_exit_block);
	//Make sure it's the direct successor
	compound_stmt_end->direct_successor = do_while_stmt_exit_block;

	//It's other successor though is the loop entry
	add_successor(compound_stmt_end, do_while_stmt_entry_block);
	//Discern the jump type here--This is a direct jump
	jump_type_t jump_type = select_appropriate_jump_stmt(package.operator, JUMP_CATEGORY_NORMAL);
		
	//We'll need a jump statement here to the entrance block
	emit_jmp_stmt(compound_stmt_end, do_while_stmt_entry_block, jump_type);

	//Always return the entry block
	return do_while_stmt_entry_block;
}


/**
 * A while statement is a very simple control flow construct. As always, the "direct successor" path is the path
 * that reliably leads us down and out
 */
static basic_block_t* visit_while_statement(values_package_t* values){
	//Create our entry block
	basic_block_t* while_statement_entry_block = basic_block_alloc();
	//And create our exit block
	basic_block_t* while_statement_end_block = basic_block_alloc();
	//Grab this for convenience
	generic_ast_node_t* while_stmt_node = values->initial_node;

	//Grab a cursor to the while statement node
	generic_ast_node_t* ast_cursor = while_stmt_node->first_child;

	//The entry block contains our expression statement
	expr_ret_package_t package = emit_expr_code(while_statement_entry_block, ast_cursor);

	//The very next node is a compound statement
	ast_cursor = ast_cursor->next_sibling;

	//If it isn't, we'll error out. This is really only for dev use
	if(ast_cursor->CLASS != AST_NODE_CLASS_COMPOUND_STMT){
		print_cfg_message(PARSE_ERROR, "Found node that is not a compound statement in while-loop subtree", while_stmt_node->line_number);
		exit(0);
	}

	//Create a values package to send in
	values_package_t compound_stmt_values;
	compound_stmt_values.initial_node = ast_cursor;
	compound_stmt_values.function_end_block = values->function_end_block;
	compound_stmt_values.loop_stmt_start = while_statement_entry_block;
	compound_stmt_values.for_loop_update_block = NULL;
	compound_stmt_values.loop_stmt_end = while_statement_end_block;

	//Now that we know it's a compound statement, we'll let the subsidiary handle it
	basic_block_t* compound_stmt_start = visit_compound_statement(&compound_stmt_values);

	//If it's null, that means that we were given an empty while loop here
	if(compound_stmt_start == NULL){
		//For the user to see
		print_cfg_message(WARNING, "While loop has empty body, has no effect", while_stmt_node->line_number);
		(*num_warnings_ref)++;
		//We'll just return now
		return while_statement_entry_block;
	}

	//We'll now determine what kind of jump statement that we have here. We want to jump to the exit if
	//we're bad, so we'll do an inverse jump
	jump_type_t jump_type = select_appropriate_jump_stmt(package.operator, JUMP_CATEGORY_INVERSE);
	//"Jump over" the body if it's bad
	emit_jmp_stmt(while_statement_entry_block, while_statement_end_block, jump_type);

	//Otherwise it isn't null, so we can add it as a successor
	add_successor(while_statement_entry_block, compound_stmt_start);
	//The direct successor to the entry block is the end block
	add_successor(while_statement_entry_block, while_statement_end_block);
	//Just to be sure
	while_statement_entry_block->direct_successor = while_statement_end_block;

	//Let's now find the end of the compound statement
	basic_block_t* compound_stmt_end = compound_stmt_start;

	//So long as it isn't null or return
	while (compound_stmt_end->direct_successor != NULL && compound_stmt_end->is_return_stmt == 0
		   && compound_stmt_end->is_cont_stmt == 0 && compound_stmt_end->is_break_stmt == 0) {
		compound_stmt_end = compound_stmt_end->direct_successor;
	}
	
	//If we make it to the end and this ending statement is a return, that means that we always return
	//Throw a warning
	if(compound_stmt_end->is_return_stmt == 1){
		//It is only an error though -- the user is allowed to do this
		print_cfg_message(WARNING, "While loop body returns in all control paths. It will only execute at most once", while_stmt_node->line_number);
		(*num_warnings_ref)++;
	}

	//No matter what, the successor to this statement is the top of the loop
	add_successor(compound_stmt_end, while_statement_entry_block);

	//The compound statement end will jump right back up to the entry block
	emit_jmp_stmt(compound_stmt_end, while_statement_entry_block, JUMP_TYPE_JMP);

	//Now we're done, so
	return while_statement_entry_block;
}


/**
 * Process the if-statement subtree into the equivalent CFG form
 *
 * We make use of the "direct successor" nodes as a direct path through the if statements. We ensure that 
 * these direct paths always exist in such if-statements. 
 *
 * The sub-structure that this tree creates has only two options:
 * 	1.) Every node flows through a return, in which case nobody hits the exit block
 * 	2.) The main path flows through the end block, out of the structure
 *
 * 
 * =========== JUMP INSTRUCTION SELECTION =========================
 *
 * This implementation will make use of a "jump-to-else" scheme. This means
 * that the "if" part of the statement is always directly underneath the entry block. These else,
 * if such a statement exists, needs to be jumped to to get to it
 */
static basic_block_t* visit_if_statement(values_package_t* values){
	//We always have an entry block here -- the end block is made for us
	basic_block_t* entry_block = basic_block_alloc();
	
	//Mark if we have a return statement
	u_int8_t returns_through_main_path = 0;
	//Mark if we have a continue statement
	u_int8_t continues_through_main_path = 0;
	//Mark if we return through an else path
	u_int8_t returns_through_second_path = 0;
	//Mark if we have a continue statement
	u_int8_t continues_through_second_path = 0;
	//Mark if we have a break statement
	u_int8_t breaks_through_main_path = 0;
	//Mark if we break through else
	u_int8_t breaks_through_second_path = 0;

	//Let's grab a cursor to walk the tree
	generic_ast_node_t* cursor = values->initial_node->first_child;

	//Add it into the start block
	expr_ret_package_t package = emit_expr_code(entry_block, cursor);

	//No we'll move one step beyond, the next node must be a compound statement
	cursor = cursor->next_sibling;

	//If it isn't, that's an issue
	if(cursor->CLASS != AST_NODE_CLASS_COMPOUND_STMT){
		print_cfg_message(PARSE_ERROR, "Expected compound statement in if node", cursor->line_number);
		exit(1);
	}

	//Create and send in a values package
	values_package_t if_compound_stmt_values;
	if_compound_stmt_values.initial_node = cursor;
	if_compound_stmt_values.function_end_block = values->function_end_block;
	if_compound_stmt_values.loop_stmt_start = values->loop_stmt_start;
	if_compound_stmt_values.if_stmt_end_block = values->if_stmt_end_block;
	if_compound_stmt_values.for_loop_update_block = values->for_loop_update_block;
	if_compound_stmt_values.loop_stmt_end = values->loop_stmt_end;

	//Now that we know it is, we'll invoke the compound statement rule
	basic_block_t* if_compound_stmt_entry = visit_compound_statement(&if_compound_stmt_values);

	//If this is null, whole thing fails
	if(if_compound_stmt_entry == NULL){
		print_cfg_message(WARNING, "Empty if clause in if-statement", cursor->line_number);
		(*num_warnings_ref)++;
	} else {
		//Add the if statement node in as a direct successor
		add_successor(entry_block, if_compound_stmt_entry);
		//Add as direct successor
		entry_block->direct_successor = if_compound_stmt_entry;

		//Now we'll find the end of this statement
		basic_block_t* if_compound_stmt_end = if_compound_stmt_entry;

		//Once we've visited, we'll need to drill to the end of this compound statement
		while(if_compound_stmt_end->direct_successor != NULL && if_compound_stmt_end->is_return_stmt == 0
			 && if_compound_stmt_end->is_cont_stmt == 0 && if_compound_stmt_end->is_break_stmt == 0){
			if_compound_stmt_end = if_compound_stmt_end->direct_successor;
		}

		//Once we get here, we either have an end block or a return statement. Which one we have will influence decisions
		returns_through_main_path = if_compound_stmt_end->is_return_stmt;
		//Mark this too
		continues_through_main_path = if_compound_stmt_end->is_cont_stmt;
		//And this one
		breaks_through_main_path = if_compound_stmt_end->is_break_stmt;

		//If it doesn't return through the main path, the successor is the end node
		if(returns_through_main_path == 0){
			add_successor(if_compound_stmt_end, values->if_stmt_end_block);
			//Ensure is direct successor
			if_compound_stmt_end->direct_successor = values->if_stmt_end_block;
		}

		//The successor to the if-stmt end path is the if statement end block
		emit_jmp_stmt(if_compound_stmt_end, values->if_stmt_end_block, JUMP_TYPE_JMP);
	}

	//This is the end if we have a lone "if"
	if(cursor->next_sibling == NULL){
		//If this is the case, the end block is a direct successor
		add_successor(entry_block, values->if_stmt_end_block);
		//Ensure is direct successor
		entry_block->direct_successor = values->if_stmt_end_block;

		/**
		 * The "if" path is always our direct path, we only need to jump to the else, so we jump
		 * in an inverse fashion
		 */
		//Let's determine the appropriate jump statement
		jump_type_t jump_type = select_appropriate_jump_stmt(package.operator, JUMP_CATEGORY_INVERSE);
		emit_jmp_stmt(entry_block, values->if_stmt_end_block, jump_type);

		//We can leave now
		return entry_block;
	}

	//If we make it here, we know that we have either an if or an if-else block
	//Advance the cursor
	cursor = cursor->next_sibling;
	
	//If we have a compound statement, we'll handle it as an "else" clause
	if(cursor->CLASS == AST_NODE_CLASS_COMPOUND_STMT){
		//Create the values package 
		values_package_t else_values_package;
		else_values_package.initial_node = cursor;
		else_values_package.function_end_block = values->function_end_block;
		else_values_package.loop_stmt_start = values->loop_stmt_start;
		else_values_package.if_stmt_end_block = values->if_stmt_end_block;
		else_values_package.for_loop_update_block = values->for_loop_update_block;
		else_values_package.loop_stmt_end = values->loop_stmt_end;

		//Visit the else statement
		basic_block_t* else_compound_stmt_entry = visit_compound_statement(&else_values_package);

		//If this is NULL, we'll send a warning and hop out -- no need for more processing here
		if(else_compound_stmt_entry == NULL){
			print_cfg_message(WARNING, "Empty else clause in if-else statement", cursor->line_number);
			(*num_warnings_ref)++;

			//The entry block's direct successor is the end statement
			entry_block->direct_successor = values->if_stmt_end_block;

			//Just get out if this happens
			return entry_block;
		}

		//Otherwise, we'll add this in as a successor
		add_successor(entry_block, else_compound_stmt_entry);
		//Let's determine the appropriate jump statement
		jump_type_t jump_type = select_appropriate_jump_stmt(package.operator, JUMP_CATEGORY_INVERSE);
		//Add in our "jump to else" clause
		emit_jmp_stmt(entry_block, else_compound_stmt_entry, jump_type);

		//Add in a jump statement to get to here

		//Now we'll find the end of this statement
		basic_block_t* else_compound_stmt_end = else_compound_stmt_entry;

		//Once we've visited, we'll need to drill to the end of this compound statement
		while(else_compound_stmt_end->direct_successor != NULL && else_compound_stmt_end->is_return_stmt == 0
			  && else_compound_stmt_end->is_cont_stmt == 0){
			else_compound_stmt_end = else_compound_stmt_end->direct_successor;
		}

		//Once we get here, we either have an end block or a return statement. Which one we have will influence decisions
		returns_through_second_path = else_compound_stmt_end->is_return_stmt;
		//Mark this too
		continues_through_second_path = else_compound_stmt_end->is_cont_stmt;
		//Mark this here as well
		breaks_through_second_path = else_compound_stmt_end->is_break_stmt;

		//If it isn't a return statement, then it's successor is the entry block
		if(returns_through_second_path == 0){
			add_successor(else_compound_stmt_end, values->if_stmt_end_block);
			//Ensure is direct successor
			else_compound_stmt_end->direct_successor = values->if_stmt_end_block;
			//Add in our jump to end here
			emit_jmp_stmt(else_compound_stmt_end, values->if_stmt_end_block, JUMP_TYPE_JMP);
		}

		/**
		 * Rules for a direct successor
		 * 	1.) If both statements are return statements, the entire thing is a return statement
		 * 	2.) If one or the other does not return, we flow through the one that does NOT return
		 * 	3.) If both don't return, we default to the "if" clause
		 */
		if(returns_through_main_path == 0 && continues_through_main_path == 0 && breaks_through_second_path == 0){
			//The direct successor is the main path
			entry_block->direct_successor = if_compound_stmt_entry;
		//We favor this one if not
		} else if(returns_through_second_path == 0 && continues_through_second_path == 0 && breaks_through_second_path == 0){
			entry_block->direct_successor = else_compound_stmt_entry;
		} else if(returns_through_main_path == 1 && returns_through_second_path == 0){
			//The direct successor is the else path
			entry_block->direct_successor = else_compound_stmt_entry;
		} else if(continues_through_main_path == 1 && breaks_through_second_path == 1){
			//The direct successor is the else path
			entry_block->direct_successor = else_compound_stmt_entry;
		} else if(breaks_through_main_path == 1 && continues_through_second_path == 1){
			//The direct successor is the main path 
			entry_block->direct_successor = if_compound_stmt_entry;
		} else {
			//If there's anything else, we default to the first path
			entry_block->direct_successor = if_compound_stmt_entry;
		}

		//We're done here, send it back
		return entry_block;
	
	//Otherwise we have an "else if" clause 
	} else if(cursor->CLASS == AST_NODE_CLASS_IF_STMT){
		//Create the hours package
		values_package_t else_if_values_package;
		else_if_values_package.initial_node = cursor;
		else_if_values_package.if_stmt_end_block = values->if_stmt_end_block;
		else_if_values_package.loop_stmt_start = values->loop_stmt_start;
		else_if_values_package.for_loop_update_block = values->for_loop_update_block;
		else_if_values_package.function_end_block = values->function_end_block;
		else_if_values_package.loop_stmt_end = values->loop_stmt_end;

		//Visit the if statment, this one is not a parent
		basic_block_t* else_if_entry = visit_if_statement(&else_if_values_package);

		//Add this as a successor to the entrant
		add_successor(entry_block, else_if_entry);
		//Let's determine the appropriate jump statement
		jump_type_t jump_type = select_appropriate_jump_stmt(package.operator, JUMP_CATEGORY_INVERSE);
		//Emit our jump to else if here
		emit_jmp_stmt(entry_block, else_if_entry, jump_type);
	
		//Once we visit this, we'll navigate to the end
		basic_block_t* else_if_end = else_if_entry;

		//We'll drill down to the end -- so long as we don't hit the end block and we don't hit a return statement
		while(else_if_end->direct_successor != NULL && else_if_end->is_return_stmt == 0
			 && else_if_end->is_cont_stmt == 0){
			//Keep track of the immediate predecessor
			else_if_end = else_if_end->direct_successor;
		}

		//Once we get here, we either have an end block or a return statement
		returns_through_second_path = else_if_end->is_return_stmt;
		//Mark this too
		continues_through_second_path = else_if_end->is_cont_stmt;
		//And mark this
		breaks_through_second_path = else_if_end->is_break_stmt;

		//If it doesnt return through the second path, then the end better be the original end
		if(returns_through_second_path == 0 && else_if_end != values->if_stmt_end_block){
			printf("DOES NOT TRACK END BLOCK\n");
		}

		/**
		 * Rules for a direct successor
		 * 	1.) If both statements are return statements, the entire thing is a return statement
		 * 	2.) If one or the other does not return, we flow through the one that does NOT return
		 * 	3.) If both don't return, we default to the "if" clause
		 */
		if(returns_through_main_path == 0 && continues_through_main_path == 0 && breaks_through_main_path == 0){
			//The direct successor is the main path
			entry_block->direct_successor = if_compound_stmt_entry;
		} else if(continues_through_second_path == 0 && returns_through_second_path == 0 && breaks_through_second_path == 0){
			entry_block->direct_successor = else_if_entry;
		} else if(returns_through_main_path == 1 && returns_through_second_path == 0){
			//The direct successor is the else path
			entry_block->direct_successor = else_if_entry;
		} else if(continues_through_main_path == 1 && breaks_through_second_path == 1){
			//The direct successor is the else path
			entry_block->direct_successor = else_if_entry;
		} else if(breaks_through_main_path == 1 && continues_through_second_path == 1){
			//The direct successor is the main path 
			entry_block->direct_successor = if_compound_stmt_entry;
		} else {
			//If there's anything else, we default to the first path
			entry_block->direct_successor = if_compound_stmt_entry;
		}

		return entry_block;
	
	//Some weird error here
	} else {
		print_cfg_message(PARSE_ERROR, "Improper node found after if-statement", cursor->line_number);
		(*num_errors_ref)++;
		exit(0);
	}
}


/**
 * Visit a case statement. These statements are handled like individual blocks
 */
static basic_block_t* visit_case_statement(values_package_t* values){
	//For a case statement, we just go through the statement and process
	//as usual. However first, we need to assign it it's own block ID. These
	//block IDs will be important, as our jump table uses them to implement the
	//switch statement functionality
	
	//Create it
	basic_block_t* case_stmt_block = basic_block_alloc();


	//We'll always give this one back
	return case_stmt_block;
}


/**
 * Visit a default statement.  These statements are also handled like individual blocks that can 
 * be jumped to
 */
static basic_block_t* visit_default_statement(values_package_t* values){
	//For a default statement, it performs very similarly to a case statement. 
	//It will be handled slightly differently in the jump table, but we'll get to that 
	//later on
	
	//Create it
	basic_block_t* default_stmt_block = basic_block_alloc();


	//Give it back
	return default_stmt_block;
}


/**
 * Visit a switch statement
 */
static basic_block_t* visit_switch_statement(values_package_t* values){

}


/**
 * A compound statement also acts as a sort of multiplexing block. It runs through all of it's statements, calling
 * the appropriate functions and making the appropriate additions
 *
 * We make use of the "direct successor" nodes as a direct path through the compound statement, if such a path exists
 */
static basic_block_t* visit_compound_statement(values_package_t* values){
	//The global starting block
	basic_block_t* starting_block = NULL;
	//The current block
	basic_block_t* current_block = starting_block;

	//Grab the initial node
	generic_ast_node_t* compound_stmt_node = values->initial_node;

	//Grab our very first thing here
	generic_ast_node_t* ast_cursor = compound_stmt_node->first_child;
	
	//Roll through the entire subtree
	while(ast_cursor != NULL){
		//We've found a declaration statement
		if(ast_cursor->CLASS == AST_NODE_CLASS_DECL_STMT){
			values_package_t values;
			values.initial_node = ast_cursor;

			//We'll visit the block here
			basic_block_t* decl_block = visit_declaration_statement(&values);

			//If the start block is null, then this is the start block. Otherwise, we merge it in
			if(starting_block == NULL){
				starting_block = decl_block;
				current_block = decl_block;
			//Just merge with current
			} else {
				current_block = merge_blocks(current_block, decl_block); 
			}

		//We've found a let statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_LET_STMT){
			values_package_t values;
			values.initial_node = ast_cursor;

			//We'll visit the block here
			basic_block_t* let_block = visit_let_statement(&values);

			//If the start block is null, then this is the start block. Otherwise, we merge it in
			if(starting_block == NULL){
				starting_block = let_block;
				current_block = let_block;
			//Just merge with current
			} else {
				current_block = merge_blocks(current_block, let_block); 
			}

		//If we have a return statement -- SPECIAL CASE HERE
		} else if (ast_cursor->CLASS == AST_NODE_CLASS_RET_STMT){
			//If for whatever reason the block is null, we'll create it
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
			}

			//Emit the return statement, let the sub rule handle
			emit_ret_stmt(current_block, ast_cursor);

			//The current block will now be marked as a return statement
			current_block->is_return_stmt = 1;

			//The current block's direct and only successor is the function exit block
			add_successor(current_block, values->function_end_block);

			//If there is anything after this statement, it is UNREACHABLE
			if(ast_cursor->next_sibling != NULL){
				print_cfg_message(WARNING, "Unreachable code detected after return statement", ast_cursor->next_sibling->line_number);
				(*num_warnings_ref)++;
			}

			//We're completely done here
			return starting_block;

		//We've found an if-statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_IF_STMT){
			//Create the end block here for pointer reasons
			basic_block_t* if_end_block = basic_block_alloc();

			//Create the values package
			values_package_t if_stmt_values;
			if_stmt_values.initial_node = ast_cursor;
			if_stmt_values.function_end_block = values->function_end_block;
			if_stmt_values.for_loop_update_block = values->for_loop_update_block;
			if_stmt_values.if_stmt_end_block = if_end_block;
			if_stmt_values.loop_stmt_start = values->loop_stmt_start;
			if_stmt_values.loop_stmt_end = values->loop_stmt_end;

			//We'll now enter the if statement
			basic_block_t* if_stmt_start = visit_if_statement(&if_stmt_values);
			
			//Once we have the if statement start, we'll add it in as a successor
			if(starting_block == NULL){
				starting_block = if_stmt_start;
				current_block = if_stmt_start;
			} else {
				//Add this in as the current block
				current_block = merge_blocks(current_block, if_stmt_start);
			}

			//Now we'll find the end of the if statement block
			//So long as we haven't hit the end and it isn't a return statement
			while (current_block->direct_successor != NULL && current_block->is_return_stmt == 0
				  && current_block->is_cont_stmt == 0){
				current_block = current_block->direct_successor;
			}
			
			/*
			 * DEVELOPER USE MESSAGE
			 */
			if(current_block->is_return_stmt == 0 && current_block->is_cont_stmt == 0 && current_block != if_end_block){
				printf("END BLOCK REFERENCE LOST");
			}

			//If it is a return statement, that means that this if statement returns through every path. We'll leave 
			//if this is the case
			if(current_block->is_return_stmt == 1){
				//Throw a warning if this happens
				if(ast_cursor->next_sibling != NULL){
					print_cfg_message(WARNING, "Unreachable code detected after if-else block that returns through every control path", ast_cursor->line_number);
					(*num_warnings_ref)++;
				}
				//Give it back
				return starting_block;
			}

			//If it's a continue statement, that means that this if statement continues through every path. We'll leave if this
			//is the case
			if(current_block->is_cont_stmt == 1){
				//Throw a warning if this happens
				if(ast_cursor->next_sibling != NULL){
					print_cfg_message(WARNING, "Unreachable code detected after if-else block that continues through every control path", ast_cursor->line_number);
					(*num_warnings_ref)++;
				}

				//Give it back
				return starting_block;
			}
		
		//Handle a while statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_WHILE_STMT){
			//Create the values here
			values_package_t while_stmt_values;
			while_stmt_values.initial_node = ast_cursor;
			while_stmt_values.for_loop_update_block = values->for_loop_update_block;
			while_stmt_values.loop_stmt_start = NULL;
			while_stmt_values.loop_stmt_end = NULL;
			while_stmt_values.if_stmt_end_block = values->if_stmt_end_block;
			while_stmt_values.function_end_block = values->function_end_block;

			//Visit the while statement
			basic_block_t* while_stmt_entry_block = visit_while_statement(&while_stmt_values);

			//We'll now add it in
			if(starting_block == NULL){
				starting_block = while_stmt_entry_block;
				current_block = starting_block;
			//We never merge while statements -- it will always be a successor
			} else {
				//Add as a successor
				add_successor(current_block, while_stmt_entry_block);
			}

			//Now we'll drill to the end here. This is easier than before, because the direct successor to
			//the entry block of a while statement is always the end block
			current_block = while_stmt_entry_block->direct_successor;
	
		//Handle a do-while statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_DO_WHILE_STMT){
			//Create the values package
			values_package_t do_while_values;
			do_while_values.initial_node = ast_cursor;
			do_while_values.function_end_block = values->function_end_block;
			do_while_values.if_stmt_end_block = values->if_stmt_end_block;
			do_while_values.loop_stmt_start = NULL;
			do_while_values.loop_stmt_end = NULL;
			do_while_values.for_loop_update_block = values->for_loop_update_block;

			//Visit the statement
			basic_block_t* do_while_stmt_entry_block = visit_do_while_statement(&do_while_values);

			//We'll now add it in
			if(starting_block == NULL){
				starting_block = do_while_stmt_entry_block;
				current_block = starting_block;
			//We never merge do-while's, they are strictly successors
			} else {
				add_successor(current_block, do_while_stmt_entry_block);
			}

			//Now we'll need to reach the end-point of this statement
			current_block = do_while_stmt_entry_block;

			//So long as we have successors and don't see returns
			while(current_block->direct_successor != NULL && current_block->is_return_stmt == 0
				  && current_block->is_cont_stmt == 0){
				current_block = current_block->direct_successor;
			}

			//If we make it here and we had a return statement, we need to get out
			if(current_block->is_return_stmt == 1){
				//Everything beyond this point is unreachable, no point in going on
				print_cfg_message(WARNING, "Unreachable code detected after block that returns in all control paths", ast_cursor->next_sibling->line_number);
				(*num_warnings_ref)++;
				//Get out now
				return starting_block;
			}

			//Otherwise, we're all set to go to the next iteration

		//Handle a for statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_FOR_STMT){
			//Create the values package
			values_package_t for_stmt_values;
			for_stmt_values.initial_node = ast_cursor;
			for_stmt_values.function_end_block = values->function_end_block;
			for_stmt_values.for_loop_update_block = values->for_loop_update_block;
			for_stmt_values.loop_stmt_start = NULL;
			for_stmt_values.loop_stmt_end = NULL;
			for_stmt_values.if_stmt_end_block = values->if_stmt_end_block;

			//First visit the statement
			basic_block_t* for_stmt_entry_block = visit_for_statement(&for_stmt_values);

			//Now we'll add it in
			if(starting_block == NULL){
				starting_block = for_stmt_entry_block;
				current_block = starting_block;
			//We ALWAYS merge for statements into the current block
			} else {
				current_block = merge_blocks(current_block, for_stmt_entry_block);
			}
			
			//Once we're here the start is in current
			while(current_block->direct_successor != NULL && current_block->is_return_stmt == 0 && current_block->is_cont_stmt == 0){
				current_block = current_block->direct_successor;
			}

			//This should never happen, so if it does we have a problem
			if(current_block->is_return_stmt == 1){
				print_parse_message(PARSE_ERROR, "It should be impossible to have a for statement that returns in all control paths", ast_cursor->line_number);
				exit(0);
			}

			//But if we don't then this is the current node

		//Handle a continue statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_CONTINUE_STMT){
			//Let's first see if we're in a loop or not
			if(values->loop_stmt_start == NULL){
				print_cfg_message(PARSE_ERROR, "Continue statement was not found in a loop", ast_cursor->line_number);
				(*num_errors_ref)++;
				return create_and_return_err();
			}

			//This could happen where we have nothing here
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
			}

			//There are two options here. We could see a regular continue or a conditional
			//continue. If the child is null, then it is a regular continue
			if(ast_cursor->first_child == NULL){
				//Mark this for later
				current_block->is_cont_stmt = 1;

				//Let's see what kind of loop we're in
				//NON for loop
				if(values->for_loop_update_block == NULL){
					//Otherwise we are in a loop, so this means that we need to point the continue statement to
					//the loop entry block
					add_successor(current_block, values->loop_stmt_start);
					//We always jump to the start of the loop statement unconditionally
					emit_jmp_stmt(current_block, values->loop_stmt_start, JUMP_TYPE_JMP);

				//We are in a for loop
				} else {
					//Otherwise we are in a for loop, so we just need to point to the for loop update block
					add_successor(current_block, values->for_loop_update_block);
					//Emit a direct unconditional jump statement to it
					emit_jmp_stmt(current_block, values->for_loop_update_block, JUMP_TYPE_JMP);
				}

				//Further, anything after this is unreachable
				if(ast_cursor->next_sibling != NULL){
					print_cfg_message(WARNING, "Unreachable code detected after continue statement", ast_cursor->next_sibling->line_number);
					(*num_warnings_ref)++;
				}

				//We're done here, so return the starting block. There is no 
				//point in going on
				return starting_block;

			//Otherwise, we have a conditional continue here
			} else {
				//Emit the expression code into the current statement
				expr_ret_package_t package = emit_expr_code(current_block, ast_cursor->first_child);
				//Decide the appropriate jump statement -- direct path here
				jump_type_t jump_type = select_appropriate_jump_stmt(package.operator, JUMP_CATEGORY_NORMAL);

				//Two divergent paths here -- whether or not we have a for loop
				//Not a for loop
				if(values->for_loop_update_block == NULL){
					//Otherwise we are in a loop, so this means that we need to point the continue statement to
					//the loop entry block
					basic_block_t* successor = current_block->direct_successor;
					//Add the successor in
					add_successor(current_block, values->loop_stmt_start);
					//Restore the direct successor
					current_block->direct_successor = successor;
					//We always jump to the start of the loop statement unconditionally
					emit_jmp_stmt(current_block, values->loop_stmt_start, jump_type);

				//We are in a for loop
				} else {
					//Otherwise we are in a for loop, so we just need to point to the for loop update block
					basic_block_t* successor = current_block->direct_successor;
					add_successor(current_block, values->for_loop_update_block);
					//Restore the direct successor
					current_block->direct_successor = successor;
					//Emit a direct unconditional jump statement to it
					emit_jmp_stmt(current_block, values->for_loop_update_block, jump_type);
				}
			}

		//Hand le a break out statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_BREAK_STMT){
			//Let's first see if we're in a loop or not
			if(values->loop_stmt_start == NULL){
				print_cfg_message(PARSE_ERROR, "Break statement was not found in a loop", ast_cursor->line_number);
				(*num_errors_ref)++;
				return create_and_return_err();
			}

			//This could happen where we have nothing here
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
			}

			//There are two options here: We could have a conditional break
			//or a normal break. If there is no child node, we have a normal break
			if(ast_cursor->first_child == NULL){
				//Mark this for later
				current_block->is_break_stmt = 1;
				//Otherwise we need to break out of the loop
				add_successor(current_block, values->loop_stmt_end);
				//We will jump to it -- this is always an uncoditional jump
				emit_jmp_stmt(current_block, values->loop_stmt_end, JUMP_TYPE_JMP);

				//If we see anything after this, it is unreachable so throw a warning
				if(ast_cursor->next_sibling != NULL){
					print_cfg_message(WARNING, "Unreachable code detected after break statement", ast_cursor->next_sibling->line_number);
					(*num_errors_ref)++;
				}

				//For a regular break statement, this is it, so we just get out
				//Give back the starting block
				return starting_block;

			//Otherwise, we have a conditional break, which will generate a conditional jump instruction
			} else {
				//This block can jump right out of the loop
				basic_block_t* successor = current_block->direct_successor;
				add_successor(current_block, values->loop_stmt_end);
				//Restore
				current_block->direct_successor = successor;
				
				//However, this block is not an ending break statement, so we will not mark it

				//First let's emit the conditional code
				expr_ret_package_t ret_package = emit_expr_code(current_block, ast_cursor->first_child);

				//Now based on whatever we have in here, we'll emit the appropriate jump type(direct jump)
				jump_type_t jump_type = select_appropriate_jump_stmt(ret_package.operator, JUMP_CATEGORY_NORMAL);

				//Emit our conditional jump now
				emit_jmp_stmt(current_block, values->loop_stmt_end, jump_type);
			}

		//Handle a defer statement. Remember that a defer statment is one monolithic
		//node with a bunch of sub-nodes underneath that are all handleable by "expr"
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_DEFER_STMT){
			//This really shouldn't happen, but it can't hurt
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
			}

			//Grab a cursor here
			generic_ast_node_t* defer_stmt_cursor = ast_cursor->first_child;

			//Ollie lang uniquely allows the user to defer assembly statements. 
			//This can be useful IF you know what you're doing when it comes to assembly.
			//Since, if you defer assembly, that is the entire statement, we only
			//need to worry about emitting it once
			if(defer_stmt_cursor->CLASS == AST_NODE_CLASS_ASM_INLINE_STMT){
				//Emit the inline assembly that we need here
				emit_asm_inline_stmt(current_block, defer_stmt_cursor);

			//Otherwise it's just a regular deferral
			} else {
				//Run through all of the children, emitting their respective
				//expr codes
				while(defer_stmt_cursor != NULL){
					//Let the helper deal with it
					emit_expr_code(current_block, defer_stmt_cursor);
					//Move this up
					defer_stmt_cursor = defer_stmt_cursor->next_sibling;
				}			
			}


		//Handle a labeled statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_LABEL_STMT){
			//This really shouldn't happen, but it can't hurt
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
			}
			
			//We rely on the helper to do it for us
			emit_label_stmt_code(current_block, ast_cursor);

		//Handle a jump statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_JUMP_STMT){
			//This really shouldn't happen, but it can't hurt
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
			}

			//We rely on the helper to do it for us
			emit_jump_stmt_code(current_block, ast_cursor);

		//A very unique case exists in the switch statement. For a switch 
		//statement, we leverage some very unique properties of the enumerable
		//types that it uses
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_SWITCH_STMT){
			//TODO IMPLEMENT

		//These are 100% user generated,
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_ASM_INLINE_STMT){
			//If we find an assembly inline statement, the actuality of it is
			//incredibly easy. All that we need to do is literally take the 
			//user's statement and insert it into the code

			//We'll need a new block here regardless
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
			}

			//Let the helper handle
			emit_asm_inline_stmt(current_block, ast_cursor);

		//This means that we have some kind of expression statement
		} else {
			//This could happen where we have nothing here
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
			}
			
			//Also emit the simplified machine code
			emit_expr_code(current_block, ast_cursor);
		}

		//Advance to the next child
		ast_cursor = ast_cursor->next_sibling;
	}

	//We always return the starting block
	//It is possible that we have a completely NULL compound statement. This returns
	//NULL in that event
	return starting_block;
}


/**
 * A function definition will always be considered a leader statement. As such, it
 * will always have it's own separate block
 */
static basic_block_t* visit_function_definition(generic_ast_node_t* function_node){
	//For error printing
	char info[1000];
	//The starting block
	basic_block_t* function_starting_block = basic_block_alloc();
	//Mark that this is a starting block
	function_starting_block->is_func_entry = 1;
	//The ending block
	basic_block_t* function_ending_block = basic_block_alloc();
	//We very clearly mark this as an ending block
	function_ending_block->is_exit_block = 1;

	//Grab the function record
	symtab_function_record_t* func_record = ((func_def_ast_node_t*)(function_node->node))->func_record;
	//Store this in the entry block
	function_starting_block->func_record = func_record;

	//We don't care about anything until we reach the compound statement
	generic_ast_node_t* func_cursor = function_node->first_child;

	//Developer error here
	if(func_cursor->CLASS != AST_NODE_CLASS_COMPOUND_STMT){
		print_parse_message(PARSE_ERROR, "Expected compound statement as only child to function declaration", func_cursor->line_number);
		exit(0);
	}

	//Create our values package
	values_package_t compound_stmt_values;
	compound_stmt_values.initial_node = func_cursor;
	compound_stmt_values.function_end_block = function_ending_block;
	compound_stmt_values.loop_stmt_start = NULL;
	compound_stmt_values.if_stmt_end_block = NULL;
	compound_stmt_values.for_loop_update_block = NULL;
	compound_stmt_values.loop_stmt_end = NULL;

	//Once we get here, we know that func cursor is the compound statement that we want
	basic_block_t* compound_stmt_block = visit_compound_statement(&compound_stmt_values);

	//If this compound statement is NULL(which is possible) we just add the starting and ending
	//blocks as successors
	if(compound_stmt_block == NULL){
		add_successor(function_starting_block, function_ending_block);
		
		//We'll also throw a warning
		sprintf(info, "Function \"%s\" was given no body", ((func_def_ast_node_t*)(function_node->node))->func_record->func_name);
		print_cfg_message(WARNING, info, func_cursor->line_number);
		//One more warning
		(*num_warnings_ref)++;

	//Otherwise we merge them
	} else {
		//Once we're done with the compound statement, we will merge it into the function
		merge_blocks(function_starting_block, compound_stmt_block);
	}

	//Let's see if we actually made it all the way through and found a return
	basic_block_t* compound_stmt_cursor = function_starting_block;

	//Until we hit the end
	while(compound_stmt_cursor->direct_successor != NULL){
		compound_stmt_cursor = compound_stmt_cursor->direct_successor;
	}

	//Once we hit the end, if this isn't an exit block, we'll make it one
	if(compound_stmt_cursor->is_exit_block == 0){
		//We'll add this in as the ending block
		add_successor(compound_stmt_cursor, function_ending_block);
		compound_stmt_cursor->direct_successor = function_ending_block;
	}

	//Once we get here, we'll now add in any deferred statements to the function ending block
	
	//So long as they aren't empty
	while(is_empty(deferred_stmts) == 0){
		//Add them in one by one
		add_statement(function_ending_block, pop(deferred_stmts));
	}

	perform_function_reachability_analysis(function_node, function_starting_block);

	//We always return the start block
	return function_starting_block;
}


/**
 * Visit a declaration statement
 */
static basic_block_t* visit_declaration_statement(values_package_t* values){
	//Create the basic block
	basic_block_t* decl_stmt_block = basic_block_alloc();

	//Emit the expression code
	emit_expr_code(decl_stmt_block, values->initial_node);

	//Give the block back
	return decl_stmt_block;
}


/**
 * Visit a let statement
 */
static basic_block_t* visit_let_statement(values_package_t* values){
	//Create the basic block
	basic_block_t* let_stmt_node = basic_block_alloc();

	emit_expr_code(let_stmt_node, values->initial_node);

	//Give the block back
	return let_stmt_node;
}


/**
 * Visit the prog node for our CFG. This rule will simply multiplex to all other rules
 * between functions, let statements and declaration statements
 */
static basic_block_t* visit_prog_node(generic_ast_node_t* prog_node){
	//Maintain a start and current block here
	basic_block_t* start_block = NULL;
	basic_block_t* current_block = start_block;

	//A prog node can decay into a function definition, a let statement or otherwise
	generic_ast_node_t* ast_cursor = prog_node->first_child;

	//So long as the AST cursor is not null
	while(ast_cursor != NULL){
		//Process a function statement
		if(ast_cursor->CLASS == AST_NODE_CLASS_FUNC_DEF){
			//Visit the function definition
			basic_block_t* function_block = visit_function_definition(ast_cursor);
			
			//If the start block is null, this becomes the start block
			if(start_block == NULL){
				start_block = function_block;
			//Otherwise, we'll add this as a successor to the current block
			} else {
				add_successor(current_block, function_block);
			}

			//We now need to find where the end of the function block is to have that as our current reference
			current_block = function_block;

			//So long as we don't see the exit statement, we keep going
			while(current_block->is_exit_block == 0){
				//Always follow the path of the direct successor
				current_block = current_block->direct_successor;
			}

			//Finally once we get down here, we have our proper current block

		//Process a let statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_LET_STMT){
			values_package_t values;
			values.initial_node = ast_cursor;

			//We'll visit the block here
			basic_block_t* let_block = visit_let_statement(&values);

			//If the start block is null, then this is the start block. Otherwise, we merge it in
			if(start_block == NULL){
				start_block = let_block;
				current_block = let_block;
			//Just merge with current
			} else {
				current_block =	merge_blocks(current_block, let_block); 
			}

		//Visit a declaration statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_DECL_STMT){
			values_package_t values;
			values.initial_node = ast_cursor;

			//We'll visit the block here
			basic_block_t* decl_block = visit_declaration_statement(&values);

			//If the start block is null, then this is the start block. Otherwise, we merge it in
			if(start_block == NULL){
				start_block = decl_block;
				current_block = decl_block;
			//Just merge with current
			} else {
				current_block = merge_blocks(current_block, decl_block); 
			}

		//Some weird error here
		} else {
			print_parse_message(PARSE_ERROR, "Unrecognizable node found as child to prog node", ast_cursor->line_number);
			(*num_errors_ref)++;
			return create_and_return_err();
		}
		
		//Advance to the next child
		ast_cursor = ast_cursor->next_sibling;
	}

	//Always return the start block
	return start_block;
}


/**
 * Build a cfg from the ground up
*/
cfg_t* build_cfg(front_end_results_package_t results, u_int32_t* num_errors, u_int32_t* num_warnings){
	//Store our references here
	num_errors_ref = num_errors;
	num_warnings_ref = num_warnings;

	//Add this in
	type_symtab = results.type_symtab;

	//Create the stack here
	deferred_stmts = create_stack();
	
	//Create the temp vars symtab
	temp_vars = initialize_variable_symtab();

	//We'll first create the fresh CFG here
	cfg_t* cfg = calloc(1, sizeof(cfg_t));
	//Hold the cfg
	cfg_ref = cfg;

	//For dev use here
	if(results.root->CLASS != AST_NODE_CLASS_PROG){
		print_parse_message(PARSE_ERROR, "Expected prog node as first node", results.root->line_number);
		exit(1);
	}

	//We'll visit the prog node, and let everything else do the rest
	cfg->root = visit_prog_node(results.root);

	//If we get a -1 block ID, this means that the whole thing failed
	if(cfg->root->block_id == -1){
		print_parse_message(PARSE_ERROR, "CFG was unable to be constructed", 0);
		(*num_errors_ref)++;
	}

	//Destroy the deferred statements stack
	destroy_stack(deferred_stmts);
	
	//Destroy the temp variable symtab
	destroy_variable_symtab(temp_vars);

	//FOR PRINTING
	emit_blocks_dfs(cfg);
	
	//Give back the reference
	return cfg;
}
