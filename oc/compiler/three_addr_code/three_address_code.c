/**
 * Author: Jack Robbins
 *
 * This is the implementation file for the three_address_code header file
*/

#include "three_address_code.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "../cfg/cfg.h"

//For standardization and convenience
#define TRUE 1
#define FALSE 0

//The atomically increasing temp name id
static int32_t current_temp_id = 0;
//The current function
static symtab_function_record_t* current_function = NULL;

//All created vars
three_addr_var_t* emitted_vars = NULL;
//All created constants
three_addr_const_t* emitted_consts = NULL;

/**
 * A helper function for our atomically increasing temp id
 */
static int32_t increment_and_get_temp_id(){
	current_temp_id++;
	return current_temp_id;
}

/**
 * Declare that we are in a new function
 */
void set_new_function(symtab_function_record_t* func){
	//We'll save this up top
	current_function = func;
}


/**
 * Dynamically allocate and create a temp var
 *
 * Temp Vars do NOT have their lightstack initialized. If ever you are using the stack of a temp
 * var, you are doing something seriously incorrect
*/
three_addr_var_t* emit_temp_var(generic_type_t* type){
	//Let's first create the temporary variable
	three_addr_var_t* var = calloc(1, sizeof(three_addr_var_t)); 

	//Attach this for memory management
	var->next_created = emitted_vars;
	emitted_vars = var;

	//Mark this as temporary
	var->is_temporary = 1;
	//Store the type info
	var->type = type;
	//Store the temp var number
	var->temp_var_number = increment_and_get_temp_id();

	//We'll now create our temporary variable name
	sprintf(var->var_name, "t%d", var->temp_var_number);

	//Finally we'll bail out
	return var;
}


/**
 * Dynamically allocate and create a non-temp var. We emit a separate, distinct variable for 
 * each SSA generation. For instance, if we emit x1 and x2, they are distinct. The only thing 
 * that they share is the overall variable that they're linked back to, which stores their type information,
 * etc.
*/
three_addr_var_t* emit_var(symtab_variable_record_t* var, u_int8_t is_label){
	//Let's first create the non-temp variable
	three_addr_var_t* emitted_var = calloc(1, sizeof(three_addr_var_t));

	//Attach it for memory management
	emitted_var->next_created = emitted_vars;
	emitted_vars = emitted_var;

	//This is not temporary
	emitted_var->is_temporary = 0;
	//Store the type info
	emitted_var->type = var->type;
	//And store the symtab record
	emitted_var->linked_var = var;

	sprintf(emitted_var->var_name, "%s", var->var_name);

	//And we're all done
	return emitted_var;
}


/**
 * Emit a copy of this variable
 */
three_addr_var_t* emit_var_copy(three_addr_var_t* var){
	//Let's first create the non-temp variable
	three_addr_var_t* emitted_var = calloc(1, sizeof(three_addr_var_t));

	//Copy the memory
	memcpy(emitted_var, var, sizeof(three_addr_var_t));
	
	//Attach it for memory management
	emitted_var->next_created = emitted_vars;
	emitted_vars = emitted_var;


	return emitted_var;
}

/**
 * Emit a statement that is in LEA form
 */
three_addr_code_stmt_t* emit_lea_stmt_three_addr_code(three_addr_var_t* assignee, three_addr_var_t* op1, three_addr_var_t* op2, u_int64_t type_size){
	//First we allocate it
	three_addr_code_stmt_t* stmt = calloc(1, sizeof(three_addr_code_stmt_t));

	//Now we'll make our populations
	stmt->CLASS = THREE_ADDR_CODE_LEA_STMT;
	stmt->assignee = assignee;
	stmt->op1 = op1;
	stmt->op2 = op2;
	stmt->lea_multiplicator = type_size;
	//What function are we in
	stmt->function = current_function;
	//This is an address, so it must be a quad word
	assignee->variable_size = QUAD_WORD;

	//And now we give it back
	return stmt;
}

/**
 * Emit a copy of this statement
 */
three_addr_code_stmt_t* emit_label_stmt_three_addr_code(three_addr_var_t* label){
	//Let's first allocate the statement
	three_addr_code_stmt_t* stmt = calloc(1, sizeof(three_addr_code_stmt_t));

	//All we do now is give this the label 
	stmt->assignee = label;
	//Note the class too
	stmt->CLASS = THREE_ADDR_CODE_LABEL_STMT;
	//What function are we in
	stmt->function = current_function;
	//And give it back
	return stmt;
}


