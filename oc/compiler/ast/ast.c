/**
 * Author: Jack Robbins
 *
 * This file implements the APIs defined in ast.h
*/

//Link to AST
#include "ast.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

//Initialize our created nodes array to be empty
static dynamic_array_t created_nodes;

/**
 * Initialize the AST system by creating the created_nodes
 * array
 */
void initialize_ast_system(){
	created_nodes = dynamic_array_alloc();
}

/**
 * Coerce a constant node's value to fit the value of it's "inferred type". This should be used after
 * we've done some constant operations inside of the parser that may require us to update the internal
 * constant type. 
 *
 * It is assumed that the caller has already set the "inferred type" to be what we're coercing to.
 *
 * Do note that the inferred type will always be *the same size or larger* than the given constant type
 * that is already in the system. This is why, as the constant types get larger, the options to coerce to
 * get smaller
 */
void coerce_constant(generic_ast_node_t* constant_node){
	//We have an inferred type here
	generic_type_t* inferred_type = dealias_type(constant_node->inferred_type);

	//If it's not a basic type then something went very wrong
	if(inferred_type->type_class != TYPE_CLASS_BASIC){
		printf("Fatal internal compiler error. Constant with a non-basic raw type of %s discovered\n", inferred_type->type_name.string);
		exit(1);
	}

	//Go based on the original type
	switch(constant_node->constant_type){
		//Now in here, we'll go based on the basic type of what our inferred
		//type is and perform a move operation(just reassignment) to have the appropriate
		//expansion
		case CHAR_CONST:
			switch(inferred_type->basic_type_token){
				case U8:
					constant_node->constant_type = BYTE_CONST_FORCE_U;
					constant_node->constant_value.unsigned_byte_value = constant_node->constant_value.char_value;
					break;

				case I8:
					constant_node->constant_type = BYTE_CONST;
					constant_node->constant_value.signed_byte_value = constant_node->constant_value.char_value;
					break;

				case U16:
					constant_node->constant_type = SHORT_CONST_FORCE_U;
					constant_node->constant_value.unsigned_short_value = constant_node->constant_value.char_value;
					break;
					
				case I16:
					constant_node->constant_type = SHORT_CONST;
					constant_node->constant_value.signed_short_value = constant_node->constant_value.char_value;
					break;

				case U32:
					constant_node->constant_type = INT_CONST;
					constant_node->constant_value.unsigned_int_value = constant_node->constant_value.char_value;
					break;

				case I32:
					constant_node->constant_type = INT_CONST;
					constant_node->constant_value.signed_int_value = constant_node->constant_value.char_value;
					break;

				case F32:
					constant_node->constant_type = FLOAT_CONST;
					constant_node->constant_value.float_value = constant_node->constant_value.char_value;
					break;

				case I64:
					constant_node->constant_type = LONG_CONST;
					constant_node->constant_value.signed_long_value = constant_node->constant_value.char_value;
					break;

				case U64:
					constant_node->constant_type = LONG_CONST_FORCE_U;
					constant_node->constant_value.unsigned_long_value = constant_node->constant_value.char_value;
					break;

				case F64:
					constant_node->constant_type = DOUBLE_CONST;
					constant_node->constant_value.double_value = constant_node->constant_value.char_value;
					break;

				default:
					break;
			}

			break;

		case BYTE_CONST:
			switch(inferred_type->basic_type_token){
				case CHAR:
					constant_node->constant_type = CHAR_CONST;
					constant_node->constant_value.char_value = constant_node->constant_value.signed_byte_value;
					break;

				case U8:
					constant_node->constant_type = BYTE_CONST_FORCE_U;
					constant_node->constant_value.unsigned_byte_value = constant_node->constant_value.signed_byte_value;
					break;

				case U16:
					constant_node->constant_type = SHORT_CONST_FORCE_U;
					constant_node->constant_value.unsigned_short_value = constant_node->constant_value.signed_byte_value;
					break;
					
				case I16:
					constant_node->constant_type = SHORT_CONST;
					constant_node->constant_value.signed_short_value = constant_node->constant_value.signed_byte_value;
					break;

				case U32:
					constant_node->constant_type = INT_CONST;
					constant_node->constant_value.unsigned_int_value = constant_node->constant_value.signed_byte_value;
					break;

				case I32:
					constant_node->constant_type = INT_CONST;
					constant_node->constant_value.signed_int_value = constant_node->constant_value.signed_byte_value;
					break;

				case F32:
					constant_node->constant_type = FLOAT_CONST;
					constant_node->constant_value.float_value = constant_node->constant_value.signed_byte_value;
					break;

				case I64:
					constant_node->constant_type = LONG_CONST;
					constant_node->constant_value.signed_long_value = constant_node->constant_value.signed_byte_value;
					break;

				case U64:
					constant_node->constant_type = LONG_CONST_FORCE_U;
					constant_node->constant_value.unsigned_long_value = constant_node->constant_value.signed_byte_value;
					break;

				case F64:
					constant_node->constant_type = DOUBLE_CONST;
					constant_node->constant_value.double_value = constant_node->constant_value.signed_byte_value;
					break;

				default:
					break;
			}

			break;

		case BYTE_CONST_FORCE_U:
			switch(inferred_type->basic_type_token){
				case CHAR:
					constant_node->constant_type = CHAR_CONST;
					constant_node->constant_value.char_value = constant_node->constant_value.unsigned_byte_value;
					break;

				case I8:
					constant_node->constant_type = BYTE_CONST;
					constant_node->constant_value.signed_byte_value = constant_node->constant_value.unsigned_byte_value;
					break;

				case U16:
					constant_node->constant_type = SHORT_CONST_FORCE_U;
					constant_node->constant_value.unsigned_short_value = constant_node->constant_value.unsigned_byte_value;
					break;
					
				case I16:
					constant_node->constant_type = SHORT_CONST;
					constant_node->constant_value.signed_short_value = constant_node->constant_value.unsigned_byte_value;
					break;

				case U32:
					constant_node->constant_type = INT_CONST;
					constant_node->constant_value.unsigned_int_value = constant_node->constant_value.unsigned_byte_value;
					break;

				case I32:
					constant_node->constant_type = INT_CONST;
					constant_node->constant_value.signed_int_value = constant_node->constant_value.unsigned_byte_value;
					break;

				case F32:
					constant_node->constant_type = FLOAT_CONST;
					constant_node->constant_value.float_value = constant_node->constant_value.unsigned_byte_value;
					break;

				case I64:
					constant_node->constant_type = LONG_CONST;
					constant_node->constant_value.signed_long_value = constant_node->constant_value.unsigned_byte_value;
					break;

				case U64:
					constant_node->constant_type = LONG_CONST_FORCE_U;
					constant_node->constant_value.unsigned_long_value = constant_node->constant_value.unsigned_byte_value;
					break;

				case F64:
					constant_node->constant_type = DOUBLE_CONST;
					constant_node->constant_value.double_value = constant_node->constant_value.unsigned_byte_value;
					break;

				default:
					break;
			}

			break;

		case SHORT_CONST:
			switch(inferred_type->basic_type_token){
				case U16:
					constant_node->constant_type = SHORT_CONST_FORCE_U;
					constant_node->constant_value.unsigned_short_value = constant_node->constant_value.signed_short_value;
					break;

				case U32:
					constant_node->constant_type = INT_CONST;
					constant_node->constant_value.unsigned_int_value = constant_node->constant_value.signed_short_value;
					break;

				case I32:
					constant_node->constant_type = INT_CONST;
					constant_node->constant_value.signed_int_value = constant_node->constant_value.signed_short_value;
					break;

				case F32:
					constant_node->constant_type = FLOAT_CONST;
					constant_node->constant_value.float_value = constant_node->constant_value.signed_short_value;
					break;

				case I64:
					constant_node->constant_type = LONG_CONST;
					constant_node->constant_value.signed_long_value = constant_node->constant_value.signed_short_value;
					break;

				case U64:
					constant_node->constant_type = LONG_CONST_FORCE_U;
					constant_node->constant_value.unsigned_long_value = constant_node->constant_value.signed_short_value;
					break;

				case F64:
					constant_node->constant_type = DOUBLE_CONST;
					constant_node->constant_value.double_value = constant_node->constant_value.signed_short_value;
					break;

				default:
					break;
			}
		
			break;

		case SHORT_CONST_FORCE_U:
			switch(inferred_type->basic_type_token){
				case I16:
					constant_node->constant_type = SHORT_CONST;
					constant_node->constant_value.signed_short_value = constant_node->constant_value.unsigned_short_value;
					break;

				case U32:
					constant_node->constant_type = INT_CONST;
					constant_node->constant_value.unsigned_int_value = constant_node->constant_value.unsigned_short_value;
					break;

				case I32:
					constant_node->constant_type = INT_CONST;
					constant_node->constant_value.signed_int_value = constant_node->constant_value.unsigned_short_value;
					break;

				case F32:
					constant_node->constant_type = FLOAT_CONST;
					constant_node->constant_value.float_value = constant_node->constant_value.unsigned_short_value;
					break;

				case I64:
					constant_node->constant_type = LONG_CONST;
					constant_node->constant_value.signed_long_value = constant_node->constant_value.unsigned_short_value;
					break;

				case U64:
					constant_node->constant_type = LONG_CONST_FORCE_U;
					constant_node->constant_value.unsigned_long_value = constant_node->constant_value.unsigned_short_value;
					break;

				case F64:
					constant_node->constant_type = DOUBLE_CONST;
					constant_node->constant_value.double_value = constant_node->constant_value.unsigned_short_value;
					break;

				default:
					break;
			}

			break;

		case INT_CONST_FORCE_U:
			switch(inferred_type->basic_type_token){
				case I32:
					constant_node->constant_type = INT_CONST;
					constant_node->constant_value.signed_int_value = constant_node->constant_value.unsigned_int_value;
					break;

				case F32:
					constant_node->constant_type = FLOAT_CONST;
					constant_node->constant_value.float_value = constant_node->constant_value.unsigned_int_value;
					break;

				case I64:
					constant_node->constant_type = LONG_CONST;
					constant_node->constant_value.signed_long_value = constant_node->constant_value.unsigned_int_value;
					break;

				case U64:
					constant_node->constant_type = LONG_CONST_FORCE_U;
					constant_node->constant_value.unsigned_long_value = constant_node->constant_value.unsigned_int_value;
					break;

				case F64:
					constant_node->constant_type = DOUBLE_CONST;
					constant_node->constant_value.double_value = constant_node->constant_value.unsigned_int_value;
					break;

				default:
					break;
			}

			break;

		case INT_CONST:
			switch(inferred_type->basic_type_token){
				case U32:
					constant_node->constant_type = INT_CONST_FORCE_U;
					constant_node->constant_value.unsigned_int_value = constant_node->constant_value.signed_int_value;
					break;

				case F32:
					constant_node->constant_type = FLOAT_CONST;
					constant_node->constant_value.float_value = constant_node->constant_value.signed_int_value;
					break;

				case I64:
					constant_node->constant_type = LONG_CONST;
					constant_node->constant_value.signed_long_value = constant_node->constant_value.signed_int_value;
					break;

				case U64:
					constant_node->constant_type = LONG_CONST_FORCE_U;
					constant_node->constant_value.unsigned_long_value = constant_node->constant_value.signed_int_value;
					break;

				case F64:
					constant_node->constant_type = DOUBLE_CONST;
					constant_node->constant_value.double_value = constant_node->constant_value.signed_int_value;
					break;

				default:
					break;
			}

			break;

		//Floats can be coerced into being ints(signed/unsigned), longs(signed/unsigned) and double
		case FLOAT_CONST:
			switch(inferred_type->basic_type_token){
				case I32:
					constant_node->constant_type = INT_CONST;
					constant_node->constant_value.signed_int_value = constant_node->constant_value.float_value;
					break;
				case U32:
					constant_node->constant_type = INT_CONST_FORCE_U;
					constant_node->constant_value.unsigned_int_value = constant_node->constant_value.float_value;
					break;

				case I64:
					constant_node->constant_type = LONG_CONST;
					constant_node->constant_value.signed_long_value = constant_node->constant_value.float_value;
					break;

				case U64:
					constant_node->constant_type = LONG_CONST_FORCE_U;
					constant_node->constant_value.unsigned_long_value = constant_node->constant_value.float_value;
					break;

				case F64:
					constant_node->constant_type = DOUBLE_CONST;
					constant_node->constant_value.double_value = constant_node->constant_value.float_value;
					break;

				default:
					break;
			}

			break;

		case LONG_CONST_FORCE_U:
			switch(inferred_type->basic_type_token){
				case I64:
					constant_node->constant_type = LONG_CONST;
					constant_node->constant_value.signed_long_value = constant_node->constant_value.unsigned_long_value;
					break;

				case F64:
					constant_node->constant_type = DOUBLE_CONST;
					constant_node->constant_value.double_value = constant_node->constant_value.unsigned_long_value;
					break;

				default:
					break;
			}

			break;

		case LONG_CONST:
			switch(inferred_type->basic_type_token){
				case U64:
					constant_node->constant_type = LONG_CONST_FORCE_U;
					constant_node->constant_value.unsigned_long_value = constant_node->constant_value.signed_long_value;
					break;

				case F64:
					constant_node->constant_type = DOUBLE_CONST;
					constant_node->constant_value.double_value = constant_node->constant_value.signed_long_value;
					break;

				default:
					break;
			}

			break;


		//Doubles can only ever be coerced to a long due to the 64-bit nature
		case DOUBLE_CONST:
			switch(inferred_type->basic_type_token){
				case U64:
					constant_node->constant_type = LONG_CONST_FORCE_U;
					constant_node->constant_value.unsigned_long_value = constant_node->constant_value.double_value;
					break;
					
				case I64:
					constant_node->constant_type = LONG_CONST;
					constant_node->constant_value.signed_long_value = constant_node->constant_value.double_value;
					break;

				default:
					break;
			}

			break;

		//This should never happen
		default:
			printf("Fatal internal compiler error: Unsupported constant type found in coercer.\n");
			exit(1);
	}
}


