/**
 * The implementation file for the type_system.h header file
*/

#include "type_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

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
generic_type_t* create_basic_type(char* type_name, Token basic_type){
	//Dynamically allocate
	generic_type_t* type = calloc(1, sizeof(generic_type_t));
	//Store the type class
	type->type_class = TYPE_CLASS_BASIC;
	//Defined line num is at -1 because it's a basic type
	type->line_number = -1;

	//Allocate a basic type, all other pointers will be null
	type->basic_type = calloc(1, sizeof(basic_type_t));
	
	//Copy the type name
	strcpy(type->type_name, type_name);
	//Assign the type in
	type->basic_type->basic_type = basic_type;

	//Now we can immediately determine the size based on what the actual type is
	if(basic_type == CHAR || basic_type == S_INT8 || basic_type == U_INT8){
		//1 BYTE
		type->basic_type->size = 1;
	} else if(basic_type == S_INT16 || basic_type == U_INT16){
		//2 BYTES
		type->basic_type->size = 2;
	} else if(basic_type == U_INT32 || basic_type == S_INT32 || basic_type == FLOAT32){
		//4 BYTES
		type->basic_type->size = 4;
	} else if(basic_type == VOID){
		//0 BYTES - special case
		type->basic_type->size = 0;
	} else {
		//Otheriwse is 8 BYTES
		type->basic_type->size = 8;
	}

	//Give back the pointer, it will need to be freed eventually
	return type;
}


/**
 * Create a pointer type dynamically. In order to have a pointer type, we must also
 * have what it points to.
 */
generic_type_t* create_pointer_type(generic_type_t* points_to, u_int32_t line_number){
	generic_type_t* type = calloc(1,  sizeof(generic_type_t));

	//Pointer type class
	type->type_class = TYPE_CLASS_POINTER;

	//Where was it declared
	type->line_number = line_number;

	//Let's first copy the type name in
	strcpy(type->type_name, points_to->type_name);

	//And then we add a pointer onto the end of it
	strcat(type->type_name, "*");

	//Now we'll make the actual pointer type
	type->pointer_type = calloc(1, sizeof(pointer_type_t));

	//Store what it points to
	type->pointer_type->points_to = points_to;

	//A pointer is always 8 bytes(Ollie lang is for x86-64 only)
	type->pointer_type->size = 8;

	return type;
}


/**
 * Create an array type dynamically. In order to have an array type, we must also know
 * what type its memebers are and the size of the array
 *
 * In ollie language, static arrays must have their overall size known at compile time.
 */
generic_type_t* create_array_type(generic_type_t* points_to, u_int32_t line_number, u_int32_t num_members){
	generic_type_t* type = calloc(1,  sizeof(generic_type_t));
	//The array bounds string
	char array_bound_str[50];

	//Array type class
	type->type_class = TYPE_CLASS_ARRAY;

	//Where was it declared
	type->line_number = line_number;

	//Let's first copy the type name in
	strcpy(type->type_name, points_to->type_name);

	//Let's construct the array bounds string
	sprintf(array_bound_str, "[%d]", num_members);

	//Concatenate it to the name of it
	strcat(type->type_name, array_bound_str);
	
	//Now we'll make the actual pointer type
	type->array_type = calloc(1, sizeof(array_type_t));

	//Store what it points to
	type->array_type->member_type = points_to;
	
	//Store the number of members
	type->array_type->num_members = num_members;

	return type;
}


/**
 * Dynamically allocate and create an enumerated type
 */
generic_type_t* create_enumerated_type(char* type_name, u_int32_t line_number){
	generic_type_t* type = calloc(1, sizeof(generic_type_t));

	//Assign the class
	type->type_class = TYPE_CLASS_ENUMERATED;
	
	//Where is the declaration?
	type->line_number = line_number;

	//Copy the name
	strcpy(type->type_name, type_name);

	//Reserve space for this
	type->enumerated_type = calloc(1, sizeof(enumerated_type_t));

	return type;
}


/**
 * Dynamically allocate and create a constructed type
 */
generic_type_t* create_constructed_type(char* type_name, u_int32_t line_number){
	generic_type_t* type = calloc(1, sizeof(generic_type_t));

	//Assign the class
	type->type_class = TYPE_CLASS_CONSTRUCT;
	
	//Where is the declaration?
	type->line_number = line_number;

	//Copy the name
	strcpy(type->type_name, type_name);

	//Reserve space for this
	type->construct_type = calloc(1, sizeof(constructed_type_t));

	return type;
}


/**
 * Dynamically allocate and create an aliased type
 */
generic_type_t* create_aliased_type(char* type_name, generic_type_t* aliased_type, u_int32_t line_number){
	generic_type_t* type = calloc(1, sizeof(generic_type_t));

	//Assign the class
	type->type_class = TYPE_CLASS_ALIAS;
	
	//Where is the declaration?
	type->line_number = line_number;

	//Copy the name
	strcpy(type->type_name, type_name);

	//Dynamically allocate the aliased type record
	type->aliased_type = calloc(1, sizeof(aliased_type_t));

	//Store this reference in here
	type->aliased_type->aliased_type = aliased_type;

	return type;
}


/**
 * Destroy a type that is no longer in use
 * TODO NOT DONE
*/
void destroy_type(generic_type_t* type){
	//We will take action based on what kind of type that we have
	//Just free type and leave
	if(type->type_class == TYPE_CLASS_BASIC){
		free(type->basic_type);
		free(type);
	} else if(type->type_class == TYPE_CLASS_ARRAY){
		free(type->array_type);
		free(type);
	} else if(type->type_class == TYPE_CLASS_POINTER){
		free(type->pointer_type);
		free(type);
	} else if(type->type_class == TYPE_CLASS_ENUMERATED){

	} else {

	}
}