/**
 * Emit a direct jump statement. This is used only with jump statements the user has made
 */
three_addr_code_stmt_t* emit_dir_jmp_stmt_three_addr_code(three_addr_var_t* jumping_to){
	//First allocate it
	three_addr_code_stmt_t* stmt = calloc(1, sizeof(three_addr_code_stmt_t));

	//Now all we need to do is give it the label
	stmt->assignee = jumping_to;
	//Note the class too
	stmt->CLASS = THREE_ADDR_CODE_DIR_JUMP_STMT;
	//What function are we in
	stmt->function = current_function;
	//and give it back
	return stmt;
}


/**
 * Directly emit an idle statement
 */
three_addr_code_stmt_t* emit_idle_statement_three_addr_code(){
	//First we allocate
	three_addr_code_stmt_t* stmt = calloc(1, sizeof(three_addr_code_stmt_t));

	//Store the class
	stmt->CLASS = THREE_ADDR_CODE_IDLE_STMT;
	//What function are we in
	stmt->function = current_function;
	//And we're done
	return stmt;
}


/**
 * Print a variable in name only. There are no spaces around the variable, and there
 * will be no newline inserted at all. This is meant solely for the use of the "print_three_addr_code_stmt"
 * and nothing more. This function is also designed to take into account the indirection aspected as well
 */
void print_variable(three_addr_var_t* variable, variable_printing_mode_t mode){
	//If we have a block header, we will NOT print out any indirection info
	//We will first print out any and all indirection("(") opening parens
	for(u_int16_t i = 0; mode != PRINTING_VAR_BLOCK_HEADER && i < variable->indirection_level; i++){
		printf("(");
	}
	
	//Print the variables declared name out -- along with it's SSA generation
	printf("%s", variable->var_name);

	//Lastly we print out the remaining indirection characters
	for(u_int16_t i = 0; mode != PRINTING_VAR_BLOCK_HEADER && i < variable->indirection_level; i++){
		printf(")");
	}
}


