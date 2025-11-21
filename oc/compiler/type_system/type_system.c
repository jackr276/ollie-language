/**
 * The implementation file for the type_system.h header file
*/

#include "type_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
//Link to the symtab for variable storage
#include "../symtab/symtab.h"
#include "../utils/constants.h"

/**
 * Is this a stack memory region variable or not? Stack memory
 * regions or memory chunks are: arrays, structs and unions
 */
u_int8_t is_memory_region(generic_type_t* type){
	switch(type->type_class){
		case TYPE_CLASS_ARRAY:
		case TYPE_CLASS_STRUCT:
		case TYPE_CLASS_UNION:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Does this type represent a memory address?
 */
u_int8_t is_memory_address_type(generic_type_t* type){
	switch(type->type_class){
		case TYPE_CLASS_POINTER:
		case TYPE_CLASS_ARRAY:
		case TYPE_CLASS_STRUCT:
		case TYPE_CLASS_UNION:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Does assigning from source to destination require a converting move
 */
u_int8_t is_converting_move_required(generic_type_t* destination_type, generic_type_t* source_type){
	//Very simple rule(for now), just compare the sizes
	if(destination_type->type_size > source_type->type_size){
		return TRUE;
	}

	//Otherwise it's fine
	return FALSE;
}


/**
 * What is the value that this needs to be aligned by?
 *
 * For arrays -> we align so that the base address is a multiple of the member type
 * For structs -> we align so that the base address is a multiple of the largest
 * member
 */
generic_type_t* get_base_alignment_type(generic_type_t* type){
	switch(type->type_class){
		//However for an array, we need to find the
		//size of the member type
		case TYPE_CLASS_ARRAY:
			//Recursively get the size of the member type
			return get_base_alignment_type(type->internal_types.member_type);

		//A struct it's the largest member size
		case TYPE_CLASS_STRUCT:
			return get_base_alignment_type(type->internal_values.largest_member_type);

		//By default just give the size back
		default:
			return type;
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
 * Is the given type memory movement appropriate
 */
u_int8_t is_type_address_calculation_compatible(generic_type_t* type){
	//The basic type token for later
	ollie_token_t basic_type;

	//Arrays and pointers are fine
	switch(type->type_class){
		//These are all essentially pointers
		case TYPE_CLASS_ARRAY:
		case TYPE_CLASS_POINTER:
		case TYPE_CLASS_STRUCT:
		case TYPE_CLASS_UNION:
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
	ollie_token_t basic_type_token;

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
		case TYPE_CLASS_UNION:
		case TYPE_CLASS_ARRAY:
		case TYPE_CLASS_STRUCT:
			return FALSE;
		case TYPE_CLASS_POINTER:
			return TRUE;
		case TYPE_CLASS_ENUMERATED:
			return TRUE;
		case TYPE_CLASS_BASIC:
			//If it's void, then we can't have it. Otherwise
			//it's fine
			if(type->basic_type_token == VOID){
				return FALSE;;
			}

			//Otherwise it's true
			return TRUE;

		default:
			return FALSE;
	}
}

/**
 * Is a type conversion needed between these two types for the source type to fit into the destination type
 */
u_int8_t is_expanding_move_required(generic_type_t* destination_type, generic_type_t* source_type){
	//The maximum that any of these can ever be is 8
	u_int32_t destination_size = destination_type->type_size <= 8 ? destination_type->type_size : 8;
	u_int32_t source_size = source_type->type_size <= 8 ? source_type->type_size : 8;

	//If the destination is larger than the source, we must convert
	if(destination_size > source_size){
		//printf("Type conversion between needed for %s to be assigned to %s\n", source_type->type_name.string, destination_type->type_name.string);
		return TRUE;
	}

	//By default, we say no
	return FALSE;
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
generic_type_t* types_assignable(generic_type_t* destination_type, generic_type_t* source_type){
	//Predeclare these for now
	ollie_token_t source_basic_type;
	ollie_token_t dest_basic_type;

	switch(destination_type->type_class){
		//This is a simpler case - constructs can only be assigned
		//if they're the exact same
		case TYPE_CLASS_STRUCT:
			if(destination_type == source_type){
				return destination_type;
			}

			return NULL;

		//This will only work if they're the exact same
		case TYPE_CLASS_UNION:
			if(destination_type == source_type){
				return destination_type;
			}
			
			return NULL;

		/**
		 * A function signature type is a very special casein terms of assignability
		 */
		case TYPE_CLASS_FUNCTION_SIGNATURE:
			//If this is not also a function signature, then we're done here
			if(source_type->type_class != TYPE_CLASS_FUNCTION_SIGNATURE){
				return NULL;
			}

			//Otherwise, we'll need to use the helper rule to determine if it's equivalent
			if(function_signatures_identical(destination_type, source_type) == TRUE){
				return destination_type;
			} else {
				return NULL;
			}
			
		//Enum's can internally be any unsigned integer
		case TYPE_CLASS_ENUMERATED:
			//Go based on what the source it
			switch(source_type->type_class){
				case TYPE_CLASS_ENUMERATED:
					//These need to be the exact same, otherwise this will not work
					if(destination_type == source_type){
						return destination_type;
					} else {
						return NULL;
					}

				//If we have a basic type, we can just compare it with the enum's internal int
				case TYPE_CLASS_BASIC:
					switch(source_type->basic_type_token){
						//These are all bad
						case F32:
						case F64:
						case VOID:
							return NULL;
						default:
							return types_assignable(destination_type->internal_values.enum_integer_type, source_type);
					}

				//Anything else is bad
				default:
					return NULL;
			}

		//Only one type of array is assignable - and that would be a char[] to a char*
		case TYPE_CLASS_ARRAY:
			//If this isn't a char[], we're done
			if(destination_type->internal_types.member_type->type_class != TYPE_CLASS_BASIC
				|| destination_type->internal_types.member_type->basic_type_token != CHAR){
				return NULL;
			}

			//If it's a pointer
			if(source_type->type_class == TYPE_CLASS_POINTER){
				generic_type_t* points_to = source_type->internal_types.points_to;

				//If it's not a basic type then leave
				if(points_to->type_class != TYPE_CLASS_BASIC){
					return NULL;
				}

				//If it's a char, then we're set
				if(points_to->basic_type_token == CHAR){
					return destination_type;
				}
			}

			return NULL;

		//Refer to the rules above for details
		case TYPE_CLASS_POINTER:
			switch(source_type->type_class){
				case TYPE_CLASS_BASIC:
					//This needs to be a u64, otherwise it's invalid
					if(source_type->basic_type_token == U64){
						//We will keep this as the pointer
						return destination_type;
					//Any other basic type will not work here
					} else {
						return NULL;
					}

				//Check if they're assignable
				case TYPE_CLASS_ARRAY:
					//If this works, return the destination type
					if(types_assignable(destination_type->internal_types.points_to, source_type->internal_types.member_type) != NULL){
						return destination_type;
					}
					
					return NULL;
		
				//Likely the most common case
				case TYPE_CLASS_POINTER:
					//If this itself is a void pointer, then we're good
					if(source_type->internal_values.is_void_pointer == TRUE){
						return destination_type;
					//This is also fine, we just give the destination type back
					} else if(destination_type->internal_values.is_void_pointer == TRUE){
						return destination_type;
					//Let's see if what they point to is the exact same
					} else {
						//If this works, return the destination type
						if(types_assignable(destination_type->internal_types.points_to, source_type->internal_types.points_to) != NULL){
							return destination_type;
						}

						return NULL;
					}

				//Otherwise it's bad here
				default:
					return NULL;
			}
	
		/**
		 * Basic types are the most interesting variety because we may need to coerce these values
		 * according to what the destination type is
		 */
		case TYPE_CLASS_BASIC:
			//Extract the destination's basic type
			dest_basic_type = destination_type->basic_type_token;

			//Switch based on the type that we have here
			switch(dest_basic_type){
				case VOID:
					return NULL;

				//Float64's can only be assigned to other float64's
				case F64:
					//We must see another f64 or an f32(widening) here
					if(source_type->type_class == TYPE_CLASS_BASIC
						&& (source_type->basic_type_token == F64
						|| source_type->basic_type_token == F32)){
						return destination_type;

					//Otherwise nothing here will work
					} else {
						return NULL;
					}

				case F32:
					//We must see another an f32 here
					if(source_type->type_class == TYPE_CLASS_BASIC
						&& source_type->basic_type_token == F32){
						return destination_type;

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
					if(source_type->type_class == TYPE_CLASS_ENUMERATED){
						return destination_type;
					}

					//Now if the source type is not a basic type, we're done here
					if(source_type->type_class != TYPE_CLASS_BASIC){
						return NULL;
					}
					
					//Once we get here, we know that the source type is a basic type. We now
					//need to check that it's not a float or void
					source_basic_type = source_type->basic_type_token;

					//Go based on what we have here
					switch(source_basic_type){
						//If we have these, we can't assign them to an int
						case F32:
						case F64:
						case VOID:
							return NULL;

						//Otherwise, once we make it here we know that the source type is a basic type and
						//and integer/char type. We can now just compare the sizes and if the destination is more
						//than or equal to the source, we're good
						default:
							if(source_type->type_size <= destination_type->type_size){
								return destination_type;
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
		case BOOL:
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
static void basic_type_widening_type_coercion(generic_type_t** a, generic_type_t** b){
	//Whomever has the largest size wins
	if((*a)->type_size > (*b)->type_size){
		//Set b to equal a
		*b = *a;
	} else if((*a)->type_size < (*b)->type_size){
		//Set a to equal b
		*a = *b;
	}
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
generic_type_t* determine_compatibility_and_coerce(void* symtab, generic_type_t** a, generic_type_t** b, ollie_token_t op){
	//For convenience
	symtab = (type_symtab_t*)symtab;

	//Before we go any further - make sure these types are fully raw(They should be anyways, but insurance never hurts)
	*a = dealias_type(*a);
	*b = dealias_type(*b);

	//Lookup what the enum type actually is and use that
	if((*a)->type_class == TYPE_CLASS_ENUMERATED){
		*a = (*a)->internal_values.enum_integer_type;
	}

	//Lookup what the enum type actually is and use that
	if((*b)->type_class == TYPE_CLASS_ENUMERATED){
		*b = (*b)->internal_values.enum_integer_type;
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

				//Give back the pointer type as the result
				return *a;
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

				//Give back the pointer type as the result
				return *b;
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
			basic_type_widening_type_coercion(a, b);

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
				return lookup_type_name_only(symtab, "bool")->type;
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
				return lookup_type_name_only(symtab, "bool")->type;
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
			basic_type_widening_type_coercion(a, b);

			//Give back a
			return lookup_type_name_only(symtab, "bool")->type;

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
			basic_type_widening_type_coercion(a, b);
		
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
			basic_type_widening_type_coercion(a, b);

			//We'll give back *a once we're finished
			return *a;

		/**
		 * Very unique case - ternary operator
		 */
		case QUESTION:
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

				return *b;
			}
			
			//If b is a pointer type. This is teh exact same scenario as a
			if((*b)->type_class == TYPE_CLASS_POINTER){
				//If b is a another pointer, then that's fine
				if((*a)->type_class == TYPE_CLASS_POINTER){
					//We'll return a final comparison type of bool 
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

				//We'll return a final comparison type of bool 
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
			basic_type_widening_type_coercion(a, b);

			//We'll return a final comparison type of bool 
			return *a;

		/**
		 * Relational operators will apply normal conversion rules. If we have
		 * a pointer, we will coerce the other integer to a u64
		 */
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
					return lookup_type_name_only(symtab, "bool")->type;
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

				//This will always return a boolean
				return lookup_type_name_only(symtab, "bool")->type;
			}
			
			//If b is a pointer type. This is teh exact same scenario as a
			if((*b)->type_class == TYPE_CLASS_POINTER){
				//If b is a another pointer, then that's fine
				if((*a)->type_class == TYPE_CLASS_POINTER){
					//We'll return a final comparison type of bool 
					return lookup_type_name_only(symtab, "bool")->type;
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

				//We'll return a final comparison type of bool 
				return lookup_type_name_only(symtab, "bool")->type;
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
			basic_type_widening_type_coercion(a, b);

			//We need to use either a bool or an i8 if they're signed. Internally,
			//these are treated the same
			generic_type_t* return_type;

			//Is it signed? If so use the i8
			if(is_type_signed(*a) == TRUE){
				return_type = lookup_type_name_only(symtab, "i8")->type;
			} else {
				return_type = lookup_type_name_only(symtab, "bool")->type;
			}

			//We'll return a final comparison type of bool 
			return return_type;

		default:
			return NULL;
	}
}


/**
 * Is the given unary operation valid for the type that was specificed?
 */
u_int8_t is_unary_operation_valid_for_type(generic_type_t* type, ollie_token_t unary_op){
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
			switch(type->type_class){
				case TYPE_CLASS_ARRAY:
				case TYPE_CLASS_STRUCT:
				case TYPE_CLASS_ALIAS:
				case TYPE_CLASS_FUNCTION_SIGNATURE:
				case TYPE_CLASS_UNION:
					return FALSE;
				case TYPE_CLASS_BASIC:
					if(type->basic_type_token == VOID){
						return FALSE;
					}
					
					return TRUE;

				default:
					return TRUE;
			}

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
			ollie_token_t type_tok = type->basic_type_token;
			
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
u_int8_t is_binary_operation_valid_for_type(generic_type_t* type, ollie_token_t binary_op, side_type_t side){
	//Just to be safe, we'll always make sure here
	type = dealias_type(type);

	//Deconstructed basic type(since we'll be using it so much)
	ollie_token_t basic_type;

	//Let's first check if we have any in a
	//series of types that never make sense for any unary operation
	switch (type->type_class) {
		case TYPE_CLASS_UNION:
		case TYPE_CLASS_ARRAY:
		case TYPE_CLASS_STRUCT:
		case TYPE_CLASS_FUNCTION_SIGNATURE:
			return FALSE;

		//Otherwise we'll just bail out of here
		default:
			break;
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
generic_type_t* create_basic_type(char* type_name, ollie_token_t basic_type, mutability_type_t mutability){
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

	//Set this to be the type name
	dynamic_string_set(&name, type_name);

	//Set the type's mutability here
	type->mutability = mutability;

	//Set the name 
	type->type_name = name;

	//Assign the type in
	type->basic_type_token = basic_type;

	//Now we can immediately determine the size based on what the actual type is
	switch(basic_type){
		case CHAR:
		case I8:
		case U8:
		case BOOL:
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

	//This is always compelete at definition
	type->type_complete = TRUE;

	//Give back the pointer, it will need to be freed eventually
	return type;
}


/**
 * Create a pointer type dynamically. In order to have a pointer type, we must also
 * have what it points to.
 */
generic_type_t* create_pointer_type(generic_type_t* points_to, u_int32_t line_number, mutability_type_t mutability){
	generic_type_t* type = calloc(1,  sizeof(generic_type_t));

	//Pointer type class
	type->type_class = TYPE_CLASS_POINTER;

	//Where was it declared
	type->line_number = line_number;

	//Is this mutable or not?
	type->mutability = mutability;

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

	//This is always compelete at definition
	type->type_complete = TRUE;

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
generic_type_t* create_array_type(generic_type_t* points_to, u_int32_t line_number, u_int32_t num_members, mutability_type_t mutability){
	//Allocate it
	generic_type_t* type = calloc(1,  sizeof(generic_type_t));

	//Array type class
	type->type_class = TYPE_CLASS_ARRAY;

	//Is this a mutable type or not?
	type->mutability = mutability;

	//Where was it declared
	type->line_number = line_number;

	//Clone the string
	type->type_name = clone_dynamic_string(&(points_to->type_name));

	//Add the dimensions in at the end
	dynamic_string_concatenate(&(type->type_name), "[]");

	//Store what it points to
	type->internal_types.member_type = points_to;
	
	//Store the number of members
	type->internal_values.num_members = num_members;

	//Store this in here
	type->type_size = points_to->type_size * num_members;

	//This type is considered complete *unless* it's size is 0
	if(type->type_size != 0){
		type->type_complete = TRUE;
	}

	return type;
}


/**
 * Dynamically allocate and create an enumerated type
 */
generic_type_t* create_enumerated_type(dynamic_string_t type_name, u_int32_t line_number, mutability_type_t mutability){
	//Dynamically allocate, 0 out
	generic_type_t* type = calloc(1, sizeof(generic_type_t));

	//Assign the class
	type->type_class = TYPE_CLASS_ENUMERATED;

	//Is this a mutable type or not?
	type->mutability = mutability;
	
	//Where is the declaration?
	type->line_number = line_number;

	//Store the name
	type->type_name = type_name;

	//Reserve space for the enum table
	type->internal_types.enumeration_table = dynamic_array_alloc();

	//This is always compelete at definition
	type->type_complete = TRUE;

	return type;
}


/**
 * Dynamically allocate and create a constructed type
 */
generic_type_t* create_struct_type(dynamic_string_t type_name, u_int32_t line_number, mutability_type_t mutability){
	generic_type_t* type = calloc(1, sizeof(generic_type_t));

	//Assign the class
	type->type_class = TYPE_CLASS_STRUCT;
	
	//Is this a mutable type or not?
	type->mutability = mutability;

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
generic_type_t* create_union_type(dynamic_string_t type_name, u_int32_t line_number, mutability_type_t mutability){
	//Dynamically allocate the union type
	generic_type_t* type = calloc(1, sizeof(generic_type_t));

	//Store that this is a union
	type->type_class = TYPE_CLASS_UNION;

	//Move the name over
	type->type_name = type_name;

	//Save the mutability
	type->mutability = mutability;

	//The line number where this was created
	type->line_number = line_number;

	//Reserve the dynamic array as well
	type->internal_types.union_table = dynamic_array_alloc();

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
 * Does this union contain said member? Return the variable if yes, NULL if not
 */
void* get_union_member(generic_type_t* union_type, char* name){
	//The current variable that we have
	symtab_variable_record_t* var;

	//Extract for convenience
	dynamic_array_t* union_table = union_type->internal_types.union_table;

	//Run through everything here
	for(u_int16_t _ = 0; _ < union_table->current_index; _++){
		//Grab the variable out
		var = dynamic_array_get_at(union_table, _);

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
 *
 * For alignment, it is important to note that we only ever align by primitive data type
 * sizes. The largest an internal alignment can be is by 8
 */
void add_struct_member(generic_type_t* type, void* member_var){
	//Grab this reference out, for convenience
	symtab_variable_record_t* var = member_var;

	//Mark that this is a struct member
	var->membership = STRUCT_MEMBER;

	//If this is the very first one, then we'll 
	if(type->internal_types.struct_table->current_index == 0){
		//This one's offset is 0
		var->struct_offset = 0;

		//Increment the size by the amount of the type
		type->type_size += var->type_defined_as->type_size;

		//Add the variable into the struct table
		dynamic_array_add(type->internal_types.struct_table, var);

		//The largest member size here is the alignment of the biggest type
		type->internal_values.largest_member_type = get_base_alignment_type(var->type_defined_as);

		//Hop out here
		return;
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

	//Get the primitive type that we will need to align by here
	generic_type_t* aligning_by_type = get_base_alignment_type(var->type_defined_as);

	//If we have a larger contender for alignment here, then this will become our largest
	//member type
	if(aligning_by_type->type_size > type->internal_values.largest_member_type->type_size){
		type->internal_values.largest_member_type = aligning_by_type;
	}

	//We will satisfy this by adding the remainder of the division of the new variable with the current
	//end in as padding to the previous entry
	
	//What padding is needed?
	u_int32_t needed_padding = 0;
	
	if(current_end < aligning_by_type->type_size){
		needed_padding = aligning_by_type->type_size - current_end;
	} else {
		needed_padding = current_end % aligning_by_type->type_size;
	}

	//Now we can update the current end
	current_end = current_end + needed_padding;

	//And now we can add in the new variable's offset
	var->struct_offset = current_end;

	//Increment the size by the amount of the type and the padding we're adding in
	type->type_size += var->type_defined_as->type_size + needed_padding;

	//Add the variable into the table
	dynamic_array_add(type->internal_types.struct_table, var);

	//Done
	return; 
}


/**
 * Add a value to an enumeration's list of values
 */
u_int8_t add_enum_member(generic_type_t* enum_type, void* enum_member, u_int8_t user_defined_values){
	//For the type system
	symtab_variable_record_t* enum_variable = enum_member;

	//Flag what this is
	enum_variable->membership = ENUM_MEMBER;

	//Are we using user-defined enum values? If so, we need to check for duplicates
	//that already exist in the list
	if(user_defined_values == TRUE){
		//Extract the enum member's actual value
		for(u_int16_t i = 0; i < enum_type->internal_types.enumeration_table->current_index; i++){
			//Grab the variable out
			symtab_variable_record_t* variable = dynamic_array_get_at(enum_type->internal_types.enumeration_table, i);

			//If these 2 equal, we fail out
			if(variable->enum_member_value == ((symtab_variable_record_t*)enum_member)->enum_member_value){
				return FAILURE;
			}
		}

		//If we survive to here, then we're good
	}

	//Just throw the member in
	dynamic_array_add(enum_type->internal_types.enumeration_table, enum_member);

	//All went well
	return SUCCESS;
}


/**
 * Add a value into the union's list of members
 */
u_int8_t add_union_member(generic_type_t* union_type, void* member_var){
	//Let's extract the union member and variable record for convenience
	symtab_variable_record_t* record = member_var;

	//Flag what this is
	record->membership = UNION_MEMBER;

	//Add this in
	dynamic_array_add(union_type->internal_types.union_table, member_var);

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
	//Grab the alignable type size
	int32_t alignable_type_size = type->internal_values.largest_member_type->type_size;

	//If the size is already a multiple of the alignable type size,
	//then we can stop here and leave
	if(type->type_size % alignable_type_size == 0){
		return;
	}

	/**
	 * The alignable type size is either: 1, 2, 4 or 8
	 *
	 * We will add this alignable type size on so that we are guaranteed to be over
	 * the next highest multiple of said type size
	 *
	 * Then we will and by the 2's complement of this value to 0 out the lowest bits
	 * that need to be 0'd out. At most, we will 0 out the bottom 3 bits for 8-byte aligned
	 */
	type->type_size = (type->type_size + alignable_type_size) & (-alignable_type_size);
}


/**
 * Print the full name of a type *into* the char buffer that
 * is provided
 */
void print_full_type_name(generic_type_t* type, char* name){
	//Mutability printing
	if(type->mutability == MUTABLE){
		sprintf(name, "mut ");
	}

	//Then throw the generated name in there
	sprintf(name, "%s", type->type_name.string);
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
generic_type_t* create_function_pointer_type(u_int8_t is_public, u_int32_t line_number, mutability_type_t mutability){
	//First allocate the parent
	generic_type_t* type = calloc(1, sizeof(generic_type_t));

	//Assign the class & line number
	type->type_class = TYPE_CLASS_FUNCTION_SIGNATURE;
	type->line_number = line_number;

	//Is this type mutable or not?
	type->mutability = mutability;

	//Now we need to create the internal function pointer type
	type->internal_types.function_type = calloc(1, sizeof(function_type_t));

	//Store whether or not this is public
	type->internal_types.function_type->is_public = is_public;

	//These are always 8 bytes
	type->type_size = 8;

	//These are always complete by default
	type->type_complete = TRUE;

	//And give the type back
	return type;
}


/**
 * Add a function's parameter in
 */
u_int8_t add_parameter_to_function_type(generic_type_t* function_type, generic_type_t* parameter){
	//Extract this for convenience
	function_type_t* internal_type = function_type->internal_types.function_type;

	//This means that we've hit the maximum number of parameters
	if(internal_type->num_params == 6){
		return FAILURE;
	}

	//Store the mutability level and parameter type
	internal_type->parameters[internal_type->num_params] = parameter;

	//Increment this
	(internal_type->num_params)++;

	//Give back success
	return SUCCESS;
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
	ollie_token_t basic_type_token = type->basic_type_token;

	switch(basic_type_token){
		case I8:
		case I16:
		case I32:
		case I64:
		case F32:
		case F64:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Select the size based only on a type
 */
variable_size_t get_type_size(generic_type_t* type){
	//What the size will be
	variable_size_t size;

	switch(type->type_class){
		//Probably the most common option
		case TYPE_CLASS_BASIC:
			//Switch based on this
			switch (type->basic_type_token) {
				case U8:
				case I8:
				case CHAR:
				case BOOL:
					size = BYTE;
					break;

				case U16:
				case I16:
					size = WORD;
					break;

				//These are 32 bit(double word)
				case I32:
				case U32:
					size = DOUBLE_WORD;
					break;

				//This is single precision
				case F32:
					size = SINGLE_PRECISION;
					break;

				//This is double precision
				case F64:
					size = DOUBLE_PRECISION;
					break;

				//These are all quad word(64 bit)
				case U64:
				case I64:
					size = QUAD_WORD;
					break;
			
				//We shouldn't get here
				default:
					break;
			}

			break;

		//Enumerated types are 32 bits for convenience
		case TYPE_CLASS_ENUMERATED:
			//An enum is just an integer type, so we can just use the internal type for a size
			size = get_type_size(type->internal_values.enum_integer_type);
			break;

		//These are always 64 bits
		case TYPE_CLASS_POINTER:
		case TYPE_CLASS_ARRAY:
		case TYPE_CLASS_STRUCT:
		case TYPE_CLASS_FUNCTION_SIGNATURE:
		case TYPE_CLASS_ALIAS:
		case TYPE_CLASS_UNION: //always a memory address
			size = QUAD_WORD;
			break;

		//Default is also quad word
		default:
			size = QUAD_WORD;
			break;
	}


	//Give it back
	return size;
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

	//Run through all of our parameters
	for(u_int16_t i = 0; i < function_type->num_params; i++){
		//Extract the parameter type
		generic_type_t* paramter_type = function_type->parameters[i];

		//Generate the mut value there if we don't have it already
		if(paramter_type->mutability == MUTABLE){
			sprintf(var_string, "mut ");
		}

		//First put this into the buffer string
		sprintf(var_string, "%s", paramter_type->type_name.string);

		//Then concatenate
		dynamic_string_concatenate(&(function_pointer_type->type_name), var_string);

		//Add the comma in if need be
		if(i != function_type->num_params - 1){
			//Then concatenate
			dynamic_string_concatenate(&(function_pointer_type->type_name), ", ");
		}
	}

	//If the return type is mutable, we need to generate the mut keyword on it
	if(function_type->return_type->mutability == MUTABLE){
		//First print this to the buffer
		sprintf(var_string, ") -> mut %s", function_type->return_type->type_name.string);
	} else {
		//First print this to the buffer
		sprintf(var_string, ") -> %s", function_type->return_type->type_name.string);
	}


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
 * Perform a symbolic dereference of a type
 */
generic_type_t* dereference_type(generic_type_t* pointer_type){
	//Dev check here
	if(pointer_type->type_class != 	TYPE_CLASS_POINTER){
		printf("Fatal internal compiler error: attempt to dereference a non-pointer\n");
		exit(1);
	}

	//Otherwise, just use the internal storage to get it
	return pointer_type->internal_types.points_to;
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
			dynamic_array_dealloc(type->internal_types.union_table);
			break;
		default:
			break;
	}

	//Destroy the internal type name
	dynamic_string_dealloc(&(type->type_name));

	//Finally just free the overall pointer
	free(type);
}
