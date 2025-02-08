/**
 * Author: Jack Robbins
 *
 * This is the implementation file for the three_address_code header file
*/

#include "three_address_code.h"
#include <stdio.h>
#include <stdlib.h>

//The atomically increasing temp name id
static int32_t current_temp_id = 0;

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
three_addr_var* emit_temp_var(generic_type_t* type){
	//Let's first create the temporary variable
	three_addr_var* var = calloc(1, sizeof(three_addr_var)); 

	//Mark this as temporary
	var->is_temporary = 1;
	//Store the type info
	var->type = type;

	//We'll now create our temporary variable name
	sprintf(var->var_name, "_t%d", increment_and_get_temp_id());

	//Finally we'll bail out
	return var;
}


/**
 * Dynamically allocate and create a non-temp var
*/
three_addr_var* emit_var(symtab_variable_record_t* var){
	//Let's first create the non-temp variable
	three_addr_var* emitted_var = calloc(1, sizeof(three_addr_var));

	//This is not temporary
	emitted_var->is_temporary = 0;
	//Store the type info
	emitted_var->type = var->type;
	//And store the symtab record
	emitted_var->linked_var = var;

	//Finally we'll get the name printed
	sprintf(emitted_var->var_name, "%s", var->var_name);

	//And we're all done
	return emitted_var;
}


/**
 * Pretty print a three address code statement
*/
void print_three_addr_code_stmt(three_addr_code_stmt* stmt){
	//If it's a binary operator statement(most common), we'll
	//print the whole thing
	if(stmt->CLASS == THREE_ADDR_CODE_BIN_OP_STMT){
		//What is our op?
		char* op;

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
			default:
				printf("BAD OP");
				exit(1);
		}

		//Once we have our op in string form, we can print the whole thing out
		printf("%s <- %s %s %s\n", stmt->assignee->var_name, stmt->op1->var_name, op, stmt->op2->var_name);
	
	//If we have a regular const assignment
	} else if(stmt->CLASS == THREE_ADDR_CODE_ASSN_STMT){
		//We'll print out the left and right ones here
		printf("%s <- %s\n", stmt->assignee->var_name, stmt->op1->var_name);
	} else if(stmt->CLASS == THREE_ADDR_CODE_ASSN_CONST_STMT){
		//First print out the assignee
		printf("%s <- ", stmt->assignee->var_name);

		//Grab our const for convenience
		three_addr_const* constant = stmt->op1_const;

		//We'll now interpret what we have here
		if(constant->const_type == INT_CONST){
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
	}
}


/**
 * Create and return a constant three address var
 */
three_addr_const* emit_constant(generic_ast_node_t* const_node){
	//First we'll dynamically allocate the constant
	three_addr_const* const_var = calloc(1, sizeof(three_addr_const));

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
		//Some very weird error here
		default:
			fprintf(stderr, "Unrecognizable constant type found in constant\n");
			exit(0);
	}
	
	//Once all that is done, we can leave
	return const_var;
}


/**
 * Emit a binary operator three address code statement. Once we're here, we expect that the caller has created and 
 * supplied the appropriate variables
 */
three_addr_code_stmt* emit_bin_op_three_addr_code(three_addr_var* assignee, three_addr_var* op1, Token op, three_addr_var* op2){
	//First allocate it
	three_addr_code_stmt* stmt = calloc(1, sizeof(three_addr_code_stmt));

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
 * Emit an assignment three address code statement. Once we're here, we expect that the caller has created and supplied the
 * appropriate variables
 */
three_addr_code_stmt* emit_assn_stmt_three_addr_code(three_addr_var* assignee, three_addr_var* op1){
	//First allocate it
	three_addr_code_stmt* stmt = calloc(1, sizeof(three_addr_code_stmt));

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
three_addr_code_stmt* emit_assn_const_stmt_three_addr_code(three_addr_var* assignee, three_addr_const* constant){
	//First allocate it
	three_addr_code_stmt* stmt = calloc(1, sizeof(three_addr_code_stmt));

	//Let's now populate it with values
	stmt->CLASS = THREE_ADDR_CODE_ASSN_CONST_STMT;
	stmt->assignee = assignee;
	stmt->op1_const = constant;

	//And that's it, we'll now just give it back
	return stmt;
}

/**
 * Deallocate the variable portion of a three address code
*/
void deallocate_three_addr_var(three_addr_var* var){
	//Null check as appropriate
	if(var != NULL){
		free(var);
	}
}

/**
 * Dellocate the constant portion of a three address code
 */
void deallocate_three_addr_const(three_addr_const* constant){
	//Null check as appropriate
	if(constant != NULL){
		free(constant);
	}
}


/**
 * Deallocate the entire three address code statement
*/
void deallocate_three_addr_stmt(three_addr_code_stmt* stmt){
	//If the statement is null we bail out
	if(stmt == NULL){
		return;
	}
	
	//Otherwise we'll deallocate all variables here
	deallocate_three_addr_var(stmt->assignee);
	deallocate_three_addr_var(stmt->op1);
	deallocate_three_addr_const(stmt->op1_const);
	deallocate_three_addr_var(stmt->op2);

	//Finally free the overall structure
	free(stmt);
}
