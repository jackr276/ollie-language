/**
 * The implementation file for the type_system.h header file
*/

#include "type_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>


/**
 * Are two types equivalent(as in, the exact same)
 */
u_int8_t types_equivalent(generic_type_t* typeA, generic_type_t* typeB){
	//If they are not in the same type class, then they are not equivalent
	if(typeA->type_class != typeB->type_class){
		return 0;
	}

	//Now that we know they are in the same class, we need to check if they're the exact same
	//If they are the exact same, return 1. Otherwise, return 0
	if(strcmp(typeA->type_name, typeB->type_name) == 0){
		return 1;
	}

	//Otherwise they aren't the exact same, so
	return 0;
}


/**
 * Are two types compatible with eachother
 *
 * TYPE COMPATIBILITY RULES:
 * 	1.) Two constructs are compatible iff they are the exact same construct
 * 	2.) Two enums are compatible iff they are the exact same enum
 * 	3.) Pointers are compatible
 * 	4.) If we're trying to put a small int into a large one that's fine
 * 	5.) If we're trying to put a small float into a large one that's fine
 * 	6.) Arrays are compatible if they point to the same type
*/
generic_type_t* types_compatible(generic_type_t* typeA, generic_type_t* typeB){
	//Handle construct types: very strict compatibility rules here
	if(typeA->type_class == TYPE_CLASS_CONSTRUCT){
		//If type B isn't a construct it's over
		if(typeB->type_class != TYPE_CLASS_CONSTRUCT){
			return NULL;
		}

		//Now if they don't have the exact same name, it's also over
		if(strcmp(typeA->type_name, typeB->type_name) != 0){
			return NULL;
		}

		//Otherwise they are compatible, so we'll return typeA
		return typeA;
	}

	//Handle enum types: also very strict compatibility rules here
	if(typeA->type_class == TYPE_CLASS_ENUMERATED){
		//If it's an int it works
		if(typeB->type_class == TYPE_CLASS_BASIC){
			if(typeB->basic_type->basic_type == U_INT8 ||
	  		   typeB->basic_type->basic_type == U_INT16 ||
	  		   typeB->basic_type->basic_type == U_INT32 ||
	  		   typeB->basic_type->basic_type == U_INT64){

				//Non fancy type wings
				return typeB;
			}
		}

		//If type B isn't an enum, we're out
		if(typeB->type_class != TYPE_CLASS_ENUMERATED){
			return NULL;
		}

		//Otherwise we need to ensure that their names are the exact same
		if(strcmp(typeA->type_name, typeB->type_name) != 0){
			return NULL;
		}

		//Otherwise return type A
		return typeA;
	}

	//Handle array types. Array types are equivalent if they point to the same stuff:w
	if(typeA->type_class == TYPE_CLASS_ARRAY){
		//Store what type A points to
		generic_type_t* type_a_points_to = typeA->array_type->member_type;
		
		//If type B is an array, it needs to point to what A points to
		if(typeB->type_class == TYPE_CLASS_ARRAY){
			if(types_equivalent(type_a_points_to, typeB->array_type->member_type) == 0){
				return NULL;
			}

			//Otherwise return type A
			return typeA;
		}

		//Otherwise this is null
		return NULL;
	}

	//If type A is a pointer, we can assign it to another pointer or an array
	if(typeA->type_class == TYPE_CLASS_POINTER){
		//Store what type A points to
		generic_type_t* type_a_points_to = typeA->pointer_type->points_to;
		
		//If it's a pointer it's fine, no matter what it points to
		if(typeB->type_class == TYPE_CLASS_POINTER){
			//Otherwise it worked so
			return typeA;
		}

		//If it's an array it's also fine, arrays are pointers
		if(typeB->type_class == TYPE_CLASS_ARRAY){
			if(types_compatible(type_a_points_to, typeB->array_type->member_type) == NULL){
				return NULL;
			}

			//Otherwise it worked so
			return typeA;
		}

		//Otherwise this failed
		return NULL;
	}

	//If we make it down here, we know that type A is a basic type. If type B isn't,
	//then we're done here
	if(typeB->type_class != TYPE_CLASS_BASIC){
		//One exception -- if type A is an int
		if(typeA->basic_type->basic_type == U_INT8 ||
		   typeA->basic_type->basic_type == U_INT16 ||
		   typeA->basic_type->basic_type == U_INT32 ||
		   typeA->basic_type->basic_type == U_INT64){
			return typeA;
		}

		//Otherwise it's bad
		return NULL;
	}

	//Otherwise we know that we have a basic type here
	Token typeA_basic_type = typeA->basic_type->basic_type;
	Token typeB_basic_type = typeB->basic_type->basic_type;

	//If one of these is void, they must both be void
	if(typeA_basic_type == VOID){
		//Type B also needs to be void
		if(typeB_basic_type != VOID){
			return NULL;
		}

		//Otherwise it worked
		return typeA;
	}

	//If type A is a float64, we need to see a float64 or a float32
	if(typeA_basic_type == FLOAT64){
		//Fail case here
		if(typeB_basic_type != FLOAT32 && typeB_basic_type != FLOAT64){
			return NULL;
		}

		//Otherwise it worked
		return typeA;
	}
	
	//If type A is a float32, we need to see a float32
	if(typeA_basic_type == FLOAT32){
		//Fail case here
		if(typeB_basic_type != FLOAT32){
			return NULL;
		}

		//Otherwise it worked
		return typeA;
	}

	//Now for ints, if we see an INT that is smaller, we're good
	if(typeA_basic_type == S_INT64 || typeA_basic_type == U_INT64){
		//It's only bad if we see floats or void
		if(typeB_basic_type == VOID || typeB_basic_type == FLOAT32 || typeB_basic_type == FLOAT64){
			return NULL;
		}

		//Otherwise it's fine so
		return typeA;
	}

	//Now for ints, if we see an INT that is smaller, we're good
	if(typeA_basic_type == U_INT32 || typeA_basic_type == S_INT32){
		//It's only bad if we see floats or void
		if(typeB_basic_type == VOID || typeB_basic_type == FLOAT32 || typeB_basic_type == FLOAT64
		   || typeB_basic_type == S_INT64 || typeB_basic_type == U_INT64){
			return NULL;
		}

		//Otherwise it's fine so
		return typeA;
	}

	//Now for ints, if we see an INT that is smaller, we're good
	if(typeA_basic_type == U_INT16 || typeA_basic_type == S_INT16){
		//If we don't see a smaller or same size one, we fail
		if(typeB_basic_type != U_INT16 && typeB_basic_type != S_INT16 && typeB_basic_type != S_INT8 
		  && typeB_basic_type != U_INT8 && typeB_basic_type != CHAR){
			return NULL;
		}

		//Otherwise it's fine so
		return typeA;
	}
	
	//Now for ints, if we see an INT that is smaller, we're good
	if(typeA_basic_type == U_INT8 || typeA_basic_type == S_INT8 || typeA_basic_type == CHAR){
		//If we don't see a smaller or same size one, we fail
		if(typeB_basic_type != S_INT8 && typeB_basic_type != U_INT8 && typeB_basic_type != CHAR){
			return NULL;
		}

		//Otherwise it's fine so
		return typeA;
	}
	
	
	//Generic fail case if we forgot something
	return NULL;
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
		type->type_size = 1;
	} else if(basic_type == S_INT16 || basic_type == U_INT16){
		//2 BYTES
		type->type_size = 2;
	} else if(basic_type == U_INT32 || basic_type == S_INT32 || basic_type == FLOAT32){
		//4 BYTES
		type->type_size = 4;
	} else if(basic_type == VOID){
		//0 BYTES - special case
		type->type_size = 0;
	} else {
		//Otheriwse is 8 BYTES
		type->type_size = 8;
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
	type->type_size = 8;

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

	/**
	 * Array type sizes are always guaranteed to be 16-byte aligned for speed's sake
	 */
	u_int32_t type_size = ((points_to->type_size * num_members) + 15) & -16;

	//Store this in here
	type->type_size = type_size;

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
 * This function will completely strip away any aliasing and return the raw type 
 * that we have underneath
 */
generic_type_t* dealias_type(generic_type_t* type){
	//Grab a cursor of sorts
	generic_type_t* raw_type = type;

	//So long as we keep having an alias
	while(raw_type->type_class == TYPE_CLASS_ALIAS){
		raw_type = raw_type->aliased_type->aliased_type;
	}

	//Give the stripped down type back
	return raw_type;
}

/**
 * Provide a way of destroying a type variable easily
*/
void type_dealloc(generic_type_t* type){
	//We'll take action based on what kind of type it is
	if(type->type_class == TYPE_CLASS_BASIC){
		free(type->basic_type);
	} else if (type->type_class == TYPE_CLASS_ALIAS){
		free(type->aliased_type);
	} else if (type->type_class == TYPE_CLASS_ARRAY){
		free(type->array_type);
	} else if(type->type_class == TYPE_CLASS_POINTER){
		free(type->pointer_type);
	} else if(type->type_class == TYPE_CLASS_CONSTRUCT){
		free(type->construct_type);
	} else if(type->type_class == TYPE_CLASS_ENUMERATED){
		free(type->enumerated_type);
	}

	//Finally just free the overall pointer
	free(type);
}
