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

//The atomically increasing temp name id
static int32_t current_temp_id = 0;

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
 * Dynamically allocate and create a temp var
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

	//We'll now create our temporary variable name
	sprintf(var->var_name, "t%d", increment_and_get_temp_id());

	//Finally we'll bail out
	return var;
}


/**
 * Dynamically allocate and create a non-temp var
*/
three_addr_var_t* emit_var(symtab_variable_record_t* var, u_int8_t assignment, u_int8_t is_label){
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
	
	//We'll increment the current generation
	if(assignment == 1){
		(var->current_generation)++;
	}

	//Finally we'll get the name printed
	if(is_label == 0){
		sprintf(emitted_var->var_name, "%s%d", var->var_name, var->current_generation);
	} else {
		sprintf(emitted_var->var_name, "%s", var->var_name);
	}

	//And we're all done
	return emitted_var;
}


/**
 * Emit a copy of this far
 */
three_addr_var_t* emit_var_copy(three_addr_var_t* var){
	//Let's first create the non-temp variable
	three_addr_var_t* emitted_var = calloc(1, sizeof(three_addr_var_t));

	//Copy the memory
	//memcpy(emitted_var, var, sizeof(three_addr_var_t));
	memmove(emitted_var, var, sizeof(three_addr_var_t));
	
	//Attach it for memory management
	emitted_var->next_created = emitted_vars;
	emitted_vars = emitted_var;


	return emitted_var;
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

	//and give it back
	return stmt;
}


/**
 * Pretty print a three address code statement
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

		//Once we have our op in string form, we can print the whole thing out
		printf("%s <- %s %s %s\n", stmt->assignee->var_name, stmt->op1->var_name, op, stmt->op2->var_name);
	
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

		//Print out
		printf("%s <- %s %s ", stmt->assignee->var_name, stmt->op1->var_name, op);

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
		printf("%s <- %s\n", stmt->assignee->var_name, stmt->op1->var_name);
	} else if(stmt->CLASS == THREE_ADDR_CODE_ASSN_CONST_STMT){
		//First print out the assignee
		printf("%s <- ", stmt->assignee->var_name);

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
		printf("ret");

		//If it has a returned variable
		if(stmt->op1 != NULL){
			printf(" %s", stmt->op1->var_name);
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
		printf(" .L%d\n", stmt->jumping_to_id);

	//If we have a function call go here
	} else if(stmt->CLASS == THREE_ADDR_CODE_FUNC_CALL){
		//First we'll print out the assignment, if one exists
		if(stmt->assignee != NULL){
			printf("%s <- ", stmt->assignee->var_name);
		}

		//No matter what, we'll need to see the "call" keyword, followed
		//by the function name
		printf("call %s(", stmt->func_record->func_name);

		//Now we can go through and print out all of our parameters here
		for(u_int8_t i = 0; i < stmt->func_record->number_of_params; i++){
			//Print this out here
			printf("%s", stmt->params[i]->var_name);

			//If we need to, print out a comma
			if(i != stmt->func_record->number_of_params - 1){
				printf(", ");
			}
		}

		//Now at the very end, close the whole thing out
		printf(")\n");

	//If we have a binary operator with a constant
	} else if (stmt->CLASS == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT){
		//TODO MAY OR MAY NOT NEED
	} else if (stmt->CLASS == THREE_ADDR_CODE_INC_STMT){
		printf("inc %s\n", stmt->assignee->var_name);
	} else if (stmt->CLASS == THREE_ADDR_CODE_DEC_STMT){
		printf("dec %s\n", stmt->assignee->var_name);
	} else if (stmt->CLASS == THREE_ADDR_CODE_BITWISE_NOT_STMT){
		printf("%s <- not %s\n", stmt->assignee->var_name, stmt->op1->var_name);
	} else if(stmt->CLASS == THREE_ADDR_CODE_NEG_STATEMENT){
		printf("%s <- neg %s\n", stmt->assignee->var_name, stmt->op1->var_name);
	} else if (stmt->CLASS == THREE_ADDR_CODE_LOGICAL_NOT_STMT){
		//First we use the test command
		printf("%s <- test %s, %s\n", stmt->assignee->var_name, stmt->op1->var_name, stmt->op1->var_name);
		//Then we "set if equal"(sete) the assigned
		printf("sete %s\n", stmt->assignee->var_name);
		//Then we move it into itself for flag setting purposes
		printf("%s <- %s\n", stmt->assignee->var_name, stmt->assignee->var_name);
	//For a label statement, we need to trim off the $ that it has
	} else if(stmt->CLASS == THREE_ADDR_CODE_LABEL_STMT){
		//Let's print it out
		printf("%s:\n", stmt->assignee->var_name + 1);
	} else if(stmt->CLASS == THREE_ADDR_CODE_DIR_JUMP_STMT){
		printf("jmp %s\n", stmt->assignee->var_name + 1);
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
	dec_stmt->assignee = decrementee;

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
	inc_stmt->assignee = incrementee;

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

	//And that's it, we'll now just give it back
	return stmt;
}


/**
 * Emit a jump statement where we jump to the block with the ID provided
 */
three_addr_code_stmt_t* emit_jmp_stmt_three_addr_code(int32_t jumping_to_id, jump_type_t jump_type){
	//First allocate it
	three_addr_code_stmt_t* stmt = calloc(1, sizeof(three_addr_code_stmt_t));

	//Let's now populate it with values
	stmt->CLASS = THREE_ADDR_CODE_JUMP_STMT;
	stmt->jumping_to_id = jumping_to_id;
	stmt->jump_type = jump_type;

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

	//Give the stmt back
	return stmt;
}

/**
 * Deallocate the variable portion of a three address code
*/
void deallocate_three_addr_var(three_addr_var_t* var){
	//Null check as appropriate
	if(var != NULL){
		free(var);
	}
}

/**
 * Dellocate the constant portion of a three address code
 */
void deallocate_three_addr_const(three_addr_const_t* constant){
	//Null check as appropriate
	if(constant != NULL){
		free(constant);
	}
}


/**
 * Deallocate the entire three address code statement
*/
void deallocate_three_addr_stmt(three_addr_code_stmt_t* stmt){
	//If the statement is null we bail out
	if(stmt == NULL){
		return;
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