/**
 * Is the value of an ast_constant_node 0? Returns true if yes and false
 * if not
 */
u_int8_t is_constant_node_value_0(generic_ast_node_t* constant_node){
	//Switch based on the value here
	switch(constant_node->constant_type){
		case BYTE_CONST:
			return constant_node->constant_value.signed_byte_value == 0 ? TRUE : FALSE;

		case BYTE_CONST_FORCE_U:
			return constant_node->constant_value.unsigned_byte_value == 0 ? TRUE : FALSE;

		case SHORT_CONST:
			return constant_node->constant_value.signed_short_value == 0 ? TRUE : FALSE;
			
		case SHORT_CONST_FORCE_U:
			return constant_node->constant_value.unsigned_short_value == 0 ? TRUE : FALSE;

		case INT_CONST_FORCE_U:
			return constant_node->constant_value.unsigned_int_value == 0 ? TRUE : FALSE;
			
		case INT_CONST:
			return constant_node->constant_value.signed_int_value == 0 ? TRUE : FALSE;

		case LONG_CONST_FORCE_U:
			return constant_node->constant_value.unsigned_long_value == 0 ? TRUE : FALSE;

		case LONG_CONST:
			return constant_node->constant_value.signed_long_value == 0 ? TRUE : FALSE;

		case FLOAT_CONST:
			return constant_node->constant_value.float_value == 0 ? TRUE : FALSE;

		case DOUBLE_CONST:
			return constant_node->constant_value.double_value == 0 ? TRUE : FALSE;

		case CHAR_CONST:
			return constant_node->constant_value.char_value == 0 ? TRUE : FALSE;

		//In normal operation we should never end up here
		default:
			printf("Fatal internal compiler error: Attempt to determine whether a non-nullable constant is 0\n");
			exit(1);
	}
}


/**
 * This helper function negates a constant node's value
 */
void negate_constant_value(generic_ast_node_t* constant_node){
	//Switch based on the value here
	switch(constant_node->constant_type){
		case BYTE_CONST:
			constant_node->constant_value.signed_byte_value *= -1;
			break;
		case BYTE_CONST_FORCE_U:
			constant_node->constant_value.unsigned_byte_value *= -1;
			break;
		case SHORT_CONST:
			constant_node->constant_value.signed_short_value *= -1;
			break;
		case SHORT_CONST_FORCE_U:
			constant_node->constant_value.unsigned_short_value *= -1;
			break;
		case INT_CONST_FORCE_U:
			constant_node->constant_value.unsigned_int_value *= -1;
			break;
		case INT_CONST:
			constant_node->constant_value.signed_int_value *= -1;
			break;
		case LONG_CONST_FORCE_U:
			constant_node->constant_value.unsigned_long_value *= -1;
			break;
		case LONG_CONST:
			constant_node->constant_value.signed_long_value *= -1;
			break;
		case FLOAT_CONST:
			constant_node->constant_value.float_value *= -1;
			break;
		case DOUBLE_CONST:
			constant_node->constant_value.double_value *= -1;
			break;
		case CHAR_CONST:
			constant_node->constant_value.char_value *= -1;
			break;
		//This should never happen
		default:
			return;
	}
}


/**
 * This helper function decrements a constant node's value
 */
void decrement_constant_value(generic_ast_node_t* constant_node){
	//Switch based on the value here
	switch(constant_node->constant_type){
		case BYTE_CONST:
			constant_node->constant_value.signed_byte_value--;
			break;
		case BYTE_CONST_FORCE_U:
			constant_node->constant_value.unsigned_byte_value--;
			break;
		case SHORT_CONST:
			constant_node->constant_value.signed_short_value--;
			break;
		case SHORT_CONST_FORCE_U:
			constant_node->constant_value.unsigned_short_value--;
			break;
		case INT_CONST_FORCE_U:
			constant_node->constant_value.unsigned_int_value--;
			break;
		case INT_CONST:
			constant_node->constant_value.signed_int_value--;
			break;
		case LONG_CONST_FORCE_U:
			constant_node->constant_value.unsigned_long_value--;
			break;
		case LONG_CONST:
			constant_node->constant_value.signed_long_value--;
			break;
		case FLOAT_CONST:
			constant_node->constant_value.float_value--;
			break;
		case DOUBLE_CONST:
			constant_node->constant_value.double_value--;
			break;
		case CHAR_CONST:
			constant_node->constant_value.char_value--;
			break;
		//This should never happen
		default:
			return;
	}
}


/**
 * This helper function increments a constant node's value
 */
void increment_constant_value(generic_ast_node_t* constant_node){
	//Switch based on the value here
	switch(constant_node->constant_type){
		case BYTE_CONST:
			constant_node->constant_value.signed_byte_value++;
			break;
		case BYTE_CONST_FORCE_U:
			constant_node->constant_value.unsigned_byte_value++;
			break;
		case SHORT_CONST:
			constant_node->constant_value.signed_short_value++;
			break;
		case SHORT_CONST_FORCE_U:
			constant_node->constant_value.unsigned_short_value++;
			break;
		case INT_CONST_FORCE_U:
			constant_node->constant_value.unsigned_int_value++;
			break;
		case INT_CONST:
			constant_node->constant_value.signed_int_value++;
			break;
		case LONG_CONST_FORCE_U:
			constant_node->constant_value.unsigned_long_value++;
			break;
		case LONG_CONST:
			constant_node->constant_value.signed_long_value++;
			break;
		case FLOAT_CONST:
			constant_node->constant_value.float_value++;
			break;
		case DOUBLE_CONST:
			constant_node->constant_value.double_value++;
			break;
		case CHAR_CONST:
			constant_node->constant_value.char_value++;
			break;
		//This should never happen
		default:
			return;
	}
}


/**
 * This helper function will logically not a consant node's value
 */
void logical_not_constant_value(generic_ast_node_t* constant_node){
	//Switch based on the value here
	switch(constant_node->constant_type){
		case BYTE_CONST:
			constant_node->constant_value.signed_byte_value = !(constant_node->constant_value.signed_byte_value);
			break;
		case BYTE_CONST_FORCE_U:
			break;
		case SHORT_CONST:
			constant_node->constant_value.signed_byte_value = !(constant_node->constant_value.signed_byte_value);
			break;
		case SHORT_CONST_FORCE_U:
			constant_node->constant_value.unsigned_short_value = !(constant_node->constant_value.unsigned_short_value);
			break;
		case INT_CONST_FORCE_U:
			constant_node->constant_value.unsigned_int_value = !(constant_node->constant_value.unsigned_int_value);
			break;
		case INT_CONST:
			constant_node->constant_value.signed_int_value = !(constant_node->constant_value.signed_int_value);
			break;
		case LONG_CONST_FORCE_U:
			constant_node->constant_value.unsigned_long_value = !(constant_node->constant_value.unsigned_long_value);
			break;
		case FLOAT_CONST:
			constant_node->constant_value.float_value = !(constant_node->constant_value.float_value);
			break;
		case DOUBLE_CONST:
			constant_node->constant_value.double_value = !(constant_node->constant_value.double_value);
			break;
		case LONG_CONST:
			constant_node->constant_value.signed_long_value = !(constant_node->constant_value.signed_long_value);
			break;
		case CHAR_CONST:
			constant_node->constant_value.char_value = !(constant_node->constant_value.char_value);
			break;
		//This should never happen
		default:
			return;
	}
}


/**
 * This helper function will logically not a consant node's value
 */