/**
 * Pretty print a three address code statement
 *
*/
void print_three_addr_code_stmt(three_addr_code_stmt_t* stmt){
	//If it's a binary operator statement(most common), we'll
	//print the whole thing
	if(stmt->CLASS == THREE_ADDR_CODE_BIN_OP_STMT){
		//What is our op?
		char* op = "";

		//Whatever we have here
		switch (stmt->op) {
			case PLUS:
				op = "+";
				break;
			case MINUS:
				op = "-";
				break;
			case STAR:
				op = "*";
				break;
			case F_SLASH:
				op = "/";
				break;
			case MOD:
				op = "%";
				break;
			case G_THAN:
				op = ">";
				break;
			case L_THAN:
				op = "<";
				break;
			case L_SHIFT:
				op = "<<";
				break;
			case R_SHIFT:
				op = ">>";
				break;
			case AND:
				op = "&";
				break;
			case OR:
				op = "|";
				break;
			case DOUBLE_OR:
				op = "||";
				break;
			case DOUBLE_AND:
				op = "&&";
				break;
			case D_EQUALS:
				op = "==";
				break;
			case NOT_EQUALS:
				op = "!=";
				break;
			case G_THAN_OR_EQ:
				op = ">=";
				break;
			case L_THAN_OR_EQ:
				op = "<=";
				break;
			default:
				printf("BAD OP");
				exit(1);
		}

		//This one comes first
		print_variable(stmt->assignee, PRINTING_VAR_INLINE);

		//Then the arrow
		printf(" <- ");

		//Now we'll do op1, token, op2
		print_variable(stmt->op1, PRINTING_VAR_INLINE);
		printf(" %s ", op);
		print_variable(stmt->op2, PRINTING_VAR_INLINE);

		//And end it out here
		printf("\n");

	//If we have a bin op with const
	} else if(stmt->CLASS == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT){
		//What is our op?
		char* op = "";

		//Whatever we have here
		switch (stmt->op) {
			case PLUS:
				op = "+";
				break;
			case MINUS:
				op = "-";
				break;
			case STAR:
				op = "*";
				break;
			case F_SLASH:
				op = "/";
				break;
			case MOD:
				op = "%";
				break;
			case G_THAN:
				op = ">";
				break;
			case L_THAN:
				op = "<";
				break;
			case L_SHIFT:
				op = "<<";
				break;
			case R_SHIFT:
				op = ">>";
				break;
			case AND:
				op = "&";
				break;
			case OR:
				op = "|";
				break;
			case DOUBLE_OR:
				op = "||";
				break;
			case DOUBLE_AND:
				op = "&&";
				break;
			case D_EQUALS:
				op = "==";
				break;
			case NOT_EQUALS:
				op = "!=";
				break;
			case G_THAN_OR_EQ:
				op = ">=";
				break;
			case L_THAN_OR_EQ:
				op = "<=";
				break;
			default:
				printf("BAD OP");
				exit(1);
		}

		//This one comes first
		print_variable(stmt->assignee, PRINTING_VAR_INLINE);

		//Then the arrow
		printf(" <- ");

		//Now we'll do op1, token, op2
		print_variable(stmt->op1, PRINTING_VAR_INLINE);
		printf(" %s ", op);

		//Grab our const for convenience
		three_addr_const_t* constant = stmt->op1_const;

		//We'll now interpret what we have here
		if(constant->const_type == INT_CONST || constant->const_type == HEX_CONST){
			printf("0x%x\n", constant->int_const);
		} else if(constant->const_type == LONG_CONST){
			printf("0x%lx\n", constant->long_const);
		} else if(constant->const_type == FLOAT_CONST){
			printf("%f\n", constant->float_const);
		} else if(constant->const_type == CHAR_CONST){
			printf("'%c'\n", constant->char_const);
		} else {
			printf("\"%s\"\n", constant->str_const);
		}
	
	//If we have a regular const assignment
	} else if(stmt->CLASS == THREE_ADDR_CODE_ASSN_STMT){
		//We'll print out the left and right ones here
		print_variable(stmt->assignee, PRINTING_VAR_INLINE);
		printf(" <- ");
		print_variable(stmt->op1, PRINTING_VAR_INLINE);
		printf("\n");
	} else if(stmt->CLASS == THREE_ADDR_CODE_ASSN_CONST_STMT){
		//First print out the assignee
		print_variable(stmt->assignee, PRINTING_VAR_INLINE);
		printf(" <- ");

		//Grab our const for convenience
		three_addr_const_t* constant = stmt->op1_const;

		//We'll now interpret what we have here
		if(constant->const_type == INT_CONST || constant->const_type == HEX_CONST){
			printf("0x%x\n", constant->int_const);
		} else if(constant->const_type == LONG_CONST){
			printf("0x%lx\n", constant->long_const);
		} else if(constant->const_type == FLOAT_CONST){
			printf("%f\n", constant->float_const);
		} else if(constant->const_type == CHAR_CONST){
			printf("'%c'\n", constant->char_const);
		} else {
			printf("\"%s\"\n", constant->str_const);
		}
	//Print out a return statement
	} else if(stmt->CLASS == THREE_ADDR_CODE_RET_STMT){
		//Use asm keyword here, getting close to machine code
		printf("ret ");

		//If it has a returned variable
		if(stmt->op1 != NULL){
			print_variable(stmt->op1, PRINTING_VAR_INLINE);
		}
		
		//No matter what, print a newline
		printf("\n");

	//Print out a jump statement
	} else if(stmt->CLASS == THREE_ADDR_CODE_JUMP_STMT){
		//Use asm keyword here, getting close to machine code
		switch(stmt->jump_type){
			case JUMP_TYPE_JE:
				printf("je");
				break;
			case JUMP_TYPE_JNE:
				printf("jne");
				break;
			case JUMP_TYPE_JG:
				printf("jg");
				break;
			case JUMP_TYPE_JL:
				printf("jl");
				break;
			case JUMP_TYPE_JNZ:
				printf("jnz");
				break;
			case JUMP_TYPE_JZ:
				printf("jz");
				break;
			case JUMP_TYPE_JMP:
				printf("jmp");
				break;
			case JUMP_TYPE_JGE:
				printf("jge");
				break;
			case JUMP_TYPE_JLE:
				printf("jle");
				break;
			default:
				printf("jmp");
				break;
		}

		//Then print out the block label
		printf(" .L%d\n", ((basic_block_t*)(stmt->jumping_to_block))->block_id);

	//If we have a function call go here
	} else if(stmt->CLASS == THREE_ADDR_CODE_FUNC_CALL){
		//First we'll print out the assignment, if one exists
		if(stmt->assignee != NULL){
			//Print the variable and assop out
			print_variable(stmt->assignee, PRINTING_VAR_INLINE);
			printf(" <- ");
		}

		//No matter what, we'll need to see the "call" keyword, followed
		//by the function name
		printf("call %s(", stmt->func_record->func_name);

		//Grab this out
		dynamic_array_t* func_params = stmt->function_parameters;

		//Now we can go through and print out all of our parameters here
		for(u_int16_t i = 0; func_params != NULL && i < func_params->current_index; i++){
			//Grab it out
			three_addr_var_t* func_param = dynamic_array_get_at(func_params, i);
			
			//Print this out here
			print_variable(func_param, PRINTING_VAR_INLINE);

			//If we need to, print out a comma
			if(i != func_params->current_index - 1){
				printf(", ");
			}
		}

		//Now at the very end, close the whole thing out
		printf(")\n");

	//If we have a binary operator with a constant
	} else if (stmt->CLASS == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT){
		//TODO MAY OR MAY NOT NEED
	} else if (stmt->CLASS == THREE_ADDR_CODE_INC_STMT){
		printf("inc ");
		print_variable(stmt->assignee, PRINTING_VAR_INLINE);
		printf("\n");
	} else if (stmt->CLASS == THREE_ADDR_CODE_DEC_STMT){
		printf("dec ");
		print_variable(stmt->assignee, PRINTING_VAR_INLINE);
		printf("\n");
	} else if (stmt->CLASS == THREE_ADDR_CODE_BITWISE_NOT_STMT){
		print_variable(stmt->assignee, PRINTING_VAR_INLINE);
		printf(" <- not ");
		print_variable(stmt->op1, PRINTING_VAR_INLINE);
		printf("\n");
	} else if(stmt->CLASS == THREE_ADDR_CODE_NEG_STATEMENT){
		print_variable(stmt->assignee, PRINTING_VAR_INLINE);
		printf(" <- neg ");
		print_variable(stmt->op1, PRINTING_VAR_INLINE);
		printf("\n");
	} else if (stmt->CLASS == THREE_ADDR_CODE_LOGICAL_NOT_STMT){
		print_variable(stmt->assignee, PRINTING_VAR_INLINE);
		//First we use the test command
		printf(" <- test ");
		print_variable(stmt->op1, PRINTING_VAR_INLINE);
		printf(", ");
		print_variable(stmt->op1, PRINTING_VAR_INLINE);
		printf("\n");
		//Then we "set if equal"(sete) the assigned
		printf("sete ");
		print_variable(stmt->assignee, PRINTING_VAR_INLINE);
		printf("\n");
		//Then we move it into itself for flag setting purposes
		print_variable(stmt->assignee, PRINTING_VAR_INLINE);
		printf(" <- ");
		print_variable(stmt->assignee, PRINTING_VAR_INLINE);
		printf("\n");

	//For a label statement, we need to trim off the $ that it has
	} else if(stmt->CLASS == THREE_ADDR_CODE_LABEL_STMT){
		//Let's print it out. This is an instance where we will not use the print var
		printf("%s:\n", stmt->assignee->var_name + 1);
	} else if(stmt->CLASS == THREE_ADDR_CODE_DIR_JUMP_STMT){
		//This is an instance where we will not use the print var
		printf("jmp %s\n", stmt->assignee->var_name + 1);
	//Display an assembly inline statement
	} else if(stmt->CLASS == THREE_ADDR_CODE_ASM_INLINE_STMT){
		//Should already have a trailing newline
		printf("%s", stmt->inlined_assembly);
	} else if(stmt->CLASS == THREE_ADDR_CODE_IDLE_STMT){
		//Just print a nop
		printf("nop\n");
	//If we have a lea statement, we will print it out in plain algebraic form here
	} else if(stmt->CLASS == THREE_ADDR_CODE_LEA_STMT){
		//Var name comes first
		print_variable(stmt->assignee, PRINTING_VAR_INLINE);

		//Print the assignment operator
		printf(" <- ");

		//Now print out the rest in order
		print_variable(stmt->op1, PRINTING_VAR_INLINE);
		//Then we have a plus
		printf(" + ");
		//Then we have the third one, times some multiplier
		print_variable(stmt->op2, PRINTING_VAR_INLINE);

		//And the finishing sequence
		printf(" * %ld\n", stmt->lea_multiplicator);
	//Print out a phi function 
	} else if(stmt->CLASS == THREE_ADDR_CODE_PHI_FUNC){
		//Print it in block header mode
		print_variable(stmt->assignee, PRINTING_VAR_BLOCK_HEADER);
		printf(" <- PHI(");

		//For convenience
		dynamic_array_t* phi_func_params = stmt->phi_function_parameters;

		//Now run through all of the parameters
		for(u_int16_t _ = 0; phi_func_params != NULL && _ < phi_func_params->current_index; _++){
			//Print out the variable
			print_variable(dynamic_array_get_at(phi_func_params, _), PRINTING_VAR_BLOCK_HEADER);

			//If it isn't the very last one, add a comma space
			if(_ != phi_func_params->current_index - 1){
				printf(", ");
			}
		}

		printf(")\n");
	//Print out a conditional branch statement
	} else if(stmt->CLASS == THREE_ADDR_CODE_COND_BRANCH_STMT){
		printf("CBR(");

		//Print out the assignee
		print_variable(stmt->assignee, PRINTING_VAR_INLINE);

		basic_block_t* if_target = stmt->if_branch_target;
		basic_block_t* else_target = stmt->else_branch_target;
		printf(", .L%d, .L%d)\n", if_target->block_id, else_target->block_id);
	}
}


