/**
 * The implementation file for the type_system.h header file
*/

#include "type_system.h"
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
//Link to the symtab for variable storage
#include "../symtab/symtab.h"

//For standardization across modules
#define SUCCESS 1
#define FAILURE 0
#define TRUE 1
#define FALSE 0 


/**
 * Is a type an unsigned 64 bit type? This is used for type conversions in 
 * the instruction selector
 */
u_int8_t is_type_unsigned_64_bit(generic_type_t* type){
	//Switch based on the class
	switch(type->type_class){
		//These are memory addresses - so yes
		case TYPE_CLASS_POINTER:
		case TYPE_CLASS_ARRAY:
		case TYPE_CLASS_CONSTRUCT:
			return TRUE;

		//Let's see what we have here
		case TYPE_CLASS_BASIC:
			//If it's a u64 then yes
			if(type->basic_type->basic_type == U_INT64){
				return TRUE;
			}

			return FALSE; 

		//By default fail out
		default:
			return FALSE;
	}
}


/**
 * Is the given type a 32 bit integer type?
 */
u_int8_t is_type_32_bit_int(generic_type_t* type){
	//If it's not a basic type we're done
	if(type->type_class != TYPE_CLASS_BASIC){
		return FALSE;
	}

	//Otherwise it is a basic type
	switch(type->basic_type->basic_type){
		//Our only real cases here
		case U_INT32:
		case S_INT32:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Get the referenced type regardless of how many indirection levels there are
 */
generic_type_t* get_referenced_type(generic_type_t* starting_type, u_int16_t indirection_level){
	//Here's the current type that we have. Initially it is the start type
	generic_type_t* current_type = starting_type;

	//We need to repeat this process for however many indirections that we have
	for(u_int16_t i = 0; i < indirection_level; i++){
		switch (current_type->type_class) {
			//This is really all we should have here
			case TYPE_CLASS_ARRAY:
				current_type = current_type->array_type->member_type;
				break;
			case TYPE_CLASS_POINTER:
				current_type = current_type->pointer_type->points_to;
				break;
			//Nothing for us here
			default:
				break;
		}
	}

	//Give back the current type
	return current_type;
}


/**
 * Are two types equivalent(as in, the exact same)
 */
static u_int8_t types_equivalent(generic_type_t* typeA, generic_type_t* typeB){
	//Make sure that both of these types are raw
	typeA = dealias_type(typeA);
	typeB = dealias_type(typeB);

	//If they are not in the same type class, then they are not equivalent
	if(typeA->type_class != typeB->type_class){
		return FALSE;
	}

	//If these are both arrays
	if(typeA->type_class == TYPE_CLASS_ARRAY
		&& typeA->array_type->num_members != typeB->array_type->num_members){
		//We can disqualify quickly if this happens
		return FALSE;
	}

	//Now that we know they are in the same class, we need to check if they're the exact same
	//If they are the exact same, return 1. Otherwise, return 0
	if(strcmp(typeA->type_name.string, typeB->type_name.string) == 0){
		return TRUE;
	}

	//Otherwise they aren't the exact same, so
	return FALSE;
}


/**
 * Is the given type memory movement appropriate
 */
u_int8_t is_type_address_calculation_compatible(generic_type_t* type){
	//The basic type token for later
	Token basic_type;

	//Arrays and pointers are fine
	switch(type->type_class){
		//These are all essentially pointers
		case TYPE_CLASS_ARRAY:
		case TYPE_CLASS_POINTER:
		case TYPE_CLASS_CONSTRUCT:
			return TRUE;

		//Some more exploration needed here
		case TYPE_CLASS_BASIC:
			//Extract this
			basic_type = type->basic_type->basic_type;
		
			//We're allowed to see 64 bit types here
			if(basic_type == U_INT64 || basic_type == S_INT64){
				return TRUE;
			}

			//Otherwise fail out
			return FALSE;

		//Default is a no
		default:
			return FALSE;
	}
}


/**
 * Is this type valid for memory addressing. Specifically, can this
 * be used as the index to an array
 */
u_int8_t is_type_valid_for_memory_addressing(generic_type_t* type){
	//First we dealias just to make sure
	type = dealias_type(type);

	//The basic type token
	Token basic_type_token;

	//Switch based on the type to determine
	switch(type->type_class){
		case TYPE_CLASS_ARRAY:
		case TYPE_CLASS_CONSTRUCT:
		case TYPE_CLASS_POINTER:
			return FALSE;
		case TYPE_CLASS_ENUMERATED:
			return TRUE;
		case TYPE_CLASS_BASIC:
			//Grab this out
			basic_type_token = type->basic_type->basic_type;

			//We just can't see floats or void here
			if(basic_type_token == VOID || basic_type_token == FLOAT32
				|| basic_type_token == FLOAT64){
				return FALSE;
			}

			//Otherwise we're good
			return TRUE;

		//By default we can't
		default:
			return FALSE;
	}

}


/**
 * Is the type valid to be used in a conditional?
 */
u_int8_t is_type_valid_for_conditional(generic_type_t* type){
	//Ensure it's not aliased
	type = dealias_type(type);

	//Switch based on the type to determine this
	switch(type->type_class){
		case TYPE_CLASS_ARRAY:
			return FALSE;
		case TYPE_CLASS_CONSTRUCT:
			return FALSE;
		case TYPE_CLASS_POINTER:
			return TRUE;
		case TYPE_CLASS_ENUMERATED:
			return TRUE;
		case TYPE_CLASS_BASIC:
			//If it's void, then we can't have it. Otherwise
			//it's fine
			if(type->basic_type->basic_type != VOID){
				return TRUE;
			} else {
				return FALSE;
			}
		default:
			return FALSE;
	}
}

/**
 * Is a type conversion needed between these two types for b to fit into a
 */
u_int8_t is_type_conversion_needed(generic_type_t* a, generic_type_t* b){
	//If the two types are the exact same, nothing is needed
	if(a == b){
		return FALSE;
	}

	//If they're both basic types
	if(a->type_class == TYPE_CLASS_BASIC && b->type_class == TYPE_CLASS_BASIC){
		//If their float status is the same and they're the same size, no conversion is needed
		if(a->type_size == b->type_size){
			return FALSE;
		} else {
			return TRUE;
		}
	}

	return TRUE;
}



/**
 * Can two types be assigned to one another? This rule will perform implicit conversions
 * if need be to make types assignable. We are always assigning source to destination. Widening
 * type conversions will be applied to source if need be. We cannot apply widening type conversions
 * to destination
 *
 * In general, the destination type always wins. This function is just a one-stop-shop for all validations
 * regarding assignments from one type to another
 *
 * CASES:
 * 1.) Construct Types: construct types must be the exact same in order to assign one from the other
 * 2.) Enumerated Types: Internally, enums are just u8's. As such, if the destination is an enumerated type, we
 * can assign other enums of the same type and integers
 * 3.) Array Types: You can never assign to an array type, this is always false
 * 4.) Pointer Types: Pointers can be assigned values of type U64.
 * 					  Void pointers can be assigned to anything
 * 					  Any other pointer can be assigned a void pointer
 * 					  Beyond this, the "pointing_to" types have to match when dealiased
 * 5.) Basic Types: See the area below for these rules, there are many
 */
generic_type_t* types_assignable(generic_type_t** destination_type, generic_type_t** source_type){
	//Before we go any further - make sure these types are fully raw
	generic_type_t* deref_destination_type = dealias_type(*destination_type);
	generic_type_t* deref_source_type = dealias_type(*source_type);

	//Predeclare these for now
	Token source_basic_type;
	Token dest_basic_type;

	switch(deref_destination_type->type_class){
		//This is a simpler case - constructs can only be assigned
		//if they're the exact same
		case TYPE_CLASS_CONSTRUCT:
			//Not assignable at all
			if(deref_source_type->type_class != TYPE_CLASS_CONSTRUCT){
				return NULL;
			}

			//Now let's check to see if they're the exact same type
			if(strcmp(deref_source_type->type_name.string, deref_source_type->type_name.string) != 0){
				return NULL;
			} else {
				//We'll give back the destination type if they are the same
				return deref_destination_type;
			}

		//Enumerated types are internally a u8
		case TYPE_CLASS_ENUMERATED:
			//If we have an enumerated type here as well
			if(deref_destination_type->type_class == TYPE_CLASS_ENUMERATED){
				//These need to be the exact same, otherwise this will not work
				if(strcmp(deref_source_type->type_name.string, deref_destination_type->type_name.string) == 0){
					return deref_destination_type;
				} else {
					return NULL;
				}

			//Otherwise it needs to be a basic type
			} else if(deref_source_type->type_class == TYPE_CLASS_BASIC){
				//Grab the type out of here
				source_basic_type = deref_source_type->basic_type->basic_type;

				//It needs to be 8 bits, otherwise we won't allow this
				if(source_basic_type == U_INT8 || source_basic_type == S_INT8 || source_basic_type == CHAR){
					//This is assignable
					return deref_destination_type;
				} else {
					//It's not assignable
					return NULL;
				}

			//This isn't going to work otherewise
			} else {
				return NULL;
			}

		//Arrays are not assignable at all - this one is easy
		case TYPE_CLASS_ARRAY:
			return NULL;

		//Refer to the rules above for details
		case TYPE_CLASS_POINTER:
			//If we have a basic type
			if(deref_source_type->type_class == TYPE_CLASS_BASIC){
				//This needs to be a u64, otherwise it's invalid
				if(deref_source_type->basic_type->basic_type == U_INT64){
					//We will keep this as the pointer
					return deref_destination_type;
				//Any other basic type will not work here
				} else {
					return NULL;
				}

			//If we have an array type, then the values that these two point
			//to must be the exact same
			} else if(deref_source_type->type_class == TYPE_CLASS_ARRAY){
				//If these are the exact same types, then we're set
				if(types_equivalent(deref_destination_type->pointer_type->points_to, deref_source_type->array_type->member_type) == TRUE){
					return deref_destination_type;
				//Otherwise this won't work at all
				} else{
					return NULL;
				}

			//This is the most interesting case that we have...pointers being assigned to eachother
			} else if(deref_source_type->type_class == TYPE_CLASS_POINTER){
				//If this itself is a void pointer, then we're good
				if(deref_source_type->pointer_type->is_void_pointer == TRUE){
					return deref_destination_type;
				//This is also fine, we just give the destination type back
				} else if(deref_destination_type->pointer_type->is_void_pointer == TRUE){
					return deref_destination_type;
				//Let's see if what they point to is the exact same
				} else {
					//They need to be the exact same
					if(types_equivalent(deref_source_type->pointer_type->points_to, deref_destination_type->pointer_type->points_to) == TRUE){
						return deref_destination_type;
					} else {
						return NULL;
					}
				}
				
			//We've exhausted all options, this is a no
			} else {
				return NULL;
			}
	
		/**
		 * The basic type class is the most interesting scenario. We have a great many rules to follow here
		 *
		 * 1.) VOID: nothing can be assigned to void. Additionally, nothing can be assigned as void
		 * 2.) F64: can be assigned anything of type F64 or F32
		 * 3.) F32: can be assigned anything of type F32
		 * 4.) Source type as enum: any integer/char can take in an enum type
		 * 5.) Integers: so long as the size of the destination is >= the size of the source,
		 *    integers can be assigned around. We will not stop the user from assigning signed to
		 *    unsigned and vice versa
		 */
		case TYPE_CLASS_BASIC:
			//Extract the destination's basic type
			dest_basic_type = deref_destination_type->basic_type->basic_type;

			//If this is the case, we're done. Nothing can be assigned to void
			if(dest_basic_type == VOID){
				return NULL;

			//Float64's can only be assigned other float64's
			} else if(dest_basic_type == FLOAT64){
				//We must see another f64 or an f32(widening) here
				if(deref_source_type->type_class == TYPE_CLASS_BASIC
					&& (deref_source_type->basic_type->basic_type == FLOAT64
					|| deref_source_type->basic_type->basic_type == FLOAT32)){
					return deref_destination_type;

				//Otherwise nothing here will work
				} else {
					return NULL;
				}
			
			//Let's see if we have an f32
			} else if(dest_basic_type == FLOAT32){
				//We must see another an f32 here
				if(deref_source_type->type_class == TYPE_CLASS_BASIC
					&& deref_source_type->basic_type->basic_type == FLOAT32){
					return deref_destination_type;

				//Otherwise nothing here will work
				} else {
					return NULL;
				}

			//Once we get to this point, we know that we have something
			//in this set for destination type: U64, I64, U32, I32, U16, I16, U8, I8, Char
			//From here, we'll go based on the type size of the source type *if* the source
			//type is also a basic type. 
			} else {
				//Special exception - the source type is an enum. These are good to be used with ints
				if(deref_source_type->type_class == TYPE_CLASS_ENUMERATED){
					return deref_destination_type;
				}

				//Now if the source type is not a basic type, we're done here
				if(deref_source_type->type_class != TYPE_CLASS_BASIC){
					return NULL;
				}

				//Once we get here, we know that the source type is a basic type. We now
				//need to check that it's not a float or void
				source_basic_type = deref_source_type->basic_type->basic_type;

				//All of these cannot be assigned to an int
				if(source_basic_type == FLOAT32 || source_basic_type == FLOAT64 || source_basic_type == VOID){
					return NULL;
				}

				//These are generic types here - they will always work no matter what
				if(source_basic_type == UNSIGNED_INT_CONST || source_basic_type == SIGNED_INT_CONST){
					//Reassign source type to be whatever this destination ends up being
					*source_type = deref_destination_type;
					return deref_destination_type;
				}

				//Otherwise, once we make it here we know that the source type is a basic type and
				//and integer/char type. We can now just compare the sizes and if the destination is more
				//than or equal to the source, we're good
				if(deref_source_type->type_size <= deref_destination_type->type_size){
					return deref_destination_type;
				} else {
					//These wouldn't fit
					return NULL;
				}
			}

		//We should never get here
		default:
			return NULL;
	}
}


/**
 * Convert a given basic type to the unsigned version of itself. We will *not*
 * perform any size manipulation here
 *
 * We'll need this because we always coerce to unsigned, *not* to signed,
 * if one operand in a certain equation is unsigned
 */
static generic_type_t* convert_to_unsigned_version(type_symtab_t* symtab, basic_type_t* type){
	//Switch based on what we have
	switch(type->basic_type){
			//Char is already unsigned
		case CHAR:
			return lookup_type_name_only(symtab, "char")->type;
		case U_INT8:
		case S_INT8:
			return lookup_type_name_only(symtab, "u8")->type;
		case U_INT16:
		case S_INT16:
			return lookup_type_name_only(symtab, "u16")->type;
		case U_INT32:
		case S_INT32:
			return lookup_type_name_only(symtab, "u32")->type;
		case U_INT64:
		case S_INT64:
			return lookup_type_name_only(symtab, "u64")->type;
		case SIGNED_INT_CONST:
			return lookup_type_name_only(symtab, "generic_unsigned_int")->type;
		//We should never get here
		default:
			return lookup_type_name_only(symtab, "u32")->type;
	}
}


/**
 * Apply signedness coercion for basic types a and b
 *
 * Signedness coercion *always* comes first before widening conversions
 */
static void basic_type_signedness_coercion(type_symtab_t* symtab, generic_type_t** a, generic_type_t** b){
	//Floats are never not signed, so this is useless for them
	if((*a)->basic_type->basic_type == FLOAT32 || (*a)->basic_type->basic_type == FLOAT64){
		return;
	}

	//If a is unsigned, b must automatically go to unsigned
	if(is_type_signed(*a) == FALSE){
		//Convert b
		*b = convert_to_unsigned_version(symtab, (*b)->basic_type);
		return;
	}

	//Likewise, if b is unsigned, then a must automatically go to unsigned
	if(is_type_signed(*b) == FALSE){
		//Convert a
		*a = convert_to_unsigned_version(symtab, (*a)->basic_type);
		return;
	}
}


/**
 * Apply standard coercion rules for basic types
 */
static void basic_type_widening_type_coercion(type_symtab_t* type_symtab, generic_type_t** a, generic_type_t** b){
	//Grab these to avoid unneeded derefs
	Token a_basic_type = (*a)->basic_type->basic_type;
	Token b_basic_type = (*b)->basic_type->basic_type;

	//These are our "flexible types" -- meaning that they can become any other
	//kind of integer that we want
	if(a_basic_type == SIGNED_INT_CONST || a_basic_type == UNSIGNED_INT_CONST){
		//If B is not one of these, we'll just make A whatever B is
		if(b_basic_type != SIGNED_INT_CONST && b_basic_type != UNSIGNED_INT_CONST){
			*a = *b;
			return;
		}

		//The final type
		generic_type_t* final_type;

		//Otherwise b is one of these, so we'll just assign them both to be 
		//the 32-bit(default) version of whatever they are
		if(a_basic_type == SIGNED_INT_CONST){
			final_type = lookup_type_name_only(type_symtab, "i32")->type;
		} else {
			final_type = lookup_type_name_only(type_symtab, "u32")->type;
		}

		//These are both now this final type
		*a = final_type;
		*b = final_type;

		return;

	//Make sure b isn't a flexible type either
	} else if(b_basic_type == SIGNED_INT_CONST || b_basic_type == UNSIGNED_INT_CONST){
		//If B is not one of these, we'll just make A whatever B is
		if(a_basic_type != SIGNED_INT_CONST && a_basic_type != UNSIGNED_INT_CONST){
			*b = *a;
			return;
		}

		//The final type
		generic_type_t* final_type;

		//Otherwise b is one of these, so we'll just assign them both to be 
		//the 32-bit(default) version of whatever they are
		if(a_basic_type == SIGNED_INT_CONST){
			final_type = lookup_type_name_only(type_symtab, "i32")->type;
		} else {
			final_type = lookup_type_name_only(type_symtab, "u32")->type;
		}

		//These are both now this final type
		*a = final_type;
		*b = final_type;

		return;
	}

	//Whomever has the largest size wins
	if((*a)->type_size > (*b)->type_size){
		//Set b to equal a
		*b = *a;
	} else if((*a)->type_size < (*b)->type_size){
		//Set a to equal b
		*a = *b;
	}
	//No else case - we don't want to deal with any other type size coercions
}


/**
 * We'll always go from integers to floating points, if there is at least one float in the
 * operation
 *
 * Go from an integer to a floating point number
 */
static void integer_to_floating_point(type_symtab_t* symtab, generic_type_t** a){
	//Go based on what we have as our basic type
	switch((*a)->basic_type->basic_type){
		//These all decome f32's
		case U_INT8:
		case S_INT8:
		case CHAR:
		case SIGNED_INT_CONST:
		case UNSIGNED_INT_CONST:
		case U_INT16:
		case S_INT16:
		case U_INT32:
		case S_INT32:
			*a = lookup_type_name_only(symtab, "f32")->type;

		//These become f64's
		case U_INT64:
		case S_INT64:
			*a = lookup_type_name_only(symtab, "f64")->type;
		default:
			return;
	}
}


/**
 * Are two types compatible with one another for a given operator? Note that by the time 
 * we get here, we guarantee that the types themselves on their own are valid for this operator.
 * The question then becomes are they valid together
 *
 * If the types are not compatible, we'll return a NULL. If they are compatible, we will coerce the
 * types appropriately for size/signedness constraints and return the type that they were both
 * coerced into
 *
 * CASES:
 * 	1.) Construct Types: Construct types are compatible if they are both the exact same type
 */
generic_type_t* determine_compatibility_and_coerce(void* symtab, generic_type_t** a, generic_type_t** b, Token op){
	//For convenience
	symtab = (type_symtab_t*)symtab;

	//Before we go any further - make sure these types are fully raw(They should be anyways, but insurance never hurts)
	*a = dealias_type(*a);
	*b = dealias_type(*b);

	//All enumerated types are in reality u8's
	if((*a)->type_class == TYPE_CLASS_ENUMERATED){
		*a = lookup_type_name_only(symtab, "u8")->type;
	}

	//All enumerated types are in reality u8's
	if((*b)->type_class == TYPE_CLASS_ENUMERATED){
		*b = lookup_type_name_only(symtab, "u8")->type;
	}
	
	/**
	 * We'll go through based on the operator and see what we can get out
	 */
	switch(op){
		/**
		 * Addition/subtraction is valid for integers and pointers. For 
		 * addition/subtraction with pointers, special detail is required and
		 * we will actually not coerce in here specifically
		 */
		case PLUS:
		case MINUS:
			//If a is a pointer type
			if((*a)->type_class == TYPE_CLASS_POINTER){
				//It is invalid to add two pointers
				if((*b)->type_class == TYPE_CLASS_POINTER){
					//This is invalid
					return NULL;
				}

				//If this is not a basic type, all other conversion is bad
				if((*b)->type_class != TYPE_CLASS_BASIC){
					return NULL;
				}

				//Now once we get here, we know that we have a basic type

				//Pointers are not compatible with floats in a comparison sense
				if((*b)->basic_type->basic_type == FLOAT32 || (*b)->basic_type->basic_type == FLOAT64){
					return NULL;
				}

				//If we get here, we know that B is valid for this. We will now expand it to be of type u64
				*b = lookup_type_name_only(symtab, "u64")->type;

				//Give back the u64 type as the result
				return *b;
			}
			
			//If b is a pointer type. This is teh exact same scenario as a
			if((*b)->type_class == TYPE_CLASS_POINTER){
				//It is invalid to add two pointers
				if((*a)->type_class == TYPE_CLASS_POINTER){
					//This is invalid
					return NULL;
				}

				//If this is not a basic type, all other conversion is bad
				if((*a)->type_class != TYPE_CLASS_BASIC){
					return NULL;
				}

				//Now once we get here, we know that we have a basic type

				//Pointers are not compatible with floats in a comparison sense
				if((*a)->basic_type->basic_type == FLOAT32 || (*a)->basic_type->basic_type == FLOAT64){
					return NULL;
				}

				//If we get here, we know that B is valid for this. We will now expand it to be of type u64
				*a = lookup_type_name_only(symtab, "u64")->type;

				//Give back the u64 type as the result
				return *a;
			}

			//At this point if these are not basic types, we're done
			if((*a)->type_class != TYPE_CLASS_BASIC || (*b)->type_class != TYPE_CLASS_BASIC){
				return NULL;
			}

			//If a is a floating point, we apply the float conversion to b
			if((*a)->basic_type->basic_type == FLOAT32 || (*a)->basic_type->basic_type == FLOAT64){
				integer_to_floating_point(symtab, b);

			//If b is a floating point, we apply the float conversion to b
			} else if((*b)->basic_type->basic_type == FLOAT32 || (*b)->basic_type->basic_type == FLOAT64){
				integer_to_floating_point(symtab, a);
			}

			//Perform any signedness correction that is needed
			basic_type_signedness_coercion(symtab, a, b);

			//We already know that these are basic types only here. We can
			//apply the standard widening type coercion
			basic_type_widening_type_coercion(symtab, a, b);

			//Give back a
			return *a;

		//These two rules are valid for integers and pointers
		case DOUBLE_AND:
		case DOUBLE_OR:
			//If a is a pointer type
			if((*a)->type_class == TYPE_CLASS_POINTER){
				//If b is a another pointer, then that's fine
				if((*b)->type_class == TYPE_CLASS_POINTER){
					//We'll return a final comparison type of u64
					return lookup_type_name_only(symtab, "u64")->type;
				}

				//If this is not a basic type, all other conversion is bad
				if((*b)->type_class != TYPE_CLASS_BASIC){
					return NULL;
				}

				//Now once we get here, we know that we have a basic type

				//Pointers are not compatible with floats in a comparison sense
				if((*b)->basic_type->basic_type == FLOAT32 || (*b)->basic_type->basic_type == FLOAT64){
					return NULL;
				}

				//If we get here, we know that B is valid for this. We will now expand it to be of type u64
				*b = lookup_type_name_only(symtab, "u64")->type;

				//Give back the u64 type as the result
				return *b;
			}
			
			//If b is a pointer type. This is teh exact same scenario as a
			if((*b)->type_class == TYPE_CLASS_POINTER){
				//If b is a another pointer, then that's fine
				if((*a)->type_class == TYPE_CLASS_POINTER){
					//We'll return a final comparison type of u64
					return lookup_type_name_only(symtab, "u64")->type;
				}

				//If this is not a basic type, all other conversion is bad
				if((*a)->type_class != TYPE_CLASS_BASIC){
					return NULL;
				}

				//Now once we get here, we know that we have a basic type

				//Pointers are not compatible with floats in a comparison sense
				if((*a)->basic_type->basic_type == FLOAT32 || (*a)->basic_type->basic_type == FLOAT64){
					return NULL;
				}

				//If we get here, we know that B is valid for this. We will now expand it to be of type u64
				*a = lookup_type_name_only(symtab, "u64")->type;

				//Give back the u64 type as the result
				return *a;
			}

			//At this point if these are not basic types, we're done
			if((*a)->type_class != TYPE_CLASS_BASIC || (*b)->type_class != TYPE_CLASS_BASIC){
				return NULL;
			}

			/**
			 * We will not perform any signedness conversion on the two of these, since in the
			 * end we will be using flags anyways. We will only perform the widening conversion
			 */

			//We already know that these are basic types only here. We can
			//apply the standard widening type coercion
			basic_type_widening_type_coercion(symtab, a, b);

			//Give back a
			return *a;

		/**
		 * Modulus types only have integers to worry about. As always, we will
		 * apply the standard widening/signed type coersion here
		 *
		 * NOTE: We know for a fact that modulus only works on basic types that are integers *and*
		 * enumerations
		 */
		case L_SHIFT:
		case R_SHIFT:
		case SINGLE_AND:
		case SINGLE_OR:
		case L_BRACKET: //Array access
		case CARROT:
			//We always apply the signedness coercion first
			basic_type_signedness_coercion(symtab, a, b);

			//We already know that these are basic types only here. We can
			//apply the standard widening type coercion
			basic_type_widening_type_coercion(symtab, a, b);
		
			//Give this back once down
			return *a;

		/**
		 * Division and multiplication are valid for integers and floating point numbers
		 *
		 * We will first aply the floating point conversion here if we need to. Following that, 
		 * we will apply any needed signedness coercion and any needed widening coercion
		 */
		case F_SLASH:
		case STAR:
		case MOD:
			//If a is a floating point, we apply the float conversion to b
			if((*a)->basic_type->basic_type == FLOAT32 || (*a)->basic_type->basic_type == FLOAT64){
				integer_to_floating_point(symtab, b);

			//If b is a floating point, we apply the float conversion to b
			} else if((*b)->basic_type->basic_type == FLOAT32 || (*b)->basic_type->basic_type == FLOAT64){
				integer_to_floating_point(symtab, a);
			}

			//Perform any signedness correction that is needed
			basic_type_signedness_coercion(symtab, a, b);

			//We already know that we only have basic types here. We can apply
			//the standard widening conversion
			basic_type_widening_type_coercion(symtab, a, b);

			//We'll give back *a once we're finished
			return *a;

		/**
		 * Relational operators will apply normal conversion rules. If we have
		 * a pointer, we will coerce the other integer to a u64
		 */
		case QUESTION:
		case G_THAN:
		case G_THAN_OR_EQ:
		case L_THAN:
		case L_THAN_OR_EQ:
		case DOUBLE_EQUALS:
		case NOT_EQUALS:
			//If a is a pointer type
			if((*a)->type_class == TYPE_CLASS_POINTER){
				//If b is a another pointer, then that's fine
				if((*b)->type_class == TYPE_CLASS_POINTER){
					//We'll return a final comparison type of u64
					return lookup_type_name_only(symtab, "u64")->type;
				}

				//If this is not a basic type, all other conversion is bad
				if((*b)->type_class != TYPE_CLASS_BASIC){
					return NULL;
				}

				//Now once we get here, we know that we have a basic type

				//Pointers are not compatible with floats in a comparison sense
				if((*b)->basic_type->basic_type == FLOAT32 || (*b)->basic_type->basic_type == FLOAT64){
					return NULL;
				}

				//If we get here, we know that B is valid for this. We will now expand it to be of type u64
				*b = lookup_type_name_only(symtab, "u64")->type;

				//Give back the u64 type as the result
				return *b;
			}
			
			//If b is a pointer type. This is teh exact same scenario as a
			if((*b)->type_class == TYPE_CLASS_POINTER){
				//If b is a another pointer, then that's fine
				if((*a)->type_class == TYPE_CLASS_POINTER){
					//We'll return a final comparison type of u64
					return lookup_type_name_only(symtab, "u64")->type;
				}

				//If this is not a basic type, all other conversion is bad
				if((*a)->type_class != TYPE_CLASS_BASIC){
					return NULL;
				}

				//Now once we get here, we know that we have a basic type

				//Pointers are not compatible with floats in a comparison sense
				if((*a)->basic_type->basic_type == FLOAT32 || (*a)->basic_type->basic_type == FLOAT64){
					return NULL;
				}

				//If we get here, we know that B is valid for this. We will now expand it to be of type u64
				*a = lookup_type_name_only(symtab, "u64")->type;

				//Give back the u64 type as the result
				return *a;
			}

			//At this point if these are not basic types, we're done
			if((*a)->type_class != TYPE_CLASS_BASIC || (*b)->type_class != TYPE_CLASS_BASIC){
				return NULL;
			}

			//If a is a floating point, we apply the float conversion to b
			if((*a)->basic_type->basic_type == FLOAT32 || (*a)->basic_type->basic_type == FLOAT64){
				integer_to_floating_point(symtab, b);

			//If b is a floating point, we apply the float conversion to b
			} else if((*b)->basic_type->basic_type == FLOAT32 || (*b)->basic_type->basic_type == FLOAT64){
				integer_to_floating_point(symtab, a);
			}
		
			//Perform any signedness correction that is needed
			basic_type_signedness_coercion(symtab, a, b);

			//We already know that we only have basic types here. We can apply
			//the standard widening conversion
			basic_type_widening_type_coercion(symtab, a, b);

			//We'll give back *a once we're finished
			return *a;

		default:
			return NULL;
	}
}


/**
 * Is the given unary operation valid for the type that was specificed?
 */
u_int8_t is_unary_operation_valid_for_type(generic_type_t* type, Token unary_op){
	//Just to be safe, we'll dealias is
	type = dealias_type(type);

	//Go based on what token we're given
	switch (unary_op) {
		//This will pull double duty for pre/post increment operators
		case PLUSPLUS:
		case MINUSMINUS:
			//This is invalid for construct types
			if(type->type_class == TYPE_CLASS_ARRAY || type->type_class == TYPE_CLASS_CONSTRUCT){
				return FALSE;
			}

			//It's also invalid for void types
			if(type->type_class == TYPE_CLASS_BASIC && type->basic_type->basic_type == VOID){
				return FALSE;
			}

			//Otherwise, it's completely fine
			return TRUE;

		//We can only dereference arrays and pointers
		case STAR:
			//These are our valid cases
			if(type->type_class == TYPE_CLASS_ARRAY || type->type_class == TYPE_CLASS_POINTER){
				return TRUE;
			}

			//Anything else is invalid
			return FALSE;

		//We can take the address of anything besides a void type
		case SINGLE_AND:
			//This is our only invalid case
			if(type->type_class == TYPE_CLASS_BASIC && type->basic_type->basic_type == VOID){
				return FALSE;
			}

			//Otherwise it's fine
			return TRUE;

		//We can only negate basic types that are not void
		case MINUS:
			//This is an instant failure
			if(type->type_class != TYPE_CLASS_BASIC){
				return FALSE;
			}

			//This is the only other way we'd fail
			if(type->basic_type->basic_type == VOID){
				return FALSE;
			}

			//Otherwise we get true
			return TRUE;

		//We can negate pointers, enums and basic types that are not void
		case L_NOT:
			//These are bad, we fail out here
			if(type->type_class == TYPE_CLASS_CONSTRUCT || type->type_class == TYPE_CLASS_ARRAY){
				return FALSE;
			}

			//Our other invalid case
			if(type->type_class == TYPE_CLASS_BASIC && type->basic_type->basic_type == VOID){
				return FALSE;
			}

			//Otherwise if we make it here, we know it's fine
			return TRUE;

		//Bitwise not expressions are only valid for integers
		case B_NOT:
			//If it's not basic, we're out of here
			if(type->type_class != TYPE_CLASS_BASIC){
				return FALSE;
			}

			//Now that we know what it is, we'll see if it's a float or void
			Token type_tok = type->basic_type->basic_type;
			
			//If it's float or void, we're done
			if(type_tok == FLOAT32 || type_tok == FLOAT64 || type_tok == VOID){
				return FALSE;
			}

			//Otherwise we are in the clear
			return TRUE;

		//We really shouldn't get here
		default:
			return FALSE;
	}
}


/**
 * Is the given operation valid for the type that was specificed?
 */
u_int8_t is_binary_operation_valid_for_type(generic_type_t* type, Token binary_op, side_type_t side){
	//Just to be safe, we'll always make sure here
	type = dealias_type(type);

	//Deconstructed basic type(since we'll be using it so much)
	basic_type_t* basic_type = NULL;

	//Switch based on what the operator is
	switch(binary_op){
		/**
		 * Shifting and modulus operators are valid only for integers
		 */
		case L_SHIFT:
		case R_SHIFT:
		case SINGLE_AND: //Bitwise and(&)
		case SINGLE_OR: //Bitwise or(|)
		case CARROT: //Exclusive or(^)
		case MOD:
			//Enumerated types are fine here, the one non-basic type that works
			if(type->type_class == TYPE_CLASS_ENUMERATED){
				return TRUE;
			}

			//If it's not a basic type we're done
			if(type->type_class != TYPE_CLASS_BASIC){
				return FALSE;
			}

			//Deconstruct this
			basic_type = type->basic_type;

			//Let's now check and make sure it's not a float or void
			if(basic_type->basic_type == VOID || basic_type->basic_type == FLOAT32 || basic_type->basic_type == FLOAT64){
				return FALSE;
			}

			//Otherwise if we make it all the way down here, this is fine
			return TRUE;

		/**
		 * The multiplication and division operators are valid for enums and all basic types with the exception of void
		 */
		case STAR:
		case F_SLASH:
			//Enumerated types are fine here, the one non-basic type that works
			if(type->type_class == TYPE_CLASS_ENUMERATED){
				return TRUE;
			}

			//If it's not a basic type we're done
			if(type->type_class != TYPE_CLASS_BASIC){
				return FALSE;
			}

			//Deconstruct this
			basic_type = type->basic_type;

			//Let's now just make sure that it is not a void type
			if(basic_type->basic_type == VOID){
				return FALSE;
			}

			//Otherwise if we make it all the way down here, this is fine
			return TRUE;


		/**
		 * Double or and double and are valid for pointers, enums, and
		 * all basic types with the exception of void
		 */
		case DOUBLE_OR:
		case DOUBLE_AND:
			//Enumerated types are fine here
			if(type->type_class == TYPE_CLASS_ENUMERATED){
				return TRUE;
			}

			//Pointers are also no issue
			if(type->type_class == TYPE_CLASS_POINTER){
				return TRUE;
			}

			//Otherwise if it's not a basic type by the time we get
			//here then we're done
			if(type->type_class != TYPE_CLASS_BASIC){
				return FALSE;
			}

			//Deconstruct this
			basic_type = type->basic_type;

			//Let's now just make sure that it is not a void type
			if(basic_type->basic_type == VOID){
				return FALSE;
			}

			//Otherwise if we make it all the way down here, this is fine
			return TRUE;

		/**
		 * Relational expressions are valid for floats, integers,
		 * enumerated types and pointers. They are invalid for
		 * void types
		 *
		 * Addition is also valid for floats, integers, enumerated
		 * types and pointers
		 */
		case L_THAN:
		case L_THAN_OR_EQ:
		case G_THAN:
		case G_THAN_OR_EQ:
		case NOT_EQUALS:
		case DOUBLE_EQUALS:
		case PLUS:
			//This doesn't work on arrays or constructs
			if(type->type_class == TYPE_CLASS_ARRAY || type->type_class == TYPE_CLASS_CONSTRUCT){
				return FALSE;
			}

			//This also doesn't work for void types
			if(type->type_class == TYPE_CLASS_BASIC && type->basic_type->basic_type == VOID){
				return FALSE;
			}

			//Otherwise, everything else that we have should work fine
			return TRUE;

		/**
		 * Subtraction is valid for floats, integers and enumerated types
		 *
		 * It is valid for pointers *only* if the pointer is on the left side
		 * i.e. int* - int is good
		 * 		int - int* is not good
		 */
		case MINUS:
			//This doesn't work on arrays or constructs
			if(type->type_class == TYPE_CLASS_ARRAY || type->type_class == TYPE_CLASS_CONSTRUCT){
				return FALSE;
			}

			//This also doesn't work for void types
			if(type->type_class == TYPE_CLASS_BASIC && type->basic_type->basic_type == VOID){
				return FALSE;
			}

			//If it's a pointer and it's not on the left side, it's bad
			if(type->type_class == TYPE_CLASS_POINTER && side != SIDE_TYPE_LEFT){
				return FALSE;
			}

			//Otherwise, everything else that we have should work fine
			return TRUE;

		default:
			return FALSE;
	}

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
	
	//Create and allocate the name
	dynamic_string_t name;
	dynamic_string_alloc(&name);

	//Set it to be our given name
	dynamic_string_set(&name, type_name);

	//Set the name 
	type->type_name = name;

	//Assign the type in
	type->basic_type->basic_type = basic_type;

	//Now we can immediately determine the size based on what the actual type is
	switch(basic_type){
		case CHAR:
		case S_INT8:
		case U_INT8:
			//1 BYTE
			type->type_size = 1;
			break;
		case S_INT16:
		case U_INT16:
			//2 BYTES
			type->type_size = 2;
			break;
		case S_INT32:
		case U_INT32:
		case FLOAT32:
			//4 BYTES
			type->type_size = 4;
			break;
		case VOID:
			//Special case -- 0 bytes
			type->type_size = 0;
			break;
		default:
			//Otheriwse is 8 BYTES
			type->type_size = 8;
			break;
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

	//Clone the string
	type->type_name = clone_dynamic_string(&(points_to->type_name));

	//Add the star at the end
	dynamic_string_add_char_to_back(&(type->type_name), '*');

	//Now we'll make the actual pointer type
	type->pointer_type = calloc(1, sizeof(pointer_type_t));

	//We need to determine if this is a generic(void) pointer
	if(points_to->type_class == TYPE_CLASS_BASIC && points_to->basic_type->basic_type == VOID){
		type->pointer_type->is_void_pointer = TRUE;

	//If we're pointing to a void*, we'll also need to carry that up the chain
	} else if(points_to->type_class == TYPE_CLASS_POINTER && points_to->pointer_type->is_void_pointer == TRUE){
		type->pointer_type->is_void_pointer = TRUE;
	}

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

	//Array type class
	type->type_class = TYPE_CLASS_ARRAY;

	//Where was it declared
	type->line_number = line_number;

	//Clone the string
	type->type_name = clone_dynamic_string(&(points_to->type_name));

	//Add the star at the end
	dynamic_string_concatenate(&(type->type_name), "[]");

	//Now we'll make the actual pointer type
	type->array_type = calloc(1, sizeof(array_type_t));

	//Store what it points to
	type->array_type->member_type = points_to;
	
	//Store the number of members
	type->array_type->num_members = num_members;

	//Store this in here
	type->type_size = points_to->type_size * num_members;

	return type;
}


/**
 * Dynamically allocate and create an enumerated type
 */
generic_type_t* create_enumerated_type(dynamic_string_t type_name, u_int32_t line_number){
	generic_type_t* type = calloc(1, sizeof(generic_type_t));

	//Assign the class
	type->type_class = TYPE_CLASS_ENUMERATED;
	
	//Where is the declaration?
	type->line_number = line_number;

	type->type_name = type_name;

	//Reserve space for this
	type->enumerated_type = calloc(1, sizeof(enumerated_type_t));

	return type;
}


/**
 * Dynamically allocate and create a constructed type
 */
generic_type_t* create_constructed_type(dynamic_string_t type_name, u_int32_t line_number){
	generic_type_t* type = calloc(1, sizeof(generic_type_t));

	//Assign the class
	type->type_class = TYPE_CLASS_CONSTRUCT;
	
	//Where is the declaration?
	type->line_number = line_number;

	type->type_name = type_name;

	//Reserve space for this
	type->construct_type = calloc(1, sizeof(constructed_type_t));

	return type;
}


/**
 * Add a value to a constructed type. The void* here is a 
 * symtab variable record
 */
u_int8_t add_construct_member(generic_type_t* type, void* member_var){
	//Grab this out for convenience
	constructed_type_t* construct = type->construct_type;

	//Check for size constraints
	if(construct->next_index >= MAX_CONSTRUCT_MEMBERS){
		return OUT_OF_BOUNDS;
	}

	//Grab this reference out, for convenience
	symtab_variable_record_t* var = member_var;

	//If this is the very first one, then we'll 
	if(construct->next_index == 0){
		constructed_type_field_t entry;	
		//Currently, we don't need any padding
		entry.padding = 0;
		entry.variable = member_var;
		//This if the very first struct member, so its offset is 0
		entry.offset = 0;

		//Also by defualt, this is currently the largest variable that we've seen
		construct->largest_member_size = var->type_defined_as->type_size;

		//Just grab this as a reference to avoid the need to cast
		symtab_variable_record_t* member = member_var;

		//Increment the size by the amount of the type
		construct->size += member->type_defined_as->type_size;

		//Add this into the construct table
		construct->construct_table[construct->next_index] = entry;

		//Increment the index for the next go around
		construct->next_index += 1;

		//This worked, so return success
		return SUCCESS;
	}
	
	//Otherwise, if we make it down here, it means that we'll need to pay a bit more
	//attention to alignment as there is more than one field
	constructed_type_field_t entry;
	//We'll update the largest member, if applicable
	if(var->type_defined_as->type_size > construct->largest_member_size){
		//Update the largest member if this happens
		construct->largest_member_size = var->type_defined_as->type_size;
	}

	//For right now let's just have this added in
	entry.variable = var;
	//And currently, we don't need any padding
	entry.padding = 0;
	
	//Let's now see where the ending address of the struct is. We can find
	//this ending dress by calculating the offset of the latest field plus
	//the size of the latest variable
	
	//The prior variable
	symtab_variable_record_t* prior_variable = construct->construct_table[construct->next_index - 1].variable;
	//And the offset of this entry
	u_int32_t offset = construct->construct_table[construct->next_index - 1].offset;
	
	//The current ending address is the offset of the last variable plus its size
	u_int32_t current_end = offset + prior_variable->type_defined_as->type_size;

	//Now for alignment, we need the offset of this new variable to be a multiple of the new variable's
	//size
	u_int32_t new_entry_size = var->type_defined_as->type_size;

	//We will satisfy this by adding the remainder of the division of the new variable with the current
	//end in as padding to the previous entry
	
	//What padding is needed?
	u_int32_t needed_padding;
	
	if(current_end < new_entry_size){
		//If it's more than 16(i.e an array), the most we'd need is 16-byte aligned
		if(new_entry_size > 16){
			needed_padding = 16 - current_end;
		} else {
			needed_padding = new_entry_size - current_end;
		}
	} else {
		needed_padding = current_end % new_entry_size;
	}

	//This needed padding will go as padding on the prior entry
	construct->construct_table[construct->next_index - 1].padding = needed_padding;

	//Now we can update the current end
	current_end = current_end + needed_padding;

	//And now we can add in the new variable's offset
	entry.offset = current_end;

	//Increment the size by the amount of the type and the padding we're adding in
	construct->size += var->type_defined_as->type_size + needed_padding;

	//Finally, we can add this new entry in
	construct->construct_table[construct->next_index] = entry;
	construct->next_index += 1;

	return SUCCESS;
}


/**
 * Does this construct contain said member? Return the variable if yes, NULL if not
 */
constructed_type_field_t* get_construct_member(constructed_type_t* construct, char* name){
	//The current variable that we have
	symtab_variable_record_t* var;

	//Run through everything here
	for(u_int16_t _ = 0; _ < construct->next_index; _++){
		//Grab the variable out
		var = construct->construct_table[_].variable;

		//Now we'll do a simple comparison. If they match, we're set
		if(strcmp(var->var_name.string, name) == 0){
			//Return the whole record if we find it
			return &(construct->construct_table[_]);
		}
	}

	//Otherwise if we get down here, it didn't work
	return NULL;
}


/**
 * Finalize the construct alignment. This should only be invoked 
 * when we're done processing members
 *
 * The struct's end address needs to be a multiple of the size
 * of it's largest field. We keep track of the largest field
 * throughout the entirety of construction, so this should be easy
 */
void finalize_construct_alignment(generic_type_t* type){
	//Let's see how far off we are from being a multiple of the
	//final address
	u_int32_t needed_padding = type->type_size % type->construct_type->largest_member_size;

	//Whatever this needed padding may be, we'll add it to the end as the final padding for our construct
	type->construct_type->construct_table[type->construct_type->next_index - 1].padding = needed_padding;

	//Increment the size accordingly
	type->construct_type->size += needed_padding;

	//Now we move this over to size
	type->type_size = type->construct_type->size;
}


/**
 * Dynamically allocate and create an aliased type
 */
generic_type_t* create_aliased_type(dynamic_string_t type_name, generic_type_t* aliased_type, u_int32_t line_number){
	generic_type_t* type = calloc(1, sizeof(generic_type_t));

	//Assign the class
	type->type_class = TYPE_CLASS_ALIAS;
	
	//Where is the declaration?
	type->line_number = line_number;

	//Copy the name
	type->type_name = type_name;

	//Dynamically allocate the aliased type record
	type->aliased_type = calloc(1, sizeof(aliased_type_t));

	//Store this reference in here
	type->aliased_type->aliased_type = aliased_type;

	return type;
}


/**
 * Dynamically allocate and create a function pointer type
 */
generic_type_t* create_function_pointer_type(u_int32_t line_number){
	//First allocate the parent
	generic_type_t* type = calloc(1, sizeof(generic_type_t));

	//Assign the class & line number
	type->type_class = TYPE_CLASS_FUNCTION_SIGNATURE;
	type->line_number = line_number;

	//Now we need to create the internal function pointer type
	type->function_type = calloc(1, sizeof(function_type_t));

	//And give the type back
	return type;
}


/**
 * Is a type signed?
 */
u_int8_t is_type_signed(generic_type_t* type){
	//We must have a basic type for it to be signed
	if(type->type_class != TYPE_CLASS_BASIC){
		//By default everything else(addresses, etc) is not signed
		return FALSE;
	}

	//If we get here there's a chance it could be signed
	Token basic_type_token = type->basic_type->basic_type;

	switch(basic_type_token){
		case S_INT8:
		case S_INT16:
		case S_INT32:
		case SIGNED_INT_CONST:
		case S_INT64:
		case FLOAT32:
		case FLOAT64:
			return TRUE;
		default:
			return FALSE;
	}
}

/**
 * Convert a generic type to a sring
 */
static char* basic_type_to_string(generic_type_t* type){
	if(type->type_class != TYPE_CLASS_BASIC){
		return "complex_type";
	}

	switch(type->basic_type->basic_type){
		case S_INT8:
			return "i8";
		case U_INT8:
			return "u8";
		case S_INT16:
			return "i16";
		case U_INT16:
			return "u16";
		case S_INT32:
			return "i32";
		case U_INT32:
			return "u32";
		case S_INT64:
			return "i64";
		case U_INT64:
			return "u64";
		case CHAR:
			return "char";
		case VOID:
			return "void";
		case FLOAT32:
			return "f32";
		case FLOAT64:
			return "f64";
		default:
			return "complex_type";
	}
}


/**
 * Print a function pointer type out
 */
void print_function_pointer_type(generic_type_t* function_pointer_type){
	//Extract this out
	function_type_t* function_type = function_pointer_type->function_type;

	printf("fn(");

	for(u_int16_t i = 0; i < function_type->num_params; i++){
		if(function_type->parameters[i].is_mutable == TRUE){
			printf("mut ");
		} 
		printf("%s", basic_type_to_string(function_type->parameters[i].parameter_type));

		if(i != function_type->num_params - 1){
			printf(", ");
		}
	}

	printf(") -> %s\n", basic_type_to_string(function_type->return_type));
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
	//Free based on what type of type we have
	switch(type->type_class){
		case TYPE_CLASS_BASIC:
			free(type->basic_type);
			break;
		case TYPE_CLASS_ALIAS:
			free(type->aliased_type);
			break;
		case TYPE_CLASS_ENUMERATED:
			free(type->enumerated_type);
			break;
		case TYPE_CLASS_FUNCTION_SIGNATURE:
			free(type->function_type);
			break;
		case TYPE_CLASS_ARRAY:
			free(type->array_type);
			break;
		case TYPE_CLASS_POINTER:
			free(type->pointer_type);
			break;
		case TYPE_CLASS_CONSTRUCT:
			free(type->construct_type);
			break;
		default:
			break;
	}

	//Finally just free the overall pointer
	free(type);
}
