/**
 * A header file for the type system of Ollie lang. This type system handles both type definition
 * and type inference, as well as type checking for the parsing stage of the compilation
 *
 * Complex Type Equivalence Philosophy: Name equivalence only. Two complex types are equivalent iff they have the same name
*/
//Include guards
#ifndef TYPE_SYSTEM_H
#define TYPE_SYSTEM_H

#include "../lexer/lexer.h"
#include "../utils/dynamic_array/dynamic_array.h"
#include <sys/types.h>

#define MAX_FUNCTION_TYPE_PARAMS 6

//The generic global type type
typedef struct generic_type_t generic_type_t;
//A function type
typedef struct function_type_t function_type_t;
//A function type's individual parameter
typedef struct function_type_parameter_t function_type_parameter_t;

//A type for which side we're on
typedef enum{
	SIDE_TYPE_LEFT,
	SIDE_TYPE_RIGHT,
} side_type_t;

/**
 * What kind of word length do we have -- used for instructions
 */
typedef enum{
	BYTE,
	WORD,
	DOUBLE_WORD,
	QUAD_WORD,
	SINGLE_PRECISION,
	DOUBLE_PRECISION //For floats
} variable_size_t;


/**
 * Which class of type is it?
 */
typedef enum type_class_t {
	TYPE_CLASS_BASIC,
	TYPE_CLASS_ARRAY,
	TYPE_CLASS_STRUCT,
	TYPE_CLASS_ENUMERATED,
	TYPE_CLASS_POINTER,
	TYPE_CLASS_FUNCTION_SIGNATURE, /* Function pointer type */
	TYPE_CLASS_UNION, /* For discriminating union types */
	TYPE_CLASS_ALIAS /* Alias types */
} type_class_t;


/**
 * A lot of times we need optionality in a type system. The generic type provides this.
 * For example, in an array, we want the option to have an array of structures, an array of
 * pointers, etc. This generic type allows the array to hold one generic and take action based
 * on what it's holding, as opposed to having several different classes of arrays
 */
struct generic_type_t{
	//The name of the type
	dynamic_string_t type_name;

	/**
	 * All fields in this union are
	 * mutually exclusive with one another.
	 * To save space, we'll bundle them all in a union
	 * and then access whichever is needed based on the type 
	 * class
	 */
	union {
		//What is the member type of an array
		generic_type_t* member_type;
		//What does a pointer type point to?
		generic_type_t* points_to;
		//For function pointers
		function_type_t* function_type;
		//Store all values in a struct
		dynamic_array_t* struct_table;
		//The union table
		dynamic_array_t* union_table;
		//The enumeration table stores all values in an enum
		dynamic_array_t* enumeration_table;
		//The aliased type
		generic_type_t* aliased_type;
	} internal_types;

	/**
	 * Some types like array types also store their
	 * number of members. Types like pointer
	 * types store flags that represent whether or not
	 * they are a void pointer. We'll have this
	 * "internal values" field to store these mutually
	 * exclusive fields in an efficient way
	 */
	union {
		//What is the integer type that an enum uses?
		generic_type_t* enum_integer_type;
		//The largest member type in a struct/union
		generic_type_t* largest_member_type;
		//The number of members in an array
		u_int32_t num_members;
		//Is a type a void pointer?
		u_int8_t is_void_pointer;
	} internal_values;

	//When was it defined: -1 = generic type
	int32_t line_number;
	//All generic types have a size
	u_int32_t type_size;
	//Has this type been fully defined or not? This will be used to avoid 
	//struct/union member recursive definitions with incomplete types
	u_int8_t type_complete;

	/**
	 * Is this a mutable type? We need to take this into account whenever doing
	 * any kind of operation checking. Mutable versions of the same type are stored
	 * as separate records
	 */
	u_int8_t is_mutable;
	//Basic types don't need anything crazy - just a token that stores what they are
	ollie_token_t basic_type_token;
	//What class of type is it
	type_class_t type_class;
};


/**
 * A type for storing the individual function parameters themselves
 */
struct function_type_parameter_t{
	//What's the type
	generic_type_t* parameter_type;
	//Is this mutable
	u_int8_t is_mutable;
};


/**
 * A function type is a function signature that is used for function pointers
 * For a function type, we simply need a list of parameters and a return type
 */
struct function_type_t{
	//A list of function parameters. Limited to 6
	function_type_parameter_t parameters[MAX_FUNCTION_TYPE_PARAMS];
	//The return type
	generic_type_t* return_type;
	//Store the number of parameters
	u_int8_t num_params;
	//Does this return a void type?
	u_int8_t returns_void;
	//Is this function public? By default it is not
	u_int8_t is_public;
};


/**
 * Does this type represent a memory region and not
 * a single variable?
 */
u_int8_t is_memory_region(generic_type_t* type);

/**
 * Does this type represent a memory address?
 */
u_int8_t is_memory_address_type(generic_type_t* type);

/**
 * Does assigning from source to destination require a converting move
 */
u_int8_t is_converting_move_required(generic_type_t* destination_type, generic_type_t* source_type);

/**
 * Get the type that we need to align for. On structs, it's the largest
 * primitive member and on arrays, it's the member size
 */
generic_type_t* get_base_alignment_type(generic_type_t* type);