/**
 * Emit a decrement instruction
 */
three_addr_code_stmt_t* emit_dec_stmt_three_addr_code(three_addr_var_t* decrementee){
	//First allocate it
	three_addr_code_stmt_t* dec_stmt = calloc(1, sizeof(three_addr_code_stmt_t));

	//Now we populate
	dec_stmt->CLASS = THREE_ADDR_CODE_DEC_STMT;
	dec_stmt->assignee = emit_var_copy(decrementee);
	dec_stmt->op1 = decrementee;
	//What function are we in
	dec_stmt->function = current_function;
	//And give it back
	return dec_stmt;
}


/**
 * Emit a decrement instruction
 */
three_addr_code_stmt_t* emit_inc_stmt_three_addr_code(three_addr_var_t* incrementee){
	//First allocate it
	three_addr_code_stmt_t* inc_stmt = calloc(1, sizeof(three_addr_code_stmt_t));

	//Now we populate
	inc_stmt->CLASS = THREE_ADDR_CODE_INC_STMT;
	inc_stmt->assignee = emit_var_copy(incrementee);
	inc_stmt->op1 = incrementee;
	//What function are we in
	inc_stmt->function = current_function;
	//And give it back
	return inc_stmt;
}


/**
 * Create and return a constant three address var
 */
