/**
 * The implementation file for the type_system.h header file
*/

#include "type_system.h"
#include <stdlib.h>
#include <string.h>

/**
 * Are two types compatible?
*/
u_int8_t types_compatible(generic_type_t* typeA, generic_type_t* typeB){
	//If they aren't the exact same type class, then they are not equivalent
	if(typeA->type_class != typeB->type_class){
		return 0;
	}

	//If we make it here we know that they're the same class of type
	if(typeA->type_class == TYPE_CLASS_BASIC){
		//If they're the exact same type, no more checks are needed
		if(typeA->basic_type->basic_type == typeB->basic_type->basic_type){
			return 1;
		}

		//However, we'll need to do some more intense checking here
		

	}
	//TODO MORE NEEDED
	

	return 1;
}


/**
 * Create a basic type dynamically
*/
generic_type_t* create_basic_type(char* type_name, BASIC_TYPE basic_type){
	//Dynamically allocate
	generic_type_t* type = calloc(1, sizeof(generic_type_t));
	//Store the type class
	type->type_class = TYPE_CLASS_BASIC;

	//Allocate a basic type, all other pointers will be null
	type->basic_type = calloc(1, sizeof(basic_type_t));
	
	//Copy the type name
	strcpy(type->type_name, type_name);

	//Now we can immediately determine the size based on what the actual type is
	if(basic_type == TYPE_CHAR || basic_type == TYPE_S_INT8 || basic_type == TYPE_U_INT8){
		//1 BYTE
		type->basic_type->size = 1;
	} else if(basic_type == TYPE_S_INT16 || basic_type == TYPE_U_INT16){
		//2 BYTES
		type->basic_type->size = 2;
	} else if(basic_type == TYPE_U_INT32 || basic_type == TYPE_S_INT32 || basic_type == TYPE_FLOAT32){
		//4 BYTES
		type->basic_type->size = 4;
	} else {
		//Otheriwse is 8 BYTES
		type->basic_type->size = 8;
	}

	//Give back the pointer, it will need to be freed eventually
	return type;
}