void bitwise_not_constant_value(generic_ast_node_t* constant_node){
	//Switch based on the value here
	switch(constant_node->constant_type){
		case BYTE_CONST:
			constant_node->constant_value.signed_byte_value = ~(constant_node->constant_value.signed_byte_value);
			break;
		case BYTE_CONST_FORCE_U:
			constant_node->constant_value.unsigned_byte_value = ~(constant_node->constant_value.unsigned_byte_value);
			break;
		case SHORT_CONST:
			constant_node->constant_value.signed_short_value = ~(constant_node->constant_value.signed_short_value);
			break;
		case SHORT_CONST_FORCE_U:
			constant_node->constant_value.unsigned_short_value = ~(constant_node->constant_value.unsigned_short_value);
			break;
		case INT_CONST_FORCE_U:
			constant_node->constant_value.unsigned_int_value = ~(constant_node->constant_value.unsigned_int_value);
			break;
		case INT_CONST:
			constant_node->constant_value.signed_int_value = ~(constant_node->constant_value.signed_int_value);
			break;
		case LONG_CONST_FORCE_U:
			constant_node->constant_value.unsigned_long_value = ~(constant_node->constant_value.unsigned_long_value);
			break;
		case LONG_CONST:
			constant_node->constant_value.signed_long_value = ~(constant_node->constant_value.signed_long_value);
			break;
		case CHAR_CONST:
			constant_node->constant_value.char_value = ~(constant_node->constant_value.char_value);
			break;
		//This should never happen
		default:
			return;
	}
}


/**
 * Emit the product of two given constants. The result will overwrite the first constant given
 *
 * The result will be: constant1 = constant1 * constant2
 */
void multiply_constant_nodes(generic_ast_node_t* constant_node1, generic_ast_node_t* constant_node2){
	switch(constant_node1->constant_type){
		case CHAR_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.char_value *= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.char_value *= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.char_value *= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.char_value *= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.char_value *= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.char_value *= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.char_value *= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.char_value *= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.char_value *= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.char_value *= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.char_value *= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_byte_value *= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value *= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_byte_value *= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_byte_value *= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value *= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_byte_value *= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_byte_value *= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value *= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_byte_value *= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value *= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_byte_value *= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_byte_value *= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value *= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_byte_value *= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_byte_value *= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value *= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_byte_value *= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_byte_value *= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value *= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_byte_value *= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value *= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_byte_value *= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_short_value *= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value *= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_short_value *= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_short_value *= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value *= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_short_value *= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_short_value *= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value *= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_short_value *= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value *= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_short_value *= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_short_value *= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value *= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_short_value *= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_short_value *= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value *= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_short_value *= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_short_value *= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value *= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_short_value *= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value *= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_short_value *= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
					exit(1);
			}

			break;

		case INT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_int_value *= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value *= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_int_value *= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_int_value *= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value *= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_int_value *= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_int_value *= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value *= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_int_value *= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value *= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_int_value *= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
					exit(1);
			}

			break;

		case INT_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_int_value *= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value *= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_int_value *= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_int_value *= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value *= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_int_value *= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_int_value *= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value *= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_int_value *= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value *= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_int_value *= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
					exit(1);
			}

			break;

		case FLOAT_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.float_value *= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.float_value *= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.float_value *= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.float_value *= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.float_value *= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.float_value *= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.float_value *= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.float_value *= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.float_value *= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.float_value *= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.float_value *= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
					exit(1);
			}

			break;

		case LONG_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_long_value *= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value *= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_long_value *= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_long_value *= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value *= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_long_value *= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_long_value *= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value *= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_long_value *= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value *= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_long_value *= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
					exit(1);
			}
			
			break;

		case LONG_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_long_value *= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value *= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_long_value *= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_long_value *= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value *= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_long_value *= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_long_value *= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value *= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_long_value *= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value *= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_long_value *= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
					exit(1);
			}

			break;

		case DOUBLE_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.double_value *= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.double_value *= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.double_value *= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.double_value *= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.double_value *= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.double_value *= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.double_value *= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.double_value *= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.double_value *= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.double_value *= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.double_value *= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
					exit(1);
			}

			break;

		default:
			printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
			exit(1);
	}
}


/**
 * Emit the quotient of two given constants. The result will overwrite the first constant given
 *
 * The result will be: constant1 = constant1 / constant2
 *
 * NOTE: We guarantee that the value of constant_node2 will *not* be 0 when we get here, so we will
 * *not* do any div by zero checks here. That is the responsibility of the caller
 */
void divide_constant_nodes(generic_ast_node_t* constant_node1, generic_ast_node_t* constant_node2){
	switch(constant_node1->constant_type){
		case CHAR_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.char_value /= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.char_value /= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.char_value /= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.char_value /= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.char_value /= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.char_value /= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.char_value /= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.char_value /= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.char_value /= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.char_value /= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.char_value /= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant division operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_byte_value /= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value /= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_byte_value /= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_byte_value /= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value /= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_byte_value /= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_byte_value /= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value /= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_byte_value /= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value /= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_byte_value /= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant division operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_byte_value /= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value /= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_byte_value /= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_byte_value /= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value /= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_byte_value /= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_byte_value /= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value /= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_byte_value /= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value /= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_byte_value /= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant division operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_short_value /= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value /= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_short_value /= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_short_value /= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value /= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_short_value /= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_short_value /= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value /= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_short_value /= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value /= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_short_value /= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant division operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_short_value /= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value /= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_short_value /= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_short_value /= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value /= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_short_value /= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_short_value /= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value /= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_short_value /= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value /= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_short_value /= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant division operation\n");
					exit(1);
			}

			break;

		case INT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_int_value /= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value /= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_int_value /= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_int_value /= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value /= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_int_value /= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_int_value /= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value /= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_int_value /= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value /= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_int_value /= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant division operation\n");
					exit(1);
			}

			break;

		case INT_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_int_value /= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value /= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_int_value /= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_int_value /= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value /= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_int_value /= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_int_value /= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value /= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_int_value /= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value /= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_int_value /= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant division operation\n");
					exit(1);
			}

			break;

		case FLOAT_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.float_value /= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.float_value /= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.float_value /= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.float_value /= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.float_value /= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.float_value /= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.float_value /= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.float_value /= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.float_value /= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.float_value /= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.float_value /= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant division operation\n");
					exit(1);
			}

			break;

		case LONG_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_long_value /= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value /= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_long_value /= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_long_value /= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value /= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_long_value /= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_long_value /= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value /= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_long_value /= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value /= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_long_value /= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant division operation\n");
					exit(1);
			}
			
			break;

		case LONG_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_long_value /= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value /= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_long_value /= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_long_value /= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value /= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_long_value /= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_long_value /= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value /= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_long_value /= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value /= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_long_value /= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant division operation\n");
					exit(1);
			}

			break;

		case DOUBLE_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.double_value /= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.double_value /= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.double_value /= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.double_value /= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.double_value /= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.double_value /= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.double_value /= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.double_value /= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.double_value /= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.double_value /= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.double_value /= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant division operation\n");
					exit(1);
			}

			break;

		default:
			printf("Fatal internal compiler error: Unsupported constant division operation\n");
			exit(1);
	}
}


/**
 * Emit the modulo of two given constants. The result will overwrite the first constant given
 *
 * The result will be: constant1 = constant1 % constant2
 *
 * NOTE: We guarantee that the value of constant_node2 will *not* be 0 when we get here, so we will
 * *not* do any mod by zero checks here. That is the responsibility of the caller
 */
void mod_constant_nodes(generic_ast_node_t* constant_node1, generic_ast_node_t* constant_node2){
	switch(constant_node1->constant_type){
		case CHAR_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.char_value %= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.char_value %= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.char_value %= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.char_value %= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.char_value %= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.char_value %= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.char_value %= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.char_value %= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.char_value %= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant modulus operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value %= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_byte_value %= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value %= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_byte_value %= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_byte_value %= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value %= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_byte_value %= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value %= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_byte_value %= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant modulus operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value %= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_byte_value %= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value %= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_byte_value %= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_byte_value %= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value %= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_byte_value %= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value %= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_byte_value %= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant modulus operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value %= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_short_value %= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value %= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_short_value %= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_short_value %= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value %= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_short_value %= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value %= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_short_value %= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant modulus operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value %= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_short_value %= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value %= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_short_value %= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_short_value %= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value %= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_short_value %= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value %= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_short_value %= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant modulus operation\n");
					exit(1);
			}

			break;

		case INT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value %= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_int_value %= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value %= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_int_value %= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_int_value %= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value %= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_int_value %= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value %= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_int_value %= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant modulus operation\n");
					exit(1);
			}

			break;

		case INT_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value %= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_int_value %= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value %= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_int_value %= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_int_value %= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value %= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_int_value %= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value %= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_int_value %= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant modulus operation\n");
					exit(1);
			}

			break;

		case LONG_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value %= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_long_value %= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value %= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_long_value %= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_long_value %= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value %= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_long_value %= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value %= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_long_value %= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant modulus operation\n");
					exit(1);
			}
			
			break;

		case LONG_CONST:
			//Now go based on the second one's type
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value %= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_long_value %= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value %= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_long_value %= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_long_value %= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value %= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_long_value %= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value %= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_long_value %= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant modulus operation\n");
					exit(1);
			}

			break;

		default:
			printf("Fatal internal compiler error: Unsupported constant modulus operation\n");
			exit(1);
	}
}


/**
 * Emit the sum of two given constants. The result will overwrite the first constant given
 *
 * The result will be: constant1 = constant1 + constant2
 */
void add_constant_nodes(generic_ast_node_t* constant_node1, generic_ast_node_t* constant_node2){
	switch(constant_node1->constant_type){
		case CHAR_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.char_value += constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.char_value += constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.char_value += constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.char_value += constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.char_value += constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.char_value += constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.char_value += constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.char_value += constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.char_value += constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.char_value += constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.char_value += constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant addition operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_byte_value += constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value += constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_byte_value += constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_byte_value += constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value += constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_byte_value += constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_byte_value += constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value += constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_byte_value += constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value += constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_byte_value += constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant addition operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_byte_value += constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value += constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_byte_value += constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_byte_value += constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value += constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_byte_value += constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_byte_value += constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value += constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_byte_value += constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value += constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_byte_value += constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant addition operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_short_value += constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value += constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_short_value += constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_short_value += constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value += constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_short_value += constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_short_value += constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value += constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_short_value += constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value += constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_short_value += constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant addition operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_short_value += constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value += constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_short_value += constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_short_value += constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value += constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_short_value += constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_short_value += constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value += constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_short_value += constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value += constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_short_value += constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant addition operation\n");
					exit(1);
			}

			break;

		case INT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_int_value += constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value += constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_int_value += constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_int_value += constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value += constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_int_value += constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_int_value += constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value += constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_int_value += constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value += constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_int_value += constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant addition operation\n");
					exit(1);
			}

			break;

		case INT_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_int_value += constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value += constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_int_value += constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_int_value += constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value += constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_int_value += constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_int_value += constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value += constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_int_value += constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value += constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_int_value += constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant addition operation\n");
					exit(1);
			}

			break;

		case FLOAT_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.float_value += constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.float_value += constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.float_value += constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.float_value += constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.float_value += constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.float_value += constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.float_value += constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.float_value += constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.float_value += constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.float_value += constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.float_value += constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant addition operation\n");
					exit(1);
			}

			break;

		case LONG_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_long_value += constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value += constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_long_value += constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_long_value += constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value += constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_long_value += constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_long_value += constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value += constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_long_value += constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value += constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_long_value += constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant addition operation\n");
					exit(1);
			}
			
			break;

		case LONG_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_long_value += constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value += constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_long_value += constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_long_value += constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value += constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_long_value += constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_long_value += constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value += constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_long_value += constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value += constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_long_value += constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant addition operation\n");
					exit(1);
			}

			break;

		case DOUBLE_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.double_value += constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.double_value += constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.double_value += constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.double_value += constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.double_value += constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.double_value += constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.double_value += constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.double_value += constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.double_value += constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.double_value += constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.double_value += constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant addition operation\n");
					exit(1);
			}

			break;

		default:
			printf("Fatal internal compiler error: Unsupported constant addition operation\n");
			exit(1);
	}
}