three_addr_const_t* emit_constant(generic_ast_node_t* const_node){
	//First we'll dynamically allocate the constant
	three_addr_const_t* const_var = calloc(1, sizeof(three_addr_const_t));

	//Attach it for memory management
	const_var->next_created = emitted_consts;
	emitted_consts = const_var;

	//Grab a reference to the const node for convenience
	constant_ast_node_t* const_node_raw = (constant_ast_node_t*)(const_node->node);

	//Now we'll assign the appropriate values
	const_var->const_type = const_node_raw->constant_type; 
	const_var->type = const_node->inferred_type;

	//Now based on what type we have we'll make assignments
	switch(const_var->const_type){
		case CHAR_CONST:
			const_var->char_const = const_node_raw->char_val;
			break;
		case INT_CONST:
			const_var->int_const = const_node_raw->int_val;
			break;
		case FLOAT_CONST:
			const_var->float_const = const_node_raw->float_val;
			break;
		case STR_CONST:
			strcpy(const_var->str_const, const_node_raw->string_val);
			break;
		case LONG_CONST:
			const_var->long_const = const_node_raw->long_val;
			break;
		case HEX_CONST:
			const_var->int_const = const_node_raw->int_val;
			break;
		//Some very weird error here
		default:
			fprintf(stderr, "Unrecognizable constant type found in constant\n");
			exit(0);
	}
	
	//Once all that is done, we can leave
	return const_var;
}


/**
 * Emit a return statement. The returnee variable may or may not be null
 */
three_addr_code_stmt_t* emit_ret_stmt_three_addr_code(three_addr_var_t* returnee){
	//First allocate it
	three_addr_code_stmt_t* stmt = calloc(1, sizeof(three_addr_code_stmt_t));

	//Let's now populate it appropriately
	stmt->CLASS = THREE_ADDR_CODE_RET_STMT;
	//Set op1 to be the returnee
	stmt->op1 = returnee;
	//What function are we in
	stmt->function = current_function;
	//And that's all, so we'll hop out
	return stmt;
}


