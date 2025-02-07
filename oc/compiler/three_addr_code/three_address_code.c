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
 * Dynamically allocate and create a binary operation statement
 */


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
		printf("%s <- %s %s %s", stmt->assignee->var_name, stmt->op1->var_name, op, stmt->op2->var_name);
	}
	//TODO ADD MORE

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
 * Deallocate the variable portion of a three address code
*/
void deallocate_three_addr_var(three_addr_var* var){
	//Null check as appropriate
	if(var != NULL){
		free(var);
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
	deallocate_three_addr_var(stmt->op2);

	//Finally free the overall structure
	free(stmt);
}