/**
 * Emit the difference of two given constants. The result will overwrite the first constant given
 *
 * The result will be: constant1 = constant1 - constant2
 */
void subtract_constant_nodes(generic_ast_node_t* constant_node1, generic_ast_node_t* constant_node2){
	switch(constant_node1->constant_type){
		case CHAR_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.char_value -= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.char_value -= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.char_value -= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.char_value -= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.char_value -= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.char_value -= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.char_value -= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.char_value -= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.char_value -= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.char_value -= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.char_value -= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant subtraction operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_byte_value -= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value -= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_byte_value -= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_byte_value -= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value -= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_byte_value -= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_byte_value -= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value -= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_byte_value -= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value -= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_byte_value -= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant subtraction operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_byte_value -= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value -= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_byte_value -= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_byte_value -= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value -= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_byte_value -= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_byte_value -= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value -= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_byte_value -= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value -= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_byte_value -= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant subtraction operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_short_value -= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value -= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_short_value -= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_short_value -= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value -= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_short_value -= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_short_value -= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value -= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_short_value -= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value -= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_short_value -= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant subtraction operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_short_value -= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value -= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_short_value -= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_short_value -= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value -= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_short_value -= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_short_value -= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value -= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_short_value -= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value -= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_short_value -= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant subtraction operation\n");
					exit(1);
			}

			break;

		case INT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_int_value -= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value -= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_int_value -= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_int_value -= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value -= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_int_value -= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_int_value -= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value -= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_int_value -= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value -= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_int_value -= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant subtraction operation\n");
					exit(1);
			}

			break;

		case INT_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_int_value -= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value -= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_int_value -= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_int_value -= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value -= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_int_value -= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_int_value -= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value -= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_int_value -= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value -= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_int_value -= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant subtraction operation\n");
					exit(1);
			}

			break;

		case FLOAT_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.float_value -= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.float_value -= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.float_value -= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.float_value -= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.float_value -= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.float_value -= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.float_value -= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.float_value -= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.float_value -= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.float_value -= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.float_value -= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant subtraction operation\n");
					exit(1);
			}

			break;

		case LONG_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_long_value -= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value -= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_long_value -= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_long_value -= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value -= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_long_value -= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_long_value -= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value -= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_long_value -= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value -= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_long_value -= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant subtraction operation\n");
					exit(1);
			}
			
			break;

		case LONG_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_long_value -= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value -= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_long_value -= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_long_value -= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value -= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_long_value -= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_long_value -= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value -= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_long_value -= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value -= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_long_value -= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant subtraction operation\n");
					exit(1);
			}

			break;

		case DOUBLE_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.double_value -= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.double_value -= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.double_value -= constant_node2->constant_value.signed_long_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.double_value -= constant_node2->constant_value.float_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.double_value -= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.double_value -= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.double_value -= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.double_value -= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.double_value -= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.double_value -= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.double_value -= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant subtraction operation\n");
					exit(1);
			}

			break;

		default:
			printf("Fatal internal compiler error: Unsupported constant subtraction operation\n");
			exit(1);
	}
}


/**
 * Emit the right shift of two given constants. The result will overwrite the first constant given
 *
 * The result will be: constant1 = constant1 >> constant2
 */
void right_shift_constant_nodes(generic_ast_node_t* constant_node1, generic_ast_node_t* constant_node2){
	switch(constant_node1->constant_type){
		case CHAR_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.char_value >>= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.char_value >>= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.char_value >>= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.char_value >>= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.char_value >>= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.char_value >>= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.char_value >>= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.char_value >>= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.char_value >>= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant right shift operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value >>= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_byte_value >>= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value >>= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_byte_value >>= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_byte_value >>= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value >>= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_byte_value >>= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value >>= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_byte_value >>= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant right shift operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value >>= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_byte_value >>= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value >>= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_byte_value >>= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_byte_value >>= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value >>= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_byte_value >>= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value >>= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_byte_value >>= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant right shift operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value >>= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_short_value >>= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value >>= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_short_value >>= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_short_value >>= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value >>= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_short_value >>= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value >>= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_short_value >>= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant right shift operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value >>= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_short_value >>= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value >>= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_short_value >>= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_short_value >>= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value >>= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_short_value >>= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value >>= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_short_value >>= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant right shift operation\n");
					exit(1);
			}

			break;

		case INT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value >>= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_int_value >>= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value >>= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_int_value >>= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_int_value >>= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value >>= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_int_value >>= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value >>= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_int_value >>= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant right shift operation\n");
					exit(1);
			}

			break;

		case INT_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value >>= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_int_value >>= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value >>= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_int_value >>= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_int_value >>= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value >>= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_int_value >>= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value >>= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_int_value >>= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant right shift operation\n");
					exit(1);
			}

			break;

		case LONG_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value >>= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_long_value >>= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value >>= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_long_value >>= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_long_value >>= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value >>= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_long_value >>= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value >>= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_long_value >>= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant right shift operation\n");
					exit(1);
			}
			
			break;

		case LONG_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value >>= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_long_value >>= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value >>= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_long_value >>= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_long_value >>= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value >>= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_long_value >>= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value >>= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_long_value >>= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant right shift operation\n");
					exit(1);
			}

			break;

		default:
			printf("Fatal internal compiler error: Unsupported constant right shift operation\n");
			exit(1);
	}
}


/**
 * Emit the left shift of two given constants. The result will overwrite the first constant given
 *
 * The result will be: constant1 = constant1 << constant2
 */
void left_shift_constant_nodes(generic_ast_node_t* constant_node1, generic_ast_node_t* constant_node2){
	switch(constant_node1->constant_type){
		case CHAR_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.char_value <<= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.char_value <<= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.char_value <<= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.char_value <<= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.char_value <<= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.char_value <<= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.char_value <<= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.char_value <<= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.char_value <<= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant left shift operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value <<= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_byte_value <<= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value <<= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_byte_value <<= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_byte_value <<= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value <<= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_byte_value <<= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value <<= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_byte_value <<= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant left shift operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value <<= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_byte_value <<= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value <<= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_byte_value <<= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_byte_value <<= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value <<= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_byte_value <<= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value <<= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_byte_value <<= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant left shift operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value <<= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_short_value <<= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value <<= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_short_value <<= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_short_value <<= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value <<= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_short_value <<= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value <<= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_short_value <<= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant left shift operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value <<= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_short_value <<= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value <<= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_short_value <<= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_short_value <<= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value <<= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_short_value <<= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value <<= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_short_value <<= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant left shift operation\n");
					exit(1);
			}

			break;

		case INT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value <<= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_int_value <<= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value <<= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_int_value <<= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_int_value <<= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value <<= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_int_value <<= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value <<= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_int_value <<= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant left shift operation\n");
					exit(1);
			}

			break;

		case INT_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value <<= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_int_value <<= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value <<= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_int_value <<= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_int_value <<= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value <<= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_int_value <<= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value <<= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_int_value <<= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant left shift operation\n");
					exit(1);
			}

			break;

		case LONG_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value <<= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_long_value <<= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value <<= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_long_value <<= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_long_value <<= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value <<= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_long_value <<= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value <<= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_long_value <<= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant left shift operation\n");
					exit(1);
			}
			
			break;

		case LONG_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value <<= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_long_value <<= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value <<= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_long_value <<= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_long_value <<= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value <<= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_long_value <<= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value <<= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_long_value <<= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant left shift operation\n");
					exit(1);
			}

			break;

		default:
			printf("Fatal internal compiler error: Unsupported constant left shift operation\n");
			exit(1);
	}
}


/**
 * Emit the bitwise or of two given constants. The result will overwrite the first constant given
 *
 * The result will be: constant1 = constant1 | constant2
 */
void bitwise_or_constant_nodes(generic_ast_node_t* constant_node1, generic_ast_node_t* constant_node2){
	switch(constant_node1->constant_type){
		case CHAR_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.char_value |= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.char_value |= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.char_value |= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.char_value |= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.char_value |= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.char_value |= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.char_value |= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.char_value |= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.char_value |= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant bitwise or operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value |= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_byte_value |= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value |= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_byte_value |= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_byte_value |= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value |= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_byte_value |= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value |= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_byte_value |= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant bitwise or operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value |= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_byte_value |= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value |= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_byte_value |= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_byte_value |= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value |= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_byte_value |= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value |= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_byte_value |= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant bitwise or operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value |= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_short_value |= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value |= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_short_value |= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_short_value |= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value |= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_short_value |= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value |= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_short_value |= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant bitwise or operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value |= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_short_value |= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value |= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_short_value |= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_short_value |= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value |= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_short_value |= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value |= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_short_value |= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant bitwise or operation\n");
					exit(1);
			}

			break;

		case INT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value |= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_int_value |= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value |= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_int_value |= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_int_value |= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value |= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_int_value |= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value |= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_int_value |= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant bitwise or operation\n");
					exit(1);
			}

			break;

		case INT_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value |= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_int_value |= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value |= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_int_value |= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_int_value |= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value |= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_int_value |= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value |= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_int_value |= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant bitwise or operation\n");
					exit(1);
			}

			break;

		case LONG_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value |= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_long_value |= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value |= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_long_value |= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_long_value |= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value |= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_long_value |= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value |= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_long_value |= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant bitwise or operation\n");
					exit(1);
			}
			
			break;

		case LONG_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value |= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_long_value |= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value |= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_long_value |= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_long_value |= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value |= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_long_value |= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value |= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_long_value |= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant bitwise or operation\n");
					exit(1);
			}

			break;

		default:
			printf("Fatal internal compiler error: Unsupported constant bitwise or operation\n");
			exit(1);
	}
}


/**
 * Emit the bitwise exclusive or of two given constants. The result will overwrite the first constant given
 *
 * The result will be: constant1 = constant1 ^ constant2
 */