/**
 * Emit a binary operator three address code statement. Once we're here, we expect that the caller has created and 
 * supplied the appropriate variables
 */
three_addr_code_stmt_t* emit_bin_op_three_addr_code(three_addr_var_t* assignee, three_addr_var_t* op1, Token op, three_addr_var_t* op2){
	//First allocate it
	three_addr_code_stmt_t* stmt = calloc(1, sizeof(three_addr_code_stmt_t));

	//Let's now populate it with the appropriate values
	stmt->CLASS = THREE_ADDR_CODE_BIN_OP_STMT;
	stmt->assignee = assignee;
	stmt->op1 = op1;
	stmt->op = op;
	stmt->op2 = op2;
	//What function are we in
	stmt->function = current_function;

	//If the operator is a || or && operator, then this statement is eligible for short circuiting
	if(op == DOUBLE_AND || op == DOUBLE_OR){
		stmt->is_short_circuit_eligible = TRUE;
	}

	//Give back the newly allocated statement
	return stmt;
}


/**
 * Emit a binary operation with a constant three address code statement
 */
three_addr_code_stmt_t* emit_bin_op_with_const_three_addr_code(three_addr_var_t* assignee, three_addr_var_t* op1, Token op, three_addr_const_t* op2){
	//First allocate it
	three_addr_code_stmt_t* stmt = calloc(1, sizeof(three_addr_code_stmt_t));

	//Let's now populate it with the appropriate values
	stmt->CLASS = THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT;
	stmt->assignee = assignee;
	stmt->op1 = op1;
	stmt->op = op;
	stmt->op1_const = op2;
	//What function are we in
	stmt->function = current_function;
	//Give back the newly allocated statement
	return stmt;
}


/**
 * Emit an assignment three address code statement. Once we're here, we expect that the caller has created and supplied the
 * appropriate variables
 */
three_addr_code_stmt_t* emit_assn_stmt_three_addr_code(three_addr_var_t* assignee, three_addr_var_t* op1){
	//First allocate it
	three_addr_code_stmt_t* stmt = calloc(1, sizeof(three_addr_code_stmt_t));

	//Let's now populate it with values
	stmt->CLASS = THREE_ADDR_CODE_ASSN_STMT;
	stmt->assignee = assignee;
	stmt->op1 = op1;
	//What function are we in
	stmt->function = current_function;
	//And that's it, we'll just leave our now
	return stmt;
}


/**
 * Emit an assignment "three" address code statement
 */
three_addr_code_stmt_t* emit_assn_const_stmt_three_addr_code(three_addr_var_t* assignee, three_addr_const_t* constant){
	//First allocate it
	three_addr_code_stmt_t* stmt = calloc(1, sizeof(three_addr_code_stmt_t));

	//Let's now populate it with values
	stmt->CLASS = THREE_ADDR_CODE_ASSN_CONST_STMT;
	stmt->assignee = assignee;
	stmt->op1_const = constant;
	//What function are we in
	stmt->function = current_function;
	//And that's it, we'll now just give it back
	return stmt;
}


/**
 * Emit a jump statement where we jump to the block with the ID provided
 */
three_addr_code_stmt_t* emit_jmp_stmt_three_addr_code(void* jumping_to_block, jump_type_t jump_type){
	//First allocate it
	three_addr_code_stmt_t* stmt = calloc(1, sizeof(three_addr_code_stmt_t));

	//Let's now populate it with values
	stmt->CLASS = THREE_ADDR_CODE_JUMP_STMT;
	stmt->jumping_to_block = jumping_to_block;
	stmt->jump_type = jump_type;
	//What function are we in
	stmt->function = current_function;
	//Give the statement back
	return stmt;
}


/**
 * Emit a function call statement where we're calling the function record provided
 */
three_addr_code_stmt_t* emit_func_call_three_addr_code(symtab_function_record_t* func_record, three_addr_var_t* assigned_to){
	//First allocate it
	three_addr_code_stmt_t* stmt = calloc(1, sizeof(three_addr_code_stmt_t));

	//Let's now populate it with values
	stmt->CLASS = THREE_ADDR_CODE_FUNC_CALL;
	stmt->func_record = func_record;
	stmt->assignee = assigned_to;
	//What function are we in
	stmt->function = current_function;
	//We do NOT add parameters here, instead we had them in the CFG function
	//Just give back the result
	return stmt;
}


