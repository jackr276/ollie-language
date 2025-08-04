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
#include <sys/types.h>

#define OUT_OF_BOUNDS 3
#define MAX_FUNCTION_TYPE_PARAMS 6

//Type names may not exceed 200 characters in length
#define MAX_TYPE_NAME_LENGTH 200
//The maximum number of members in a construct
#define MAX_CONSTRUCT_MEMBERS 100
//The maximum number of members in an enumerated
#define MAX_ENUMERATED_MEMBERS 200


//The generic global type type
typedef struct generic_type_t generic_type_t;
//The most basic type that we can have
typedef struct basic_type_t basic_type_t;
//An array type
typedef struct array_type_t array_type_t;
//A pointer type
typedef struct pointer_type_t pointer_type_t;
//A function type
typedef struct function_type_t function_type_t;
//A function type's individual parameter
typedef struct function_type_parameter_t function_type_parameter_t;
//An enumerated type
typedef struct enumerated_type_t enumerated_type_t;
//A constructed type
typedef struct constructed_type_t constructed_type_t;
//A constructed type field
typedef struct constructed_type_field_t constructed_type_field_t;
//An aliased type
typedef struct aliased_type_t aliased_type_t;
//Compiler option type
typedef struct compiler_options_t compiler_options_t;


/**
 * Define an enum that stores all compiler options.
 * This struct will be used throughout the compiler
 * to tell us what to print out
 */
struct compiler_options_t {
	//The name of the file(-f)
	char* file_name;
	//The name of the output file(-o )
	char* output_file;
	//Do we want to skip outputting
	//to assembly? 
	u_int8_t skip_output;
	//Enable all debug printing 
	u_int8_t enable_debug_printing;
	//Print out summary? 
	u_int8_t show_summary;
	//Only output assembly(no .o)
	u_int8_t go_to_assembly; 
	//Time execution for performance testing
	u_int8_t time_execution;
	//Is this a CI run?
	u_int8_t is_test_run;
	//Print intermediate representations
	u_int8_t print_irs;
};


//A type for which side we're on
typedef enum{
	SIDE_TYPE_LEFT,
	SIDE_TYPE_RIGHT,
} side_type_t;


/**
 * Which class of type is it?
 */
typedef enum TYPE_CLASS{
	TYPE_CLASS_BASIC,
	TYPE_CLASS_ARRAY,
	TYPE_CLASS_CONSTRUCT,
	TYPE_CLASS_ENUMERATED,
	TYPE_CLASS_POINTER,
	TYPE_CLASS_FUNCTION_SIGNATURE, /* Function pointer type */
	TYPE_CLASS_ALIAS /* Alias types */
} TYPE_CLASS;


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
	 * The following pointers will be null except for the one that the type class
	 * specifies this type belongs to
	 */
	basic_type_t* basic_type;
	array_type_t* array_type;
	pointer_type_t* pointer_type;
	//For function pointers
	function_type_t* function_type;
	constructed_type_t* construct_type;
	enumerated_type_t* enumerated_type;
	aliased_type_t* aliased_type;
	//When was it defined: -1 = generic type
	int32_t line_number;
	//All generic types have a size
	u_int32_t type_size;
	//What class of type is it
	TYPE_CLASS type_class;
};


/**
 * The most basic type that we can have. Encompasses one of the BASIC_TYPEs from
 * above and encodes the pointer_level, size and the name
 */
struct basic_type_t{
	//What basic type is it
	Token basic_type;
	//Is it a label?
	u_int8_t is_label;
};


/**
 * An array type is a linear, contiguous collection of other types in memory.
 */
struct array_type_t{
	//Whatever the members are in the array
	generic_type_t* member_type;
	//Array bounds
	u_int32_t num_members;
};


/**
 * A pointer type contains a reference to the memory address of another 
 * variable in memory. This other type may very well itself be a pointer, which is why the
 * actual type pointed to is generic
 */