void bitwise_exclusive_or_constant_nodes(generic_ast_node_t* constant_node1, generic_ast_node_t* constant_node2){
	switch(constant_node1->constant_type){
		case CHAR_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.char_value ^= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.char_value ^= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.char_value ^= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.char_value ^= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.char_value ^= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.char_value ^= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.char_value ^= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.char_value ^= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.char_value ^= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant bitwise xor operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value ^= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_byte_value ^= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value ^= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_byte_value ^= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_byte_value ^= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value ^= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_byte_value ^= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value ^= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_byte_value ^= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant bitwise xor operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value ^= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_byte_value ^= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value ^= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_byte_value ^= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_byte_value ^= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value ^= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_byte_value ^= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value ^= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_byte_value ^= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant bitwise xor operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value ^= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_short_value ^= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value ^= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_short_value ^= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_short_value ^= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value ^= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_short_value ^= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value ^= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_short_value ^= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant bitwise xor operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value ^= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_short_value ^= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value ^= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_short_value ^= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_short_value ^= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value ^= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_short_value ^= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value ^= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_short_value ^= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant bitwise xor operation\n");
					exit(1);
			}

			break;

		case INT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value ^= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_int_value ^= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value ^= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_int_value ^= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_int_value ^= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value ^= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_int_value ^= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value ^= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_int_value ^= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant bitwise xor operation\n");
					exit(1);
			}

			break;

		case INT_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value ^= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_int_value ^= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value ^= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_int_value ^= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_int_value ^= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value ^= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_int_value ^= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value ^= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_int_value ^= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant bitwise xor operation\n");
					exit(1);
			}

			break;

		case LONG_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value ^= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_long_value ^= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value ^= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_long_value ^= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_long_value ^= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value ^= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_long_value ^= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value ^= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_long_value ^= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant bitwise xor operation\n");
					exit(1);
			}
			
			break;

		case LONG_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value ^= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_long_value ^= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value ^= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_long_value ^= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_long_value ^= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value ^= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_long_value ^= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value ^= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_long_value ^= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant bitwise xor operation\n");
					exit(1);
			}

			break;

		default:
			printf("Fatal internal compiler error: Unsupported constant bitwise xor operation\n");
			exit(1);
	}
}


/**
 * Emit the bitwise and of two given constants. The result will overwrite the first constant given
 *
 * The result will be: constant1 = constant1 & constant2
 */
void bitwise_and_constant_nodes(generic_ast_node_t* constant_node1, generic_ast_node_t* constant_node2){
	switch(constant_node1->constant_type){
		case CHAR_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.char_value &= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.char_value &= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.char_value &= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.char_value &= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.char_value &= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.char_value &= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.char_value &= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.char_value &= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.char_value &= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant bitwise and operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value &= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_byte_value &= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value &= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_byte_value &= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_byte_value &= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value &= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_byte_value &= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value &= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_byte_value &= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant bitwise and operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value &= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_byte_value &= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value &= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_byte_value &= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_byte_value &= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value &= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_byte_value &= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value &= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_byte_value &= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant bitwise and operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value &= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_short_value &= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value &= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_short_value &= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_short_value &= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value &= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_short_value &= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value &= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_short_value &= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant bitwise and operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value &= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_short_value &= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value &= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_short_value &= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_short_value &= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value &= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_short_value &= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value &= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_short_value &= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant bitwise and operation\n");
					exit(1);
			}

			break;

		case INT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value &= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_int_value &= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value &= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_int_value &= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_int_value &= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value &= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_int_value &= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value &= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_int_value &= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant bitwise and operation\n");
					exit(1);
			}

			break;

		case INT_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value &= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_int_value &= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value &= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_int_value &= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_int_value &= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value &= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_int_value &= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value &= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_int_value &= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant bitwise and operation\n");
					exit(1);
			}

			break;

		case LONG_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value &= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_long_value &= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value &= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_long_value &= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_long_value &= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value &= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_long_value &= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value &= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_long_value &= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant bitwise and operation\n");
					exit(1);
			}
			
			break;

		case LONG_CONST:
			switch(constant_node2->constant_type){
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value &= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_long_value &= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value &= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_long_value &= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_long_value &= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value &= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_long_value &= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value &= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_long_value &= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant bitwise and operation\n");
					exit(1);
			}

			break;

		default:
			printf("Fatal internal compiler error: Unsupported constant bitwise and operation\n");
			exit(1);
	}
}


/**
 * Emit the != comparison of two given constant nodes. The result will be stored in the first argument
 *
 * The result will be: constant1 = constant1 != constant2
 *
 * NOTE: Whenever casting has to happen here, remember that unsigned always wins in the signedness department
 */
void not_equals_constant_nodes(generic_ast_node_t* constant_node1, generic_ast_node_t* constant_node2){
	switch(constant_node1->constant_type){
		case INT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value != constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value != constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value != (u_int64_t)(constant_node2->constant_value.signed_long_value);
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value != constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value != constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value != (u_int32_t)(constant_node2->constant_value.signed_int_value);
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value != (u_int16_t)(constant_node2->constant_value.signed_short_value);
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value != constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value != (u_int8_t)(constant_node2->constant_value.signed_byte_value);
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value != constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value != (u_int8_t)(constant_node2->constant_value.char_value);
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant != operation\n");
					exit(1);
			}

			break;

		case INT_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value != constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value = (u_int64_t)(constant_node1->constant_value.signed_int_value) != constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value != constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.signed_int_value = (u_int32_t)(constant_node1->constant_value.signed_int_value) != constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value != constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value != constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value != constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value = (u_int32_t)(constant_node1->constant_value.signed_int_value) != constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value != constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value = (u_int32_t)(constant_node1->constant_value.signed_int_value) != constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value != constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant != operation\n");
					exit(1);
			}

			break;

		case FLOAT_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value != constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value != constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.float_value = constant_node1->constant_value.float_value != constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.float_value = constant_node1->constant_value.float_value != constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value != constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value != constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value != constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value != constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value != constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value != constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value != constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant != operation\n");
					exit(1);
			}

			break;

		case LONG_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value != constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value != constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value != (u_int64_t)(constant_node2->constant_value.signed_long_value);
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value != constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value != constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value != (u_int32_t)(constant_node2->constant_value.signed_int_value);
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value != (u_int16_t)(constant_node2->constant_value.signed_short_value);
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value != constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value != (u_int16_t)(constant_node2->constant_value.signed_byte_value);
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value != constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value != (u_int8_t)(constant_node2->constant_value.char_value);
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant != operation\n");
					exit(1);
			}
			
			break;

		case LONG_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value != constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value = (u_int64_t)(constant_node1->constant_value.signed_long_value) != constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value != constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.signed_long_value = (u_int64_t)(constant_node1->constant_value.signed_long_value) != constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value != constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value != constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value != constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value = (u_int64_t)(constant_node1->constant_value.signed_long_value) != constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value != constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value = (u_int64_t)(constant_node1->constant_value.signed_long_value) != constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value != constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant != operation\n");
					exit(1);
			}

			break;

		case DOUBLE_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value != constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value != constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.double_value = constant_node1->constant_value.double_value != constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.double_value = constant_node1->constant_value.double_value != constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value != constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value != constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value != constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value != constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value != constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.double_value	= constant_node1->constant_value.double_value != constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value != constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant != operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value != constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value = (u_int8_t)(constant_node1->constant_value.signed_byte_value) != constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value != constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.signed_byte_value = (u_int8_t)(constant_node1->constant_value.signed_byte_value) != constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value != constant_node2->constant_value.signed_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value != constant_node2->constant_value.float_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value != constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value = (u_int8_t)(constant_node1->constant_value.signed_byte_value) != constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value != constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value = (u_int8_t)(constant_node1->constant_value.signed_byte_value) != constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value != constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant != operation\n");
					exit(1);
			}
			break;

		case BYTE_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value != constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value != constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value != (u_int64_t)(constant_node2->constant_value.signed_long_value);
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value != constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value != (u_int32_t)(constant_node2->constant_value.signed_int_value);
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value != constant_node2->constant_value.float_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value != (u_int16_t)(constant_node2->constant_value.signed_short_value);
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value != constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value != (u_int8_t)(constant_node2->constant_value.signed_byte_value);
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value != constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value != (u_int8_t)(constant_node2->constant_value.char_value);
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant != operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value != constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value = (u_int16_t)(constant_node1->constant_value.signed_short_value) != constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value != constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.signed_short_value = (u_int16_t)(constant_node1->constant_value.signed_short_value) != constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value != constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value != constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value != constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value = (u_int16_t)(constant_node1->constant_value.signed_short_value) != constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value != constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value = (u_int16_t)(constant_node1->constant_value.signed_short_value) != constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value != constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant != operation\n");
					exit(1);
			}

		case SHORT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value != constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value != constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value != (u_int64_t)(constant_node2->constant_value.signed_long_value);
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value != constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value != constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value != (u_int32_t)(constant_node2->constant_value.signed_int_value);
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value != (u_int32_t)(constant_node2->constant_value.signed_short_value);
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value != constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value != (u_int8_t)(constant_node2->constant_value.signed_byte_value);
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value != constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value != (u_int8_t)(constant_node2->constant_value.char_value);
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant != operation\n");
					exit(1);
			}


		case CHAR_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value != constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.char_value = (u_int8_t)(constant_node1->constant_value.char_value) != constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.char_value = constant_node1->constant_value.char_value != constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.char_value = (u_int8_t)(constant_node1->constant_value.char_value) != constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value != constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value != constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value != constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.char_value = (u_int8_t)(constant_node1->constant_value.char_value) != constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value != constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.char_value = (u_int8_t)(constant_node1->constant_value.char_value) != constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value != constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant != operation\n");
					exit(1);
			}

			break;

		default:
			printf("Fatal internal compiler error: Unsupported constant != operation\n");
			exit(1);
	}
}


/**
 * Emit the == comparison of two given constant nodes. The result will be stored in the first argument
 *
 * The result will be: constant1 = constant1 == constant2
 *
 * NOTE: when we have unsigned compared with signed, remember that the unsigned one always wins out in the type converter
 */