/**
 * Is a type an unsigned 64 bit type? This is used for type conversions in 
 * the instruction selector
 */
u_int8_t is_type_unsigned_64_bit(generic_type_t* type);

/**
 * Is the given type a 32 bit integer type?
 */
u_int8_t is_type_32_bit_int(generic_type_t* type);

/**
 * Get the referenced type regardless of how many indirection levels there are
 */
generic_type_t* get_referenced_type(generic_type_t* starting_type, u_int16_t indirection_level);

/**
 * Is the given type memory movement appropriate
 */
u_int8_t is_type_address_calculation_compatible(generic_type_t* type);

/**
 * Is this type valid for memory addressing?
 */
u_int8_t is_type_valid_for_memory_addressing(generic_type_t* type);

/**
 * Is the type valid to be used in a conditional?
 */
u_int8_t is_type_valid_for_conditional(generic_type_t* type);

/**
 * Do we need an expanding move to convert between two types?
 */
u_int8_t is_expanding_move_required(generic_type_t* destination_type, generic_type_t* source_type);

/**
 * Simple helper to check if a function is void
 */
u_int8_t is_void_type(generic_type_t* type);

/**
 * Determine the compatibility of two types and coerce appropraitely. The double pointer
 * reference exists so that this method may transform types as appropriate
 */
generic_type_t* determine_compatibility_and_coerce(void* type_symtab, generic_type_t** a, generic_type_t** b, ollie_token_t op);

/**
 * Are we able to assign something of "source_type" to something on "destination_type"? Returns
 * NULL if we can't
 */
generic_type_t* types_assignable(generic_type_t* destination_type, generic_type_t* source_type);

/**
 * Dynamically allocate and create a basic type
*/
generic_type_t* create_basic_type(char* type_name, ollie_token_t basic_type);

/**
 * Strip any aliasing away from a type that we have
 */
generic_type_t* dealias_type(generic_type_t* type);

/**
 * Dynamically allocate and create a pointer type
 */
generic_type_t* create_pointer_type(generic_type_t* points_to, u_int32_t line_number);

/**
 * Dynamically allocate and create an enumerated type
 */
generic_type_t* create_enumerated_type(dynamic_string_t type_name, u_int32_t line_number);

/**
 * Dynamically allocate and create a struct type
 */
generic_type_t* create_struct_type(dynamic_string_t type_name, u_int32_t line_number);

/**
 * Dynamically allocate and create a union type
 */
generic_type_t* create_union_type(dynamic_string_t type_name, u_int32_t line_number);

/**
 * Is the given binary operation valid for the type that was specificed?
 */
u_int8_t is_binary_operation_valid_for_type(generic_type_t* type, ollie_token_t binary_op, side_type_t side);

/**
 * Is the given unary operation valid for the type that was specificed?
 */
u_int8_t is_unary_operation_valid_for_type(generic_type_t* type, ollie_token_t unary_op);

/**
 * Is the given unary operation valid for the type that was specificed?
 */
u_int8_t is_unary_operation_valid_for_type(generic_type_t* type, ollie_token_t unary_op);

/**
 * Add a value into a construct's table
 */
void add_struct_member(generic_type_t* type, void* member_var);

/**
 * Add a value to an enumeration's list of values
 */
u_int8_t add_enum_member(generic_type_t* enum_type, void* enum_member, u_int8_t user_defined_values);

/**
 * Add a value into the union's list of members
 */
u_int8_t add_union_member(generic_type_t* union_type, void* member_var);

/**
 * Finalize the struct alignment
 */
void finalize_struct_alignment(generic_type_t* type);

/**
 * Does this struct contain said member? Return the variable if yes, NULL if not
 */
void* get_struct_member(generic_type_t* structure, char* name);

/**
 * Does this union contain said member? Return the variable if yes, NULL if not
 */
void* get_union_member(generic_type_t* union_type, char* name);

/**
 * Dynamically allocate and create an array type
 */
generic_type_t* create_array_type(generic_type_t* points_to, u_int32_t line_number, u_int32_t num_members);

/**
 * Dynamically allocate and create an aliased type
 */
generic_type_t* create_aliased_type(dynamic_string_t type_name, generic_type_t* aliased_type, u_int32_t line_number);

/**
 * Dynamically allocate and create a function pointer type
 */
generic_type_t* create_function_pointer_type(u_int8_t is_public, u_int32_t line_number);

/**
 * Add a function's parameter in
 */
u_int8_t add_parameter_to_function_type(generic_type_t* function_type, generic_type_t* parameter, u_int8_t is_mutable);

/**
 * Print a function pointer type out
 */
void generate_function_pointer_type_name(generic_type_t* function_pointer_type);

/**
 * Perform a symbolic dereference of a type
 */
generic_type_t* dereference_type(generic_type_t* pointer_type);

/**
 * Select the size based only on a type
 */
variable_size_t get_type_size(generic_type_t* type);

/**
 * Is a type signed?
 */
u_int8_t is_type_signed(generic_type_t* type);

/**
 * Is this type equivalent to a char**? This is used
 * exclusively for main function validation
 */
u_int8_t is_type_string_array(generic_type_t* type);

/**
 * Destroy a type that is no longer in use
*/
void type_dealloc(generic_type_t* type);

#endif /* TYPE_SYSTEM_H */