struct pointer_type_t{
	//What do we point to?
	generic_type_t* points_to;
	//Is this a void*
	u_int8_t is_void_pointer;
};


/**
 * The constructed type's individual members
 */
struct constructed_type_field_t{
	//What variable is stored in here?
	void* variable;
	//What kind of padding do we need to ensure alignment?
	u_int16_t padding;
	//What is the offset(address) in bytes from the start of this field
	u_int16_t offset;
};


/**
 * A constructed type contains a list of other types that are inside of it.
 * As such, the type here contains an array of generic types of at most 100
 */
struct constructed_type_t{
	//We will store internally a pre-aligned construct table. The construct
	//table itself will be aligned internally, and we'll use the given order of
	//the construct members to compute it. Due to this, it would be advantageous for
	//the programmer to order the structure table with larger elements first
	constructed_type_field_t construct_table[MAX_CONSTRUCT_MEMBERS];
	//The overall size in bytes of the struct
	u_int32_t size;
	//The size of the largest member
	u_int32_t largest_member_size;
	//The next index
	u_int8_t next_index;
};


/**
 * An enumerated type has a name and an array of enumeration tokens. These tokens
 * are positionally encoded and this encoding is fixed at declaration
*/
struct enumerated_type_t{
	//The number of enumerated types
	void* tokens[MAX_ENUMERATED_MEMBERS];
	//The current number of tokens
	u_int8_t token_num;
};


/**
 * An aliased type is quite simply a name that points to the real type. This can
 * be used by the programmer to define simpler names for themselves
 */
struct aliased_type_t{
	//What does it point to?
	generic_type_t* aliased_type;
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
};


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
 * Is a type conversion needed between these two types for b to fit into a
 */
u_int8_t is_type_conversion_needed(generic_type_t* a, generic_type_t* b);

/**
 * Determine the compatibility of two types and coerce appropraitely. The double pointer
 * reference exists so that this method may transform types as appropriate
 */
generic_type_t* determine_compatibility_and_coerce(void* type_symtab, generic_type_t** a, generic_type_t** b, Token op);

/**
 * Are we able to assign something of "source_type" to something on "destination_type"? Returns
 * NULL if we can't
 */
generic_type_t* types_assignable(generic_type_t** destination_type, generic_type_t** source_type);

/**
 * Dynamically allocate and create a basic type
*/
generic_type_t* create_basic_type(char* type_name, Token basic_type);

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
 * Dynamically allocate and create a constructed type
 */
generic_type_t* create_constructed_type(dynamic_string_t type_name, u_int32_t line_number);

/**
 * Is the given binary operation valid for the type that was specificed?
 */
u_int8_t is_binary_operation_valid_for_type(generic_type_t* type, Token binary_op, side_type_t side);

/**
 * Is the given unary operation valid for the type that was specificed?
 */
u_int8_t is_unary_operation_valid_for_type(generic_type_t* type, Token unary_op);

/**
 * Is the given unary operation valid for the type that was specificed?
 */
u_int8_t is_unary_operation_valid_for_type(generic_type_t* type, Token unary_op);

/**
 * Add a value into a construct's table
 */
u_int8_t add_construct_member(generic_type_t* type, void* member_var);

/**
 * Finalize the construct alignment
 */
void finalize_construct_alignment(generic_type_t* type);

/**
 * Does a constructed type contain a given member variable?
 */
constructed_type_field_t* get_construct_member(constructed_type_t* construct, char* name);

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
generic_type_t* create_function_pointer_type(u_int32_t line_number);

/**
 * Print a function pointer type out
 */
void print_function_pointer_type(generic_type_t* function_pointer_type);

/**
 * Is a type signed?
 */
u_int8_t is_type_signed(generic_type_t* type);

/**
 * Destroy a type that is no longer in use
*/
void type_dealloc(generic_type_t* type);

#endif /* TYPE_SYSTEM_H */
