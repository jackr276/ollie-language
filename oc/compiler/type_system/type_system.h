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
//An enumerated type
typedef struct enumerated_type_t enumerated_type_t;
//A constructed type
typedef struct constructed_type_t constructed_type_t;
//A constructed type field
typedef struct constructed_type_field_t constructed_type_field_t;
//A construct member
typedef struct construct_member_t construct_member_t;
//An aliased type
typedef struct aliased_type_t aliased_type_t;


/**
 * Which class of type is it?
 */
typedef enum TYPE_CLASS{
	TYPE_CLASS_BASIC,
	TYPE_CLASS_ARRAY,
	TYPE_CLASS_CONSTRUCT,
	TYPE_CLASS_ENUMERATED,
	TYPE_CLASS_POINTER,
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
	char type_name[MAX_TYPE_NAME_LENGTH];
	//What class of type is it
	TYPE_CLASS type_class;
	//When was it defined: -1 = generic type
	int32_t line_number;
	//All generic types have a size
	u_int32_t type_size;

	/**
	 * The following pointers will be null except for the one that the type class
	 * specifies this type belongs to
	 */
	basic_type_t* basic_type;
	array_type_t* array_type;
	pointer_type_t* pointer_type;
	constructed_type_t* construct_type;
	enumerated_type_t* enumerated_type;
	aliased_type_t* aliased_type;
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
};


/**
 * The constructed type's individual members
 */
struct constructed_type_field_t{
	//What variable is stored in here?
	void* variable;
	//What kind of padding do we need to ensure alignment?
	u_int16_t padding;
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
	//The current number of members
	u_int8_t num_members;
	//The overall size in bytes of the struct
	u_int32_t size;
	//The size of the largest element in the structure
	u_int32_t largest_member;
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
 * Are two types the exact same?
 *
 * NOTE: WE NEED RAW TYPES FOR THIS RULE
 */
u_int8_t types_equivalent(generic_type_t* typeA, generic_type_t* typeB);

/**
 * Are two types compatible with eachother?
 * 
 * This function returns a reference to the type that you will get IF you try to operate
 * on these types
 *
 * NOTE: WE NEED RAW TYPES FOR THIS RULE
 * 
 * ASSUMPTION: We assume that typeA is the type whom is being assigned to. So we're really asking
 * "can I put something of typeB into a space that expects typeA?"
 */
generic_type_t* types_compatible(generic_type_t* typeA, generic_type_t* typeB);

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
generic_type_t* create_enumerated_type(char* type_name, u_int32_t line_number);

/**
 * Dynamically allocate and create a constructed type
 */
generic_type_t* create_constructed_type(char* type_name, u_int32_t line_number);


/**
 * Add a value into a construct's table
 */
u_int8_t add_construct_member(constructed_type_t* type, void* member_var);


/**
 * Dynamically allocate and create an array type
 */
generic_type_t* create_array_type(generic_type_t* points_to, u_int32_t line_number, u_int32_t num_members);

/**
 * Dynamically allocate and create an aliased type
 */
generic_type_t* create_aliased_type(char* type_name, generic_type_t* aliased_type, u_int32_t line_number);

/**
 * Destroy a type that is no longer in use
*/
void type_dealloc(generic_type_t* type);

#endif /* TYPE_SYSTEM_H */
