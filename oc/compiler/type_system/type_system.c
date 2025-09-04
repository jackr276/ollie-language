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
 * Does this type represent a memory address?
 */
u_int8_t is_memory_address_type(generic_type_t* type){
	switch(type->type_class){
		case TYPE_CLASS_POINTER:
		case TYPE_CLASS_ARRAY:
		case TYPE_CLASS_STRUCT:
			return TRUE;
		default:
			return FALSE;
	}
}


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
		case TYPE_CLASS_STRUCT:
			return TRUE;

		//Let's see what we have here
		case TYPE_CLASS_BASIC:
			//If it's a u64 then yes
			if(type->basic_type_token == U64){
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
	switch(type->basic_type_token){
		//Our only real cases here
		case U32:
		case I32:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Simple helper to check if a type is void
 */
u_int8_t is_void_type(generic_type_t* type){
	if(type->type_class != TYPE_CLASS_BASIC){
		return FALSE;
	}

	if(type->basic_type_token != VOID){
		return FALSE;
	}

	return TRUE;
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
				current_type = current_type->internal_types.member_type;
				break;
			case TYPE_CLASS_POINTER:
				current_type = current_type->internal_types.points_to;
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
		&& typeA->internal_values.num_members != typeB->internal_values.num_members){
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
		case TYPE_CLASS_STRUCT:
			return TRUE;

		//Some more exploration needed here
		case TYPE_CLASS_BASIC:
			//Extract this
			basic_type = type->basic_type_token;
		
			//We're allowed to see 64 bit types here
			if(basic_type == U64 || basic_type == I64){
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
		case TYPE_CLASS_STRUCT:
		case TYPE_CLASS_POINTER:
			return FALSE;
		case TYPE_CLASS_ENUMERATED:
			return TRUE;
		case TYPE_CLASS_BASIC:
			//Grab this out
			basic_type_token = type->basic_type_token;

			//We just can't see floats or void here
			if(basic_type_token == VOID || basic_type_token == F32
				|| basic_type_token == F64){
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
		case TYPE_CLASS_STRUCT:
			return FALSE;
		case TYPE_CLASS_POINTER:
			return TRUE;
		case TYPE_CLASS_ENUMERATED:
			return TRUE;
		case TYPE_CLASS_BASIC:
			//If it's void, then we can't have it. Otherwise
			//it's fine
			if(type->basic_type_token != VOID){
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
 * Function signatures must be absolutely identical for them to be considered assignable.
 * If they are not 100% the same, then they are not assignable and this rule will return false
 *
 * We have a clever workaround for this one and only case. Since function pointers have their
 * names generated by the compiler based on their parameters/return type by the compiler, we
 * can just compare the two names to tell if they're the same
 */
static u_int8_t function_signatures_identical(generic_type_t* a, generic_type_t* b){
	//If these are not the same, fail out
	if(strcmp(a->type_name.string, b->type_name.string) != 0){
		return FALSE;
	}

	//If we survived to down here, then we return TRUE
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
		case TYPE_CLASS_STRUCT:
			//Not assignable at all
			if(deref_source_type->type_class != TYPE_CLASS_STRUCT){
				return NULL;
			}

			//Now let's check to see if they're the exact same type
			if(strcmp(deref_source_type->type_name.string, deref_source_type->type_name.string) != 0){
				return NULL;
			} else {
				//We'll give back the destination type if they are the same
				return deref_destination_type;
			}

		/**
		 * A function signature type is a very special casein terms of assignability
		 */
		case TYPE_CLASS_FUNCTION_SIGNATURE:
			//If this is not also a function signature, then we're done here
			if(deref_source_type->type_class != TYPE_CLASS_FUNCTION_SIGNATURE){
				return NULL;
			}

			//Otherwise, we'll need to use the helper rule to determine if it's equivalent
			if(function_signatures_identical(deref_destination_type, deref_source_type) == TRUE){
				return deref_destination_type;
			} else {
				return NULL;
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
				source_basic_type = deref_source_type->basic_type_token;

				//It needs to be 8 bits, otherwise we won't allow this
				if(source_basic_type == U8 || source_basic_type == I8 || source_basic_type == CHAR){
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

		//Only one type of array is assignable - and that would be a char[] to a char*
		case TYPE_CLASS_ARRAY:
			//If this isn't a char[], we're done
			if(deref_destination_type->internal_types.member_type->type_class != TYPE_CLASS_BASIC
				|| deref_destination_type->internal_types.member_type->basic_type_token != CHAR){
				return NULL;
			}

			//If it's a pointer
			if(deref_source_type->type_class == TYPE_CLASS_POINTER){
				generic_type_t* points_to = deref_source_type->internal_types.points_to;

				//If it's not a basic type then leave
				if(points_to->type_class != TYPE_CLASS_BASIC){
					return NULL;
				}

				//If it's a char, then we're set
				if(points_to->basic_type_token == CHAR){
					return deref_destination_type;
				}
			}

			return NULL;

		//Refer to the rules above for details
		case TYPE_CLASS_POINTER:
			switch(deref_source_type->type_class){
				case TYPE_CLASS_BASIC:
					//This needs to be a u64, otherwise it's invalid
					if(deref_source_type->basic_type_token == U64){
						//We will keep this as the pointer
						return deref_destination_type;
					//Any other basic type will not work here
					} else {
						return NULL;
					}

				case TYPE_CLASS_ARRAY:
					//If these are the exact same types, then we're set
					if(types_equivalent(deref_destination_type->internal_types.points_to, deref_source_type->internal_types.member_type) == TRUE){
						return deref_destination_type;
					//Otherwise this won't work at all
					} else{
						return NULL;
					}
		
				//Likely the most common case
				case TYPE_CLASS_POINTER:
					//If this itself is a void pointer, then we're good
					if(deref_source_type->internal_values.is_void_pointer == TRUE){
						return deref_destination_type;
					//This is also fine, we just give the destination type back
					} else if(deref_destination_type->internal_values.is_void_pointer == TRUE){
						return deref_destination_type;
					//Let's see if what they point to is the exact same
					} else {
						//They need to be the exact same
						if(types_equivalent(deref_source_type->internal_types.points_to, deref_destination_type->internal_types.points_to) == TRUE){
							return deref_destination_type;
						} else {
							return NULL;
						}
					}

				//Otherwise it's bad here
				default:
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
			dest_basic_type = deref_destination_type->basic_type_token;

			//Switch based on the type that we have here
			switch(dest_basic_type){
				case VOID:
					return NULL;

				//Float64's can only be assigned to other float64's
				case F64:
					//We must see another f64 or an f32(widening) here
					if(deref_source_type->type_class == TYPE_CLASS_BASIC
						&& (deref_source_type->basic_type_token == F64
						|| deref_source_type->basic_type_token == F32)){
						return deref_destination_type;

					//Otherwise nothing here will work
					} else {
						return NULL;
					}

				case F32:
					//We must see another an f32 here
					if(deref_source_type->type_class == TYPE_CLASS_BASIC
						&& deref_source_type->basic_type_token == F32){
						return deref_destination_type;

					//Otherwise nothing here will work
					} else {
						return NULL;
					}

				//Once we get to this point, we know that we have something
				//in this set for destination type: U64, I64, U32, I32, U16, I16, U8, I8, Char
				//From here, we'll go based on the type size of the source type *if* the source
				//type is also a basic type. 
				default:
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
					source_basic_type = deref_source_type->basic_type_token;

					//Go based on what we have here
					switch(source_basic_type){
						//If we have these, we can't assign them to an int
						case F32:
						case F64:
						case VOID:
							return NULL;

						//These generic constant types will always work
						case UNSIGNED_INT_CONST:
						case SIGNED_INT_CONST:
							//Reassign source type to be whatever this destination ends up being
							*source_type = deref_destination_type;
							return deref_destination_type;

						//Otherwise, once we make it here we know that the source type is a basic type and
						//and integer/char type. We can now just compare the sizes and if the destination is more
						//than or equal to the source, we're good
						default:
							if(deref_source_type->type_size <= deref_destination_type->type_size){
								return deref_destination_type;
							} else {
								//These wouldn't fit
								return NULL;
							}
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
static generic_type_t* convert_to_unsigned_version(type_symtab_t* symtab, generic_type_t* type){
	//Switch based on what we have
	switch(type->basic_type_token){
			//Char is already unsigned
		case CHAR:
			return lookup_type_name_only(symtab, "char")->type;
		case U8:
		case I8:
			return lookup_type_name_only(symtab, "u8")->type;
		case U16:
		case I16:
			return lookup_type_name_only(symtab, "u16")->type;
		case U32:
		case I32:
			return lookup_type_name_only(symtab, "u32")->type;
		case U64:
		case I64:
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
	if((*a)->basic_type_token == F32 || (*a)->basic_type_token == F64){
		return;
	}

	//If a is unsigned, b must automatically go to unsigned
	if(is_type_signed(*a) == FALSE){
		//Convert b
		*b = convert_to_unsigned_version(symtab, *b);
		return;
	}

	//Likewise, if b is unsigned, then a must automatically go to unsigned
	if(is_type_signed(*b) == FALSE){
		//Convert a
		*a = convert_to_unsigned_version(symtab, *a);
		return;
	}
}


/**
 * Apply standard coercion rules for basic types
 */
static void basic_type_widening_type_coercion(type_symtab_t* type_symtab, generic_type_t** a, generic_type_t** b){
	//Grab these to avoid unneeded derefs
	Token a_basic_type = (*a)->basic_type_token;
	Token b_basic_type = (*b)->basic_type_token;

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
	switch((*a)->basic_type_token){
		//These all decome f32's
		case U8:
		case I8:
		case CHAR:
		case SIGNED_INT_CONST:
		case UNSIGNED_INT_CONST:
		case U16:
		case I16:
		case U32:
		case I32:
			*a = lookup_type_name_only(symtab, "f32")->type;

		//These become f64's
		case U64:
		case I64:
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
				if((*b)->basic_type_token == F32 || (*b)->basic_type_token == F64){
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
				if((*a)->basic_type_token == F32 || (*a)->basic_type_token == F64){
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
			if((*a)->basic_type_token == F32 || (*a)->basic_type_token == F64){
				integer_to_floating_point(symtab, b);

			//If b is a floating point, we apply the float conversion to b
			} else if((*b)->basic_type_token == F32 || (*b)->basic_type_token == F64){
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
				if((*b)->basic_type_token == F32 || (*b)->basic_type_token == F64){
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
				if((*a)->basic_type_token == F32 || (*a)->basic_type_token == F64){
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
			if((*a)->basic_type_token == F32 || (*a)->basic_type_token == F64){
				integer_to_floating_point(symtab, b);

			//If b is a floating point, we apply the float conversion to b
			} else if((*b)->basic_type_token == F32 || (*b)->basic_type_token == F64){
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
				if((*b)->basic_type_token == F32 || (*b)->basic_type_token == F64){
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
				if((*a)->basic_type_token == F32 || (*a)->basic_type_token == F64){
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
			if((*a)->basic_type_token == F32 || (*a)->basic_type_token == F64){
				integer_to_floating_point(symtab, b);

			//If b is a floating point, we apply the float conversion to b
			} else if((*b)->basic_type_token == F32 || (*b)->basic_type_token == F64){
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

	//Function signatures are never valid for any unary operation
	if(type->type_class == TYPE_CLASS_FUNCTION_SIGNATURE){
		return FALSE;
	}

	//Go based on what token we're given
	switch (unary_op) {
		//This will pull double duty for pre/post increment operators
		case PLUSPLUS:
		case MINUSMINUS:
			//This is invalid for construct types
			if(type->type_class == TYPE_CLASS_ARRAY || type->type_class == TYPE_CLASS_STRUCT){
				return FALSE;
			}

			//It's also invalid for void types
			if(type->type_class == TYPE_CLASS_BASIC && type->basic_type_token == VOID){
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
			if(type->type_class == TYPE_CLASS_BASIC && type->basic_type_token == VOID){
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
			if(type->basic_type_token == VOID){
				return FALSE;
			}

			//Otherwise we get true
			return TRUE;

		//We can negate pointers, enums and basic types that are not void
		case L_NOT:
			//These are bad, we fail out here
			if(type->type_class == TYPE_CLASS_STRUCT || type->type_class == TYPE_CLASS_ARRAY){
				return FALSE;
			}

			//Our other invalid case
			if(type->type_class == TYPE_CLASS_BASIC && type->basic_type_token == VOID){
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
			Token type_tok = type->basic_type_token;
			
			//If it's float or void, we're done
			if(type_tok == F32 || type_tok == F64 || type_tok == VOID){
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
	Token basic_type;

	//Function signatures are never valid for any binary operations
	if(type->type_class == TYPE_CLASS_FUNCTION_SIGNATURE){
		return FALSE;
	}


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
			basic_type = type->basic_type_token;

			//Let's now check and make sure it's not a float or void
			if(basic_type == VOID || basic_type == F32 || basic_type == F64){
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
			basic_type = type->basic_type_token;

			//Let's now just make sure that it is not a void type
			if(basic_type == VOID){
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
			basic_type = type->basic_type_token;

			//Let's now just make sure that it is not a void type
			if(basic_type == VOID){
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
			if(type->type_class == TYPE_CLASS_ARRAY || type->type_class == TYPE_CLASS_STRUCT){
				return FALSE;
			}

			//This also doesn't work for void types
			if(type->type_class == TYPE_CLASS_BASIC && type->basic_type_token == VOID){
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
			if(type->type_class == TYPE_CLASS_ARRAY || type->type_class == TYPE_CLASS_STRUCT){
				return FALSE;
			}

			//This also doesn't work for void types
			if(type->type_class == TYPE_CLASS_BASIC && type->basic_type_token == VOID){
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

	//Store the basic type token in here
	type->basic_type_token = basic_type;
	
	//Create and allocate the name
	dynamic_string_t name;
	dynamic_string_alloc(&name);

	//Set it to be our given name
	dynamic_string_set(&name, type_name);

	//Set the name 
	type->type_name = name;

	//Assign the type in
	type->basic_type_token = basic_type;

	//Now we can immediately determine the size based on what the actual type is
	switch(basic_type){
		case CHAR:
		case I8:
		case U8:
			//1 BYTE
			type->type_size = 1;
			break;
		case I16:
		case U16:
			//2 BYTES
			type->type_size = 2;
			break;
		case I32:
		case U32:
		case F32:
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

	//We need to determine if this is a generic(void) pointer
	if(points_to->type_class == TYPE_CLASS_BASIC && points_to->basic_type_token == VOID){
		type->internal_values.is_void_pointer = TRUE;

	//If we're pointing to a void*, we'll also need to carry that up the chain
	} else if(points_to->type_class == TYPE_CLASS_POINTER && points_to->internal_values.is_void_pointer == TRUE){
		type->internal_values.is_void_pointer = TRUE;
	}

	//Store what it points to
	type->internal_types.points_to = points_to;

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

	//Store what it points to
	type->internal_types.member_type = points_to;
	
	//Store the number of members
	type->internal_values.num_members = num_members;

	//Store this in here
	type->type_size = points_to->type_size * num_members;

	return type;
}


/**
 * Dynamically allocate and create an enumerated type
 */
generic_type_t* create_enumerated_type(dynamic_string_t type_name, u_int32_t line_number){
	//Dynamically allocate, 0 out
	generic_type_t* type = calloc(1, sizeof(generic_type_t));

	//Assign the class
	type->type_class = TYPE_CLASS_ENUMERATED;
	
	//Where is the declaration?
	type->line_number = line_number;

	//Store the name
	type->type_name = type_name;

	//Reserve space for the enum table
	type->internal_types.enumeration_table = dynamic_array_alloc();

	return type;
}


/**
 * Dynamically allocate and create a constructed type
 */
generic_type_t* create_struct_type(dynamic_string_t type_name, u_int32_t line_number){
	generic_type_t* type = calloc(1, sizeof(generic_type_t));

	//Assign the class
	type->type_class = TYPE_CLASS_STRUCT;
	
	//Where is the declaration?
	type->line_number = line_number;

	type->type_name = type_name;

	//Reserve dynamic array space for the struct table
	type->internal_types.struct_table = dynamic_array_alloc();

	return type;
}


/**
 * Dynamically allocate and create a union type
 */
generic_type_t* create_union_type(dynamic_string_t type_name, u_int32_t line_number){
	//Dynamically allocate the union type
	generic_type_t* type = calloc(1, sizeof(generic_type_t));

	//Move the name over
	type->type_name = type_name;

	//The line number where this was created
	type->line_number = line_number;

	//Reserve space for the internal type as well
	type->internal_types.union_type = calloc(1, sizeof(union_type_t));

	//And give the type pointer back
	return type;
}


/**
 * Does this struct contain said member? Return the variable if yes, NULL if not
 */
void* get_struct_member(generic_type_t* structure, char* name){
	//The current variable that we have
	symtab_variable_record_t* var;

	//Extract for convenience
	dynamic_array_t* struct_table = structure->internal_types.struct_table;

	//Run through everything here
	for(u_int16_t _ = 0; _ < struct_table->current_index; _++){
		//Grab the variable out
		var = dynamic_array_get_at(struct_table, _);

		//Now we'll do a simple comparison. If they match, we're set
		if(strcmp(var->var_name.string, name) == 0){
			//Return the whole record if we find it
			return var;
		}
	}

	//Otherwise if we get down here, it didn't work
	return NULL;
}


/**
 * Add a value to a struct type. The void* here is a 
 * symtab variable record
 */
u_int8_t add_struct_member(generic_type_t* type, void* member_var){
	//Grab this reference out, for convenience
	symtab_variable_record_t* var = member_var;

	//Mark that this is a struct member
	var->is_struct_member = TRUE;

	//If this is the very first one, then we'll 
	if(type->internal_types.struct_table->current_index == 0){
		var->struct_offset = 0;

		//Also by defualt, this is currently the largest variable that we've seen
		type->internal_values.largest_member_size = var->type_defined_as->type_size;

		//Just grab this as a reference to avoid the need to cast
		symtab_variable_record_t* member = member_var;

		//Increment the size by the amount of the type
		type->type_size += member->type_defined_as->type_size;

		//Add the variable into the struct table
		dynamic_array_add(type->internal_types.struct_table, var);

		//This worked, so return success
		return SUCCESS;
	}
	
	//We'll update the largest member, if applicable
	if(var->type_defined_as->type_size > type->internal_values.largest_member_size){
		//Update the largest member if this happens
		type->internal_values.largest_member_size = var->type_defined_as->type_size;
	}

	//Let's now see where the ending address of the struct is. We can find
	//this ending dress by calculating the offset of the latest field plus
	//the size of the latest variable
	
	//The prior variable
	symtab_variable_record_t* prior_variable = dynamic_array_get_at(type->internal_types.struct_table, type->internal_types.struct_table->current_index - 1);

	//And the offset of this entry
	u_int32_t offset = prior_variable->struct_offset;
	
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

	//Now we can update the current end
	current_end = current_end + needed_padding;

	//And now we can add in the new variable's offset
	var->struct_offset = current_end;

	//Increment the size by the amount of the type and the padding we're adding in
	type->type_size += var->type_defined_as->type_size + needed_padding;

	//Add the variable into the table
	dynamic_array_add(type->internal_types.struct_table, var);

	return SUCCESS;
}


/**
 * Add a value into the union's list of members
 */
u_int8_t add_union_member(generic_type_t* union_type, void* member_var){
	//Let's extract the union member and variable record for convenience
	symtab_variable_record_t* record = member_var;
	union_type_t* internal_union_type = union_type->internal_types.union_type;

	//We've overflowed the bounds here, so fail out
	if(internal_union_type->next_index == MAX_UNION_MEMBERS){
		return FAILURE;
	}

	//Add the member variable in
	internal_union_type->members[internal_union_type->next_index] = member_var;

	//Increment the next index
	internal_union_type->next_index++;

	//If the size of this value is larger than the total size, we need to reassign
	//the total size to this. Union types are always as large as their largest memeber
	if(record->type_defined_as->type_size > union_type->type_size){
		union_type->type_size = record->type_defined_as->type_size;
	}

	//All went well
	return SUCCESS;
}


/**
 * Finalize the construct alignment. This should only be invoked 
 * when we're done processing members
 *
 * The struct's end address needs to be a multiple of the size
 * of it's largest field. We keep track of the largest field
 * throughout the entirety of construction, so this should be easy
 */
void finalize_struct_alignment(generic_type_t* type){
	//Let's see how far off we are from being a multiple of the
	//final address
	u_int32_t needed_padding = type->type_size % type->internal_values.largest_member_size;

	//Increment the size accordingly
	type->type_size += needed_padding;
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

	//Store this reference in here
	type->internal_types.aliased_type = aliased_type;

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
	type->internal_types.function_type = calloc(1, sizeof(function_type_t));

	//These are always 8 bytes
	type->type_size = 8;

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
	Token basic_type_token = type->basic_type_token;

	switch(basic_type_token){
		case I8:
		case I16:
		case I32:
		case SIGNED_INT_CONST:
		case I64:
		case F32:
		case F64:
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
		return type->type_name.string;
	}

	switch(type->basic_type_token){
		case I8:
			return "i8";
		case U8:
			return "u8";
		case I16:
			return "i16";
		case U16:
			return "u16";
		case I32:
			return "i32";
		case U32:
			return "u32";
		case I64:
			return "i64";
		case U64:
			return "u64";
		case CHAR:
			return "char";
		case VOID:
			return "void";
		case F32:
			return "f32";
		case F64:
			return "f64";
		default:
			return type->type_name.string;
	}
}


/**
 * Generate the full name for the function pointer type
 */
void generate_function_pointer_type_name(generic_type_t* function_pointer_type){
	//Reserve this for variable printing
	char var_string[MAX_IDENT_LENGTH];

	//Allocate the type name
	dynamic_string_alloc(&(function_pointer_type->type_name));

	//Extract this out
	function_type_t* function_type = function_pointer_type->internal_types.function_type;

	//Set the type name initially
	dynamic_string_set(&(function_pointer_type->type_name), "fn(");

	for(u_int16_t i = 0; i < function_type->num_params; i++){
		if(function_type->parameters[i].is_mutable == TRUE){
			//Add this in dynamically
			dynamic_string_concatenate(&(function_pointer_type->type_name), "mut ");
		} 

		//First put this into the buffer string
		sprintf(var_string, "%s", basic_type_to_string(function_type->parameters[i].parameter_type));

		//Then concatenate
		dynamic_string_concatenate(&(function_pointer_type->type_name), var_string);

		//Add the comma in if need be
		if(i != function_type->num_params - 1){
			//Then concatenate
			dynamic_string_concatenate(&(function_pointer_type->type_name), ", ");
		}
	}

	//First print this to the buffer
	sprintf(var_string, ") -> %s", basic_type_to_string(function_type->return_type));

	//Add the closing sequence
	dynamic_string_concatenate(&(function_pointer_type->type_name), var_string);
}


/**
 * Is this type equivalent to a char**? This is used
 * exclusively for main function validation
 */
u_int8_t is_type_string_array(generic_type_t* type){
	//Grab the first level
	generic_type_t* first_level = dealias_type(type);

	//If it isn't a pointer, we fail out
	if(first_level->type_class != TYPE_CLASS_POINTER){
		return FALSE;
	}

	//Now we go one level deeper
	generic_type_t* second_level = dealias_type(first_level->internal_types.points_to);

	//If it isn't a pointer, we fail out
	if(second_level->type_class != TYPE_CLASS_POINTER){
		return FALSE;
	}

	//Now we get to the base type
	generic_type_t* base_type = dealias_type(second_level->internal_types.points_to);

	//If this isn't a char, we fail
	if(base_type->type_class != TYPE_CLASS_BASIC || base_type->basic_type_token != CHAR){
		return FALSE;
	}

	//If we make it here, we know we have char**
	return TRUE;
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
		raw_type = raw_type->internal_types.aliased_type;
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
		case TYPE_CLASS_ENUMERATED:
			free(type->internal_types.enumeration_table);
			break;
		case TYPE_CLASS_FUNCTION_SIGNATURE:
			free(type->internal_types.function_type);
			break;
		//For this one we can deallocate the struct table
		case TYPE_CLASS_STRUCT:
			dynamic_array_dealloc(type->internal_types.struct_table);
			break;
		case TYPE_CLASS_UNION:
			free(type->internal_types.union_type);
			break;
		default:
			break;
	}

	//Finally just free the overall pointer
	free(type);
}
