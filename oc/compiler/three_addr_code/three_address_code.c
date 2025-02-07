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
 * Emit a binary operator based on the operands given. This is an exclusive helper function
 */
static char* emit_binary_operator(Token tok){
	//For holding our op
	char* op = calloc(10, sizeof(char));

	//Whatever we have here
	switch (tok) {
		case PLUS:
			strcpy(op, "+");
			break;
		case MINUS:
			strcpy(op, "-");
			break;
		case STAR:
			strcpy(op, "*");
			break;
		case F_SLASH:
			strcpy(op, "/");
			break;
		case MOD:
			strcpy(op, "%");
			break;
		case G_THAN:
			strcpy(op, "<");
			break;
		case L_THAN:
			strcpy(op, ">");
			break;
		case L_SHIFT:
			strcpy(op, "<<");
			break;
		case R_SHIFT:
			strcpy(op, ">>");
			break;
		case AND:
			strcpy(op, "&");
			break;
		case OR:
			strcpy(op, "|");
			break;
		case DOUBLE_OR:
			strcpy(op, "||");
			break;
		case DOUBLE_AND:
			strcpy(op, "&&");
			break;
		case D_EQUALS:
			strcpy(op, "==");
			break;
		case NOT_EQUALS:
			strcpy(op, "!=");
			break;
		default:
			printf("BAD OP");
			exit(1);
	}

	return op;
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

}