/**
 * Emit an int constant direct 
 */
three_addr_const_t* emit_int_constant_direct(int int_const){
	three_addr_const_t* constant = calloc(1, sizeof(three_addr_const_t));

	//Attach it for memory management
	constant->next_created = emitted_consts;
	emitted_consts = constant;

	//Store the class
	constant->const_type = INT_CONST;
	//Store the int value
	constant->int_const = int_const;


	//Return out
	return constant;
}


/**
 * Emit a negation statement
 */
three_addr_code_stmt_t* emit_neg_stmt_three_addr_code(three_addr_var_t* assignee, three_addr_var_t* negatee){
	//First we'll create the negation
	three_addr_code_stmt_t* stmt = calloc(1, sizeof(three_addr_code_stmt_t));

	//Now we'll assign whatever we need
	stmt->CLASS = THREE_ADDR_CODE_NEG_STATEMENT;
	stmt->assignee = assignee;
	stmt->op1 = negatee;
	//What function are we in
	stmt->function = current_function;

	//Give it back
	return stmt;
}


/**
 * Emit a not instruction 
 */
three_addr_code_stmt_t* emit_not_stmt_three_addr_code(three_addr_var_t* var){
	//First allocate it
	three_addr_code_stmt_t* stmt = calloc(1, sizeof(three_addr_code_stmt_t));

	//Let's make it a not stmt
	stmt->CLASS = THREE_ADDR_CODE_BITWISE_NOT_STMT;
	//The only var here is the assignee
	stmt->assignee = var;
	//For the potential of temp variables
	stmt->op1 = var;
	//What function are we in
	stmt->function = current_function;

	//Give the statement back
	return stmt;
}


/**
 * Emit a logical not statement
 */
three_addr_code_stmt_t* emit_logical_not_stmt_three_addr_code(three_addr_var_t* assignee, three_addr_var_t* var){
	//First allocate it
	three_addr_code_stmt_t* stmt = calloc(1, sizeof(three_addr_code_stmt_t));

	//Let's make it a logical not stmt
	stmt->CLASS = THREE_ADDR_CODE_LOGICAL_NOT_STMT;
	stmt->assignee = assignee;
	//Leave it in here
	stmt->op1 = var;
	//What function are we in
	stmt->function = current_function;

	//Give the stmt back
	return stmt;
}


/**
 * Emit an assembly inline statement. Once emitted, these statements are final and are ignored
 * by any future optimizations
 */
three_addr_code_stmt_t* emit_asm_statement_three_addr_code(asm_inline_stmt_ast_node_t* asm_inline_node){
	//First we allocate it
	three_addr_code_stmt_t* stmt = calloc(1, sizeof(three_addr_code_stmt_t));

	//Store the class
	stmt->CLASS = THREE_ADDR_CODE_ASM_INLINE_STMT;

	//Then we'll allocate the needed space for the string holding the assembly
	stmt->inlined_assembly = calloc(asm_inline_node->max_length, sizeof(char));

	//Copy the assembly over
	strncpy(stmt->inlined_assembly, asm_inline_node->asm_line_statements, asm_inline_node->length);
	//What function are we in
	stmt->function = current_function;

	//And we're done, now we'll bail out
	return stmt;
}


/**
 * Emit a phi function for a given variable. Once emitted, these statements are compiler exclusive,
 * but they are needed for our optimization
 */
three_addr_code_stmt_t* emit_phi_function(symtab_variable_record_t* variable){
	//First we allocate it
	three_addr_code_stmt_t* stmt = calloc(1, sizeof(three_addr_code_stmt_t));

	//We'll just store the assignee here, no need for anything else
	stmt->assignee = emit_var(variable, FALSE);

	//Note what kind of node this is
	stmt->CLASS = THREE_ADDR_CODE_PHI_FUNC;
	//What function are we in
	stmt->function = current_function;

	//And give the statement back
	return stmt;
}


/**
 * Emit a conditional branch statement
 */