void equals_constant_nodes(generic_ast_node_t* constant_node1, generic_ast_node_t* constant_node2){
	switch(constant_node1->constant_type){
		case INT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value == constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value == constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value == (u_int64_t)(constant_node2->constant_value.signed_long_value);
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value == constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value == constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value == (u_int32_t)(constant_node2->constant_value.signed_int_value);
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value == (u_int16_t)(constant_node2->constant_value.signed_short_value);
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value == constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value == (u_int8_t)(constant_node2->constant_value.signed_byte_value);
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value == constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value == (u_int8_t)(constant_node2->constant_value.char_value);
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant == operation\n");
					exit(1);
			}

			break;

		case INT_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value == constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value = (u_int64_t)(constant_node1->constant_value.signed_int_value) == constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value == constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.signed_int_value = (u_int32_t)(constant_node1->constant_value.signed_int_value) == constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value == constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value == constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value == constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value = (u_int32_t)(constant_node1->constant_value.signed_int_value) == constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value == constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value = (u_int32_t)(constant_node1->constant_value.signed_int_value) == constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value == constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant == operation\n");
					exit(1);
			}

			break;

		case FLOAT_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value == constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value == constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.float_value = constant_node1->constant_value.float_value == constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.float_value = constant_node1->constant_value.float_value == constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value == constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value == constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value == constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value == constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value == constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value == constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value == constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant == operation\n");
					exit(1);
			}

			break;

		case LONG_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value == constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value == constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value == (u_int64_t)(constant_node2->constant_value.signed_long_value);
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value == constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value == constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value == (u_int32_t)(constant_node2->constant_value.signed_int_value);
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value == (u_int16_t)(constant_node2->constant_value.signed_short_value);
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value == constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value == (u_int16_t)(constant_node2->constant_value.signed_byte_value);
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value == constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value == (u_int8_t)(constant_node2->constant_value.char_value);
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant == operation\n");
					exit(1);
			}
			
			break;

		case LONG_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value == constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value = (u_int64_t)(constant_node1->constant_value.signed_long_value) == constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value == constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.signed_long_value = (u_int64_t)(constant_node1->constant_value.signed_long_value) == constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value == constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value == constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value == constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value = (u_int64_t)(constant_node1->constant_value.signed_long_value) == constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value == constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value = (u_int64_t)(constant_node1->constant_value.signed_long_value) == constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value == constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant == operation\n");
					exit(1);
			}

			break;

		case DOUBLE_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value == constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value == constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.double_value = constant_node1->constant_value.double_value == constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.double_value = constant_node1->constant_value.double_value == constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value == constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value == constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value == constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value == constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value == constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.double_value	= constant_node1->constant_value.double_value == constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value == constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant == operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value == constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value = (u_int8_t)(constant_node1->constant_value.signed_byte_value) == constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value == constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.signed_byte_value = (u_int8_t)(constant_node1->constant_value.signed_byte_value) == constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value == constant_node2->constant_value.signed_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value == constant_node2->constant_value.float_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value == constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value = (u_int8_t)(constant_node1->constant_value.signed_byte_value) == constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value == constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value = (u_int8_t)(constant_node1->constant_value.signed_byte_value) == constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value == constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant == operation\n");
					exit(1);
			}
			break;

		case BYTE_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value == constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value == constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value == (u_int64_t)(constant_node2->constant_value.signed_long_value);
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value == constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value == (u_int32_t)(constant_node2->constant_value.signed_int_value);
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value == constant_node2->constant_value.float_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value == (u_int16_t)(constant_node2->constant_value.signed_short_value);
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value == constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value == (u_int8_t)(constant_node2->constant_value.signed_byte_value);
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value == constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value == (u_int8_t)(constant_node2->constant_value.char_value);
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant == operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value == constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value = (u_int16_t)(constant_node1->constant_value.signed_short_value) == constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value == constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.signed_short_value = (u_int16_t)(constant_node1->constant_value.signed_short_value) == constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value == constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value == constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value == constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value = (u_int16_t)(constant_node1->constant_value.signed_short_value) == constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value == constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value = (u_int16_t)(constant_node1->constant_value.signed_short_value) == constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value == constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant == operation\n");
					exit(1);
			}

		case SHORT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value == constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value == constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value == (u_int64_t)(constant_node2->constant_value.signed_long_value);
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value == constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value == constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value == (u_int32_t)(constant_node2->constant_value.signed_int_value);
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value == (u_int32_t)(constant_node2->constant_value.signed_short_value);
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value == constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value == (u_int8_t)(constant_node2->constant_value.signed_byte_value);
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value == constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value == (u_int8_t)(constant_node2->constant_value.char_value);
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant == operation\n");
					exit(1);
			}


		case CHAR_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value == constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.char_value = (u_int8_t)(constant_node1->constant_value.char_value) == constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.char_value = constant_node1->constant_value.char_value == constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.char_value = (u_int8_t)(constant_node1->constant_value.char_value) == constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value == constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value == constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value == constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.char_value = (u_int8_t)(constant_node1->constant_value.char_value) == constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value == constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.char_value = (u_int8_t)(constant_node1->constant_value.char_value) == constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value == constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant == operation\n");
					exit(1);
			}

			break;

		default:
			printf("Fatal internal compiler error: Unsupported constant == operation\n");
			exit(1);
	}
}


/**
 * Emit the > of two given constants. The result will overwrite the first constant given
 *
 * The result will be: constant1 = constant1 > constant2
 */
void greater_than_constant_nodes(generic_ast_node_t* constant_node1, generic_ast_node_t* constant_node2){
	switch(constant_node1->constant_type){
		case INT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value > constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value > constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value > (u_int64_t)(constant_node2->constant_value.signed_long_value);
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value > constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value > constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value > (u_int32_t)(constant_node2->constant_value.signed_int_value);
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value > (u_int16_t)(constant_node2->constant_value.signed_short_value);
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value > constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value > (u_int8_t)(constant_node2->constant_value.signed_byte_value);
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value > constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value > (u_int8_t)(constant_node2->constant_value.char_value);
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant > operation\n");
					exit(1);
			}

			break;

		case INT_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value > constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value = (u_int64_t)(constant_node1->constant_value.signed_int_value) > constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value > constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.signed_int_value = (u_int32_t)(constant_node1->constant_value.signed_int_value) > constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value > constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value > constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value > constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value = (u_int32_t)(constant_node1->constant_value.signed_int_value) > constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value > constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value = (u_int32_t)(constant_node1->constant_value.signed_int_value) > constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value > constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant > operation\n");
					exit(1);
			}

			break;

		case FLOAT_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value > constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value > constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.float_value = constant_node1->constant_value.float_value > constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.float_value = constant_node1->constant_value.float_value > constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value > constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value > constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value > constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value > constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value > constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value > constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value > constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant > operation\n");
					exit(1);
			}

			break;

		case LONG_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value > constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value > constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value > (u_int64_t)(constant_node2->constant_value.signed_long_value);
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value > constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value > constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value > (u_int32_t)(constant_node2->constant_value.signed_int_value);
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value > (u_int16_t)(constant_node2->constant_value.signed_short_value);
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value > constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value > (u_int16_t)(constant_node2->constant_value.signed_byte_value);
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value > constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value > (u_int8_t)(constant_node2->constant_value.char_value);
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant > operation\n");
					exit(1);
			}
			
			break;

		case LONG_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value > constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value = (u_int64_t)(constant_node1->constant_value.signed_long_value) > constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value > constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.signed_long_value = (u_int64_t)(constant_node1->constant_value.signed_long_value) > constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value > constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value > constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value > constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value = (u_int64_t)(constant_node1->constant_value.signed_long_value) > constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value > constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value = (u_int64_t)(constant_node1->constant_value.signed_long_value) > constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value > constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant > operation\n");
					exit(1);
			}

			break;

		case DOUBLE_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value > constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value > constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.double_value = constant_node1->constant_value.double_value > constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.double_value = constant_node1->constant_value.double_value > constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value > constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value > constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value > constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value > constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value > constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.double_value	= constant_node1->constant_value.double_value > constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value > constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant > operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value > constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value = (u_int8_t)(constant_node1->constant_value.signed_byte_value) > constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value > constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.signed_byte_value = (u_int8_t)(constant_node1->constant_value.signed_byte_value) > constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value > constant_node2->constant_value.signed_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value > constant_node2->constant_value.float_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value > constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value = (u_int8_t)(constant_node1->constant_value.signed_byte_value) > constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value > constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value = (u_int8_t)(constant_node1->constant_value.signed_byte_value) > constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value > constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant > operation\n");
					exit(1);
			}
			break;

		case BYTE_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value > constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value > constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value > (u_int64_t)(constant_node2->constant_value.signed_long_value);
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value > constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value > (u_int32_t)(constant_node2->constant_value.signed_int_value);
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value > constant_node2->constant_value.float_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value > (u_int16_t)(constant_node2->constant_value.signed_short_value);
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value > constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value > (u_int8_t)(constant_node2->constant_value.signed_byte_value);
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value > constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value > (u_int8_t)(constant_node2->constant_value.char_value);
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant > operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value > constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value = (u_int16_t)(constant_node1->constant_value.signed_short_value) > constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value > constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.signed_short_value = (u_int16_t)(constant_node1->constant_value.signed_short_value) > constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value > constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value > constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value > constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value = (u_int16_t)(constant_node1->constant_value.signed_short_value) > constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value > constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value = (u_int16_t)(constant_node1->constant_value.signed_short_value) > constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value > constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant > operation\n");
					exit(1);
			}

		case SHORT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value > constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value > constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value > (u_int64_t)(constant_node2->constant_value.signed_long_value);
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value > constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value > constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value > (u_int32_t)(constant_node2->constant_value.signed_int_value);
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value > (u_int32_t)(constant_node2->constant_value.signed_short_value);
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value > constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value > (u_int8_t)(constant_node2->constant_value.signed_byte_value);
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value > constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value > (u_int8_t)(constant_node2->constant_value.char_value);
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant > operation\n");
					exit(1);
			}


		case CHAR_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value > constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.char_value = (u_int8_t)(constant_node1->constant_value.char_value) > constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.char_value = constant_node1->constant_value.char_value > constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.char_value = (u_int8_t)(constant_node1->constant_value.char_value) > constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value > constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value > constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value > constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.char_value = (u_int8_t)(constant_node1->constant_value.char_value) > constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value > constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.char_value = (u_int8_t)(constant_node1->constant_value.char_value) > constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value > constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant > operation\n");
					exit(1);
			}

			break;

		default:
			printf("Fatal internal compiler error: Unsupported constant > operation\n");
			exit(1);
	}
}


/**
 * Emit the >= of two given constants. The result will overwrite the first constant given
 *
 * The result will be: constant1 >= constant1 > constant2
 */
