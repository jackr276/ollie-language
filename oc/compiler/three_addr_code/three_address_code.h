/**
 * Author: Jack Robbins
 *
 * This header file defines methods that are used in the production and interpretation of
 * three address code. Three address code is what the middle-level IR of the compiler
 * is, and occupies the basic blocks of the CFG.
*/

#ifndef THREE_ADDRESS_CODE_H
#define THREE_ADDRESS_CODE_H

//For symtab linking
#include "../symtab/symtab.h"
#include <sys/types.h>

//A struct that holds all knowledge of three address codes
typedef struct three_addr_code_stmt three_addr_code_stmt;
//A struct that holds our three address variables
typedef struct three_addr_var three_addr_var;

/**
 * A three address var may be a temp variable or it may be
 * linked to a non-temp variable. It keeps a generation counter
 * for eventual SSA and type information
*/
struct three_addr_var{
	//For convenience
	char var_name[115];
	//Link to symtab(NULL if not there)
	symtab_variable_record_t* linked_var;
	//Is this a temp variable?
	u_int8_t is_temporary;
	//Store the type info for faster access
	//Types will be used for eventual register assignment
	generic_type_t* type;
};


struct three_addr_code_stmt{
	//A three address code always has 2 operands and an assignee
	three_addr_var* op1;
	three_addr_var* op2;
	three_addr_var* assignee;
	//TODO may add more
};


three_addr_code_stmt* 



#endif