three_addr_code_stmt_t* emit_cbr_statement_three_addr_code(three_addr_var_t* assignee, void* if_branch_target, void* else_branch_target){
	//First we allocate it
	three_addr_code_stmt_t* stmt = calloc(1, sizeof(three_addr_code_stmt_t));
	
	//Store these all
	stmt->assignee = assignee;
	stmt->if_branch_target = if_branch_target;
	stmt->else_branch_target = else_branch_target;

	//Mark the type
	stmt->CLASS = THREE_ADDR_CODE_COND_BRANCH_STMT;

	//Mark the function
	stmt->function = current_function;

	//And give it back
	return stmt;
}

/**
 * Emit a complete copy of whatever was in here previously
 */
three_addr_code_stmt_t* copy_three_addr_code_stmt(three_addr_code_stmt_t* copied){
	//First we allocate
	three_addr_code_stmt_t* copy = calloc(1, sizeof(three_addr_code_stmt_t));

	//Perform a complete memory copy
	memcpy(copy, copied, sizeof(three_addr_code_stmt_t));

	//Now we'll check for special values. NOTE: if we're using this, we should NOT have
	//any phi functions OR assembly in here. The only thing that we might have are
	//function calls
	
	//Null these out, better safe than sorry
	copy->phi_function_parameters = NULL;
	copy->inlined_assembly = NULL;
	copy->next_statement = NULL;
	copy->previous_statement = NULL;
	
	//If we have function call parameters, emit a copy of them
	if(copied->function_parameters != NULL){
		copy->function_parameters = clone_dynamic_array(copied->function_parameters);
	}

	//Give back the copied one
	return copied;
}


/**
 * Are two variables equal? A helper method for searching
 */
u_int8_t variables_equal(three_addr_var_t* a, three_addr_var_t* b){
	//Easy way to tell here
	if(a == NULL || b == NULL){
		return FALSE;
	}

	//Another easy way to tell
	if(a->is_temporary != b->is_temporary){
		return FALSE;
	}

	//Another way to tell
	if(a->indirection_level != b->indirection_level){
		return FALSE;
	}

	//For temporary variables, the comparison is very easy
	if(a->is_temporary){
		if(a->temp_var_number == b->temp_var_number){
			return TRUE;
		} else {
			return FALSE;
		}
	//Otherwise, we're comparing two non-temp variables
	} else {
		//Do they reference the same overall variable?
		if(a->linked_var != b->linked_var){
			return FALSE;
		}

		//Final check here via string comparison
		if(strcmp(a->var_name, b->var_name) == 0){
			return TRUE;
		}
	}

	//If we get here it's a no go
	return FALSE;
}


/**
 * Deallocate the variable portion of a three address code
*/
void three_addr_var_dealloc(three_addr_var_t* var){
	//Null check as appropriate
	if(var != NULL){
		free(var);
	}
}

/**
 * Dellocate the constant portion of a three address code
 */
void three_addr_const_dealloc(three_addr_const_t* constant){
	//Null check as appropriate
	if(constant != NULL){
		free(constant);
	}
}


/**
 * Deallocate the entire three address code statement
*/
void three_addr_stmt_dealloc(three_addr_code_stmt_t* stmt){
	//If the statement is null we bail out
	if(stmt == NULL){
		return;
	}

	//If we have an asm inline statement
	if(stmt->CLASS == THREE_ADDR_CODE_ASM_INLINE_STMT){
		//We must also free the pointer in here
		free(stmt->inlined_assembly);
	}

	//If we have a phi function, deallocate the dynamic array
	if(stmt->phi_function_parameters != NULL){
		dynamic_array_dealloc(stmt->phi_function_parameters);
	}

	//If we have function parameters get rid of them
	if(stmt->function_parameters != NULL){
		dynamic_array_dealloc(stmt->function_parameters);
	}
	
	//Free the overall stmt -- variables handled elsewhere
	free(stmt);
}


/**
 * Deallocate all variables using our global list strategy
*/
void deallocate_all_vars(){
	//For holding 
	three_addr_var_t* temp;

	//Run through the whole list
	while(emitted_vars != NULL){
		//Hold onto it here
		temp = emitted_vars;
		//Advance
		emitted_vars = emitted_vars->next_created;
		//Free the one we just had
		free(temp);
	}
}


/**
 * Deallocate all constants using our global list strategy
*/
void deallocate_all_consts(){
	//For holding
	three_addr_const_t* temp;

	//Run through the whole list
	while(emitted_consts != NULL){
		//Hold onto it here
		temp = emitted_consts;
		//Advance
		emitted_consts = emitted_consts->next_created;
		//Deallocate temp
		free(temp);
	}
}