void greater_than_or_equal_to_constant_nodes(generic_ast_node_t* constant_node1, generic_ast_node_t* constant_node2){
	switch(constant_node1->constant_type){
		case INT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value >= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value >= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value >= (u_int64_t)(constant_node2->constant_value.signed_long_value);
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value >= constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value >= constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value >= (u_int32_t)(constant_node2->constant_value.signed_int_value);
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value >= (u_int16_t)(constant_node2->constant_value.signed_short_value);
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value >= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value >= (u_int8_t)(constant_node2->constant_value.signed_byte_value);
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value >= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value >= (u_int8_t)(constant_node2->constant_value.char_value);
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant >= operation\n");
					exit(1);
			}

			break;

		case INT_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value >= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value = (u_int64_t)(constant_node1->constant_value.signed_int_value) >= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value >= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.signed_int_value = (u_int32_t)(constant_node1->constant_value.signed_int_value) >= constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value >= constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value >= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value >= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value = (u_int32_t)(constant_node1->constant_value.signed_int_value) >= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value >= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value = (u_int32_t)(constant_node1->constant_value.signed_int_value) >= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value >= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant >= operation\n");
					exit(1);
			}

			break;

		case FLOAT_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value >= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value >= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.float_value = constant_node1->constant_value.float_value >= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.float_value = constant_node1->constant_value.float_value >= constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value >= constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value >= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value >= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value >= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value >= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value >= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value >= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant >= operation\n");
					exit(1);
			}

			break;

		case LONG_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value >= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value >= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value >= (u_int64_t)(constant_node2->constant_value.signed_long_value);
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value >= constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value >= constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value >= (u_int32_t)(constant_node2->constant_value.signed_int_value);
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value >= (u_int16_t)(constant_node2->constant_value.signed_short_value);
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value >= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value >= (u_int16_t)(constant_node2->constant_value.signed_byte_value);
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value >= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value >= (u_int8_t)(constant_node2->constant_value.char_value);
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant >= operation\n");
					exit(1);
			}
			
			break;

		case LONG_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value >= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value = (u_int64_t)(constant_node1->constant_value.signed_long_value) >= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value >= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.signed_long_value = (u_int64_t)(constant_node1->constant_value.signed_long_value) >= constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value >= constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value >= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value >= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value = (u_int64_t)(constant_node1->constant_value.signed_long_value) >= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value >= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value = (u_int64_t)(constant_node1->constant_value.signed_long_value) >= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value >= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant >= operation\n");
					exit(1);
			}

			break;

		case DOUBLE_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value >= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value >= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.double_value = constant_node1->constant_value.double_value >= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.double_value = constant_node1->constant_value.double_value >= constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value >= constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value >= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value >= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value >= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value >= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.double_value	= constant_node1->constant_value.double_value >= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value >= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant >= operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value >= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value = (u_int8_t)(constant_node1->constant_value.signed_byte_value) >= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value >= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.signed_byte_value = (u_int8_t)(constant_node1->constant_value.signed_byte_value) >= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value >= constant_node2->constant_value.signed_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value >= constant_node2->constant_value.float_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value >= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value = (u_int8_t)(constant_node1->constant_value.signed_byte_value) >= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value >= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value = (u_int8_t)(constant_node1->constant_value.signed_byte_value) >= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value >= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant >= operation\n");
					exit(1);
			}
			break;

		case BYTE_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value >= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value >= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value >= (u_int64_t)(constant_node2->constant_value.signed_long_value);
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value >= constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value >= (u_int32_t)(constant_node2->constant_value.signed_int_value);
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value >= constant_node2->constant_value.float_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value >= (u_int16_t)(constant_node2->constant_value.signed_short_value);
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value >= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value >= (u_int8_t)(constant_node2->constant_value.signed_byte_value);
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value >= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value >= (u_int8_t)(constant_node2->constant_value.char_value);
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant >= operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value >= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value = (u_int16_t)(constant_node1->constant_value.signed_short_value) >= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value >= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.signed_short_value = (u_int16_t)(constant_node1->constant_value.signed_short_value) >= constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value >= constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value >= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value >= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value = (u_int16_t)(constant_node1->constant_value.signed_short_value) >= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value >= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value = (u_int16_t)(constant_node1->constant_value.signed_short_value) >= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value >= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant >= operation\n");
					exit(1);
			}

		case SHORT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value >= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value >= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value >= (u_int64_t)(constant_node2->constant_value.signed_long_value);
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value >= constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value >= constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value >= (u_int32_t)(constant_node2->constant_value.signed_int_value);
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value >= (u_int32_t)(constant_node2->constant_value.signed_short_value);
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value >= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value >= (u_int8_t)(constant_node2->constant_value.signed_byte_value);
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value >= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value >= (u_int8_t)(constant_node2->constant_value.char_value);
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant >= operation\n");
					exit(1);
			}


		case CHAR_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value >= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.char_value = (u_int8_t)(constant_node1->constant_value.char_value) >= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.char_value = constant_node1->constant_value.char_value >= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.char_value = (u_int8_t)(constant_node1->constant_value.char_value) >= constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value >= constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value >= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value >= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.char_value = (u_int8_t)(constant_node1->constant_value.char_value) >= constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value >= constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.char_value = (u_int8_t)(constant_node1->constant_value.char_value) >= constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value >= constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant >= operation\n");
					exit(1);
			}

			break;

		default:
			printf("Fatal internal compiler error: Unsupported constant >= operation\n");
			exit(1);
	}
}


/**
 * Emit the < of two given constants. The result will overwrite the first constant given
 *
 * The result will be: constant1 = constant1 < constant2
 */
void less_than_constant_nodes(generic_ast_node_t* constant_node1, generic_ast_node_t* constant_node2){
	switch(constant_node1->constant_type){
		case INT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value < constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value < constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value < (u_int64_t)(constant_node2->constant_value.signed_long_value);
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value < constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value < constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value < (u_int32_t)(constant_node2->constant_value.signed_int_value);
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value < (u_int16_t)(constant_node2->constant_value.signed_short_value);
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value < constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value < (u_int8_t)(constant_node2->constant_value.signed_byte_value);
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value < constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value < (u_int8_t)(constant_node2->constant_value.char_value);
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant < operation\n");
					exit(1);
			}

			break;

		case INT_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value < constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value = (u_int64_t)(constant_node1->constant_value.signed_int_value) < constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value < constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.signed_int_value = (u_int32_t)(constant_node1->constant_value.signed_int_value) < constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value < constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value < constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value < constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value = (u_int32_t)(constant_node1->constant_value.signed_int_value) < constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value < constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value = (u_int32_t)(constant_node1->constant_value.signed_int_value) < constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value < constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant < operation\n");
					exit(1);
			}

			break;

		case FLOAT_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value < constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value < constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.float_value = constant_node1->constant_value.float_value < constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.float_value = constant_node1->constant_value.float_value < constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value < constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value < constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value < constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value < constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value < constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value < constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value < constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant < operation\n");
					exit(1);
			}

			break;

		case LONG_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value < constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value < constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value < (u_int64_t)(constant_node2->constant_value.signed_long_value);
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value < constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value < constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value < (u_int32_t)(constant_node2->constant_value.signed_int_value);
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value < (u_int16_t)(constant_node2->constant_value.signed_short_value);
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value < constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value < (u_int16_t)(constant_node2->constant_value.signed_byte_value);
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value < constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value < (u_int8_t)(constant_node2->constant_value.char_value);
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant < operation\n");
					exit(1);
			}
			
			break;

		case LONG_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value < constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value = (u_int64_t)(constant_node1->constant_value.signed_long_value) < constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value < constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.signed_long_value = (u_int64_t)(constant_node1->constant_value.signed_long_value) < constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value < constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value < constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value < constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value = (u_int64_t)(constant_node1->constant_value.signed_long_value) < constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value < constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value = (u_int64_t)(constant_node1->constant_value.signed_long_value) < constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value < constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant < operation\n");
					exit(1);
			}

			break;

		case DOUBLE_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value < constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value < constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.double_value = constant_node1->constant_value.double_value < constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.double_value = constant_node1->constant_value.double_value < constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value < constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value < constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value < constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value < constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value < constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.double_value	= constant_node1->constant_value.double_value < constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value < constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant < operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value < constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value = (u_int8_t)(constant_node1->constant_value.signed_byte_value) < constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value < constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.signed_byte_value = (u_int8_t)(constant_node1->constant_value.signed_byte_value) < constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value < constant_node2->constant_value.signed_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value < constant_node2->constant_value.float_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value < constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value = (u_int8_t)(constant_node1->constant_value.signed_byte_value) < constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value < constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_byte_value = (u_int8_t)(constant_node1->constant_value.signed_byte_value) < constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_byte_value = constant_node1->constant_value.signed_byte_value < constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant < operation\n");
					exit(1);
			}
			break;

		case BYTE_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value < constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value < constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value < (u_int64_t)(constant_node2->constant_value.signed_long_value);
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value < constant_node2->constant_value.unsigned_int_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value < (u_int32_t)(constant_node2->constant_value.signed_int_value);
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value < constant_node2->constant_value.float_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value < (u_int16_t)(constant_node2->constant_value.signed_short_value);
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value < constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value < (u_int8_t)(constant_node2->constant_value.signed_byte_value);
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value < constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_byte_value = constant_node1->constant_value.unsigned_byte_value < (u_int8_t)(constant_node2->constant_value.char_value);
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant < operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value < constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value = (u_int16_t)(constant_node1->constant_value.signed_short_value) < constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value < constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.signed_short_value = (u_int16_t)(constant_node1->constant_value.signed_short_value) < constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value < constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value < constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value < constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value = (u_int16_t)(constant_node1->constant_value.signed_short_value) < constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value < constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value = (u_int16_t)(constant_node1->constant_value.signed_short_value) < constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value < constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant < operation\n");
					exit(1);
			}

		case SHORT_CONST_FORCE_U:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value < constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value < constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value < (u_int64_t)(constant_node2->constant_value.signed_long_value);
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value < constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value < constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value < (u_int32_t)(constant_node2->constant_value.signed_int_value);
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value < (u_int32_t)(constant_node2->constant_value.signed_short_value);
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value < constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value < (u_int8_t)(constant_node2->constant_value.signed_byte_value);
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value < constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value < (u_int8_t)(constant_node2->constant_value.char_value);
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant < operation\n");
					exit(1);
			}


		case CHAR_CONST:
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value < constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.char_value = (u_int8_t)(constant_node1->constant_value.char_value) < constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.char_value = constant_node1->constant_value.char_value < constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.char_value = (u_int8_t)(constant_node1->constant_value.char_value) < constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value < constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value < constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value < constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.char_value = (u_int8_t)(constant_node1->constant_value.char_value) < constant_node2->constant_value.unsigned_short_value;
					break;
				case BYTE_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value < constant_node2->constant_value.signed_byte_value;
					break;
				case BYTE_CONST_FORCE_U:
					constant_node1->constant_value.char_value = (u_int8_t)(constant_node1->constant_value.char_value) < constant_node2->constant_value.unsigned_byte_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value < constant_node2->constant_value.char_value;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant < operation\n");
					exit(1);
			}

			break;

		default:
			printf("Fatal internal compiler error: Unsupported constant < operation\n");
			exit(1);
	}
}


/**
 * Emit the <= of two given constants. The result will overwrite the first constant given
 *
 * The result will be: constant1 = constant1 < constant2
 */
void less_than_or_equal_to_constant_nodes(generic_ast_node_t* constant_node1, generic_ast_node_t* constant_node2){
	//Go based on the first one's type
	switch(constant_node1->constant_type){
		case INT_CONST_FORCE_U:
			//Now go based on the second one's type
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value <= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value <= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value <= (u_int64_t)(constant_node2->constant_value.signed_long_value);
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value <= constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value <= constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value <= (u_int32_t)(constant_node2->constant_value.signed_int_value);
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value <= (u_int16_t)(constant_node2->constant_value.signed_short_value);
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value <= constant_node2->constant_value.unsigned_short_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_int_value = constant_node1->constant_value.unsigned_int_value <= (u_int8_t)(constant_node2->constant_value.char_value);
					break;
				//This should never happen
				default:
					printf("Fatal internal compiler error: Unsupported constant <= operation\n");
					exit(1);
			}

			break;

		case INT_CONST:
			//Now go based on the second one's type
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value <= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value = (u_int64_t)(constant_node1->constant_value.signed_int_value) <= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value <= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.signed_int_value = (u_int32_t)(constant_node1->constant_value.signed_int_value) <= constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value <= constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value <= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value <= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_int_value = (u_int32_t)(constant_node1->constant_value.signed_int_value) <= constant_node2->constant_value.unsigned_short_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_int_value = constant_node1->constant_value.signed_int_value <= constant_node2->constant_value.char_value;
					break;
				//This should never happen
				default:
					printf("Fatal internal compiler error: Unsupported constant <= operation\n");
					exit(1);
			}

			break;

		case FLOAT_CONST:
			//Now go based on the second one's type
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value <= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value <= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.float_value = constant_node1->constant_value.float_value <= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.float_value = constant_node1->constant_value.float_value <= constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value <= constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value <= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value <= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value <= constant_node2->constant_value.unsigned_short_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.float_value = constant_node1->constant_value.float_value <= constant_node2->constant_value.char_value;
					break;
				//This should never happen
				default:
					printf("Fatal internal compiler error: Unsupported constant <= operation\n");
					exit(1);
			}

			break;

		case LONG_CONST_FORCE_U:
			//Now go based on the second one's type
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value <= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value <= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value <= (u_int64_t)(constant_node2->constant_value.signed_long_value);
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value <= constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value <= constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value <= (u_int32_t)(constant_node2->constant_value.signed_int_value);
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value <= (u_int16_t)(constant_node2->constant_value.signed_short_value);
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value <= constant_node2->constant_value.unsigned_short_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_long_value = constant_node1->constant_value.unsigned_long_value <= (u_int8_t)(constant_node2->constant_value.char_value);
					break;
				//This should never happen
				default:
					printf("Fatal internal compiler error: Unsupported constant <= operation\n");
					exit(1);
			}
			
			break;

		case LONG_CONST:
			//Now go based on the second one's type
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value <= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value = (u_int64_t)(constant_node1->constant_value.signed_long_value) <= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value <= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.signed_long_value = (u_int64_t)(constant_node1->constant_value.signed_long_value) <= constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value <= constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value <= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value <= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_long_value = (u_int64_t)(constant_node1->constant_value.signed_long_value) <= constant_node2->constant_value.unsigned_short_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_long_value = constant_node1->constant_value.signed_long_value <= constant_node2->constant_value.char_value;
					break;
				//This should never happen
				default:
					printf("Fatal internal compiler error: Unsupported constant <= operation\n");
					exit(1);
			}

			break;

		case DOUBLE_CONST:
			//Now go based on the second one's type
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value <= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value <= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.double_value = constant_node1->constant_value.double_value <= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.double_value = constant_node1->constant_value.double_value <= constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value <= constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value <= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value <= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value <= constant_node2->constant_value.unsigned_short_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.double_value = constant_node1->constant_value.double_value <= constant_node2->constant_value.char_value;
					break;
				//This should never happen
				default:
					printf("Fatal internal compiler error: Unsupported constant <= operation\n");
					exit(1);
			}

			break;


		case SHORT_CONST:
			//Now go based on the second one's type
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value <= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value = (u_int16_t)(constant_node1->constant_value.signed_short_value) <= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value <= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.signed_short_value = (u_int16_t)(constant_node1->constant_value.signed_short_value) <= constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value <= constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value <= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value <= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.signed_short_value = (u_int16_t)(constant_node1->constant_value.signed_short_value) <= constant_node2->constant_value.unsigned_short_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.signed_short_value = constant_node1->constant_value.signed_short_value <= constant_node2->constant_value.char_value;
					break;
				//This should never happen
				default:
					printf("Fatal internal compiler error: Unsupported constant <= operation\n");
					exit(1);
			}

		case SHORT_CONST_FORCE_U:
			//Now go based on the second one's type
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value <= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value <= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value <= (u_int64_t)(constant_node2->constant_value.signed_long_value);
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value <= constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value <= constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value <= (u_int32_t)(constant_node2->constant_value.signed_int_value);
					break;
				case SHORT_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value <= (u_int32_t)(constant_node2->constant_value.signed_short_value);
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value <= constant_node2->constant_value.unsigned_short_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.unsigned_short_value = constant_node1->constant_value.unsigned_short_value <= (u_int8_t)(constant_node2->constant_value.char_value);
					break;
				//This should never happen
				default:
					printf("Fatal internal compiler error: Unsupported constant <= operation\n");
					exit(1);
			}


		case CHAR_CONST:
			//Now go based on the second one's type
			switch(constant_node2->constant_type){
				case DOUBLE_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value <= constant_node2->constant_value.double_value;
					break;
				case LONG_CONST_FORCE_U:
					constant_node1->constant_value.char_value = (u_int8_t)(constant_node1->constant_value.char_value) <= constant_node2->constant_value.unsigned_long_value;
					break;
				case LONG_CONST:
					 constant_node1->constant_value.char_value = constant_node1->constant_value.char_value <= constant_node2->constant_value.signed_long_value;
					break;
				case INT_CONST_FORCE_U:
					 constant_node1->constant_value.char_value = (u_int8_t)(constant_node1->constant_value.char_value) <= constant_node2->constant_value.unsigned_int_value;
					break;
				case FLOAT_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value <= constant_node2->constant_value.float_value;
					break;
				case INT_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value <= constant_node2->constant_value.signed_int_value;
					break;
				case SHORT_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value <= constant_node2->constant_value.signed_short_value;
					break;
				case SHORT_CONST_FORCE_U:
					constant_node1->constant_value.char_value = (u_int8_t)(constant_node1->constant_value.char_value) <= constant_node2->constant_value.unsigned_short_value;
					break;
				case CHAR_CONST:
					constant_node1->constant_value.char_value = constant_node1->constant_value.char_value <= constant_node2->constant_value.char_value;
					break;
				//This should never happen
				default:
					printf("Fatal internal compiler error: Unsupported constant <= operation\n");
					exit(1);
			}

			break;

		//This should never happen
		default:
			printf("Fatal internal compiler error: Unsupported constant <= operation\n");
			exit(1);
	}
}


/**
 * We will completely duplicate a deferred statement here. Since all deferred statements
 * are logical expressions, we will perform a deep copy to create an entirely new
 * chain of deferred statements
 */
generic_ast_node_t* duplicate_subtree(generic_ast_node_t* duplicatee, side_type_t side){
	//Base case here -- although in theory we shouldn't make it here
	if(duplicatee == NULL){
		return NULL;
	}

	//Duplicate the node here
	generic_ast_node_t* duplicated_root = duplicate_node(duplicatee, side);

	//Now for each child in the node, we duplicate it and add it in as a child
	generic_ast_node_t* child_cursor = duplicatee->first_child;

	//The duplicated child
	generic_ast_node_t* duplicated_child = NULL;

	//So long as we aren't null
	while(child_cursor != NULL){
		//Recursive call
		duplicated_child = duplicate_subtree(child_cursor, side);

		//Add the duplicate child into the node
		add_child_node(duplicated_root, duplicated_child);

		//Advance the cursor
		child_cursor = child_cursor->next_sibling;
	}

	//Return the duplicate root
	return duplicated_root;
}


/**
 * A utility function for duplicating nodes
 */
generic_ast_node_t* duplicate_node(generic_ast_node_t* node, side_type_t side){
	//First allocate the overall node here
	generic_ast_node_t* duplicated = calloc(1, sizeof(generic_ast_node_t));

	//We will perform a deep copy here
	memcpy(duplicated, node, sizeof(generic_ast_node_t));

	//Let's see if we have any special cases here that require extra attention
	switch(node->ast_node_type){
		//Asm inline is a special case because we'll need to copy the assembly over
		case AST_NODE_TYPE_ASM_INLINE_STMT:
		case AST_NODE_TYPE_IDENTIFIER:
			duplicated->string_value = clone_dynamic_string(&(node->string_value));
			break;

		//Constants are another special case, because they contain a special inner node
		case AST_NODE_TYPE_CONSTANT:
			//If we have a string constant, we'll duplicate the dynamic string
			if(node->constant_type == STR_CONST){
				duplicated->string_value = clone_dynamic_string(&(node->string_value));
			}

			break;

		//By default we do nothing, this is just there for the compiler to not complain
		default:
			break;

	}
	
	//We don't want to hold onto any of these old references here
	duplicated->first_child = NULL;
	duplicated->next_sibling = NULL;

	//Add the appropriate side
	duplicated->side = side;

	//Add this into the memory management structure
	dynamic_array_add(&created_nodes, duplicated);

	//Give back the duplicated node
	return duplicated;
}


/**
 * Simple function that handles all of the hard work for node allocation for us. The user gives us the pointer
 * that they want to use. It is assumed that the user already knows the proper type and takes appropriate action based
 * on that
*/
generic_ast_node_t* ast_node_alloc(ast_node_type_t ast_node_type, side_type_t side){
	//We always have a generic AST node
	generic_ast_node_t* node = calloc(1, sizeof(generic_ast_node_t));

	//Add this into our memoyr management structure
	dynamic_array_add(&created_nodes, node);

	//Assign the class
	node->ast_node_type = ast_node_type;

	//Assign the side of the node
	node->side = side;

	//And give it back
	return node;
}


/**
 * A helper function that will appropriately add a child node into the parent
 */
void add_child_node(generic_ast_node_t* parent, generic_ast_node_t* child){
	//We first deal with a special case -> if this is the first child
	//If so, we just add it in and leave
	if(parent->first_child == NULL){
		parent->first_child = child;
		return;
	}

	/**
	 * But if we make it here, we now know that there are other children. As such,
	 * we need to move to the end of the child linked list and append it there
	 */
	generic_ast_node_t* cursor = parent->first_child;

	//As long as there are more siblings
	while(cursor->next_sibling != NULL){
		cursor = cursor->next_sibling;
	}

	//When we get here, we know that we're at the very last child, so
	//we'll add it in and be finished
	cursor->next_sibling = child;
}


/**
 * Global tree deallocation function
 */
void ast_dealloc(){
	//Run through all of the nodes in the created array
	for(u_int16_t i = 0; i < created_nodes.current_index; i++){
		//Grab the node out
		generic_ast_node_t* node = dynamic_array_get_at(&created_nodes, i);

		//Some additional freeing may be needed
		switch(node->ast_node_type){
			case AST_NODE_TYPE_IDENTIFIER:
			case AST_NODE_TYPE_ASM_INLINE_STMT:
				dynamic_string_dealloc(&(node->string_value));
				break;

			//We could see a case where this is a string const
			case AST_NODE_TYPE_CONSTANT:
				if(node->constant_type == STR_CONST){
					dynamic_string_dealloc(&(node->string_value));
				}
				break;

			//By default we don't need to worry about this
			default:
				break;
		}

		//Destroy temp here
		free(node);
	}

	//Finally, we can destroy the entire array as well
	dynamic_array_dealloc(&created_nodes);
}
