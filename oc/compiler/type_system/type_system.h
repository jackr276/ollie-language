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
//The tokens of an enumerated type
typedef struct enumerated_type_token_t enumerated_type_token_t;


/**
 * Which class of type is it?
 */
typedef enum TYPE_CLASS{
	TYPE_CLASS_BASIC,
	TYPE_CLASS_ARRAY,
	TYPE_CLASS_CONSTRUCT,
	TYPE_CLASS_ENUMERATED,
	TYPE_CLASS_POINTER
} TYPE_CLASS;


/**
 * All basic types in the system. This is helpful for
 * quick identification with basic types, which we can assume
 * will be the most common. Allows us to avoid doing string comparisons
 * for basic types
 */
typedef enum BASIC_TYPE{
	TYPE_U_INT8,
	TYPE_S_INT8,
	TYPE_U_INT16,
	TYPE_S_INT16,
	TYPE_U_INT32,
	TYPE_S_INT32,
	TYPE_U_INT64,
	TYPE_S_INT64,
	TYPE_FLOAT32,
	TYPE_FLOAT64,
	TYPE_CHAR,
} BASIC_TYPE;


/**
 * A lot of times we need optionality in a type system. The generic type provides this.
 * For example, in an array, we want the option to have an array of structures, an array of
 * pointers, etc. This generic type allows the array to hold one generic and take action based
 * on what it's holding, as opposed to having several different classes of arrays
 */
struct generic_type_t{
	//What class of type is it
	TYPE_CLASS type_class;
	/**
	 * The following pointers will be null except for the one that the type class
	 * specifies this type belongs to
	 */
	basic_type_t* basic_type;
	array_type_t* array_type;
	pointer_type_t* pointer_type;
	constructed_type_t* construct_type;
	enumerated_type_t* enumerated_type;
};


/**
 * The most basic type that we can have. Encompasses one of the BASIC_TYPEs from
 * above and encodes the pointer_level, size and the name
 */
struct basic_type_t{
	Lexer_item type_lex; //TODO PROBABLY DON'T NEED
	//The type name
	char type_name[MAX_TYPE_NAME_LENGTH];
	//What basic type is it
	BASIC_TYPE basic_type;
	//What is the size of this type?
	u_int32_t size;
};


/**
 * An array type is a linear, contiguous collection of other types in memory.
 */
struct array_type_t{
	//The name of the array
	char type_name[MAX_TYPE_NAME_LENGTH];
	//Whatever the members are in the array
	generic_type_t* member_type;
	//Array bounds
	u_int32_t num_members;
	//The total size
	u_int32_t size;
};


/**
 * A pointer type contains a reference to the memory address of another 
 * variable in memory. This other type may very well itself be a pointer, which is why the
 * actual type pointed to is generic
 */
struct pointer_type_t{
	//The name of the type
	char type_name[MAX_TYPE_NAME_LENGTH];
	//What do we point to?
	generic_type_t* points_to;
	//What is the size
	u_int32_t size; /* Always 8 for a pointer */
};


/**
 * A constructed type contains a list of other types that are inside of it.
 * As such, the type here contains an array of generic types of at most 100
 */
struct constructed_type_t{
	//The name of the type
	char type_name[MAX_TYPE_NAME_LENGTH];
	//What types do we contain?
	generic_type_t members[MAX_CONSTRUCT_MEMBERS];
	//How many members are there?
	u_int8_t num_members;
	//What is the size?
	u_int32_t size;
};


/**
 * Enumerated type tokens. Enumerated type token equality exists only
 * if there is exact string parity
 */
struct enumerated_type_token_t{
	char token_name[MAX_TYPE_NAME_LENGTH];
};


/**
 * An enumerated type has a name and an array of enumeration tokens. These tokens
 * are positionally encoded and this encoding is fixed at declaration
*/
struct enumerated_type_t{
	char type_name[MAX_TYPE_NAME_LENGTH];
	//We need an array of enumerated type tokens
	//We can have at most 500 of these
	enumerated_type_token_t tokens[MAX_ENUMERATED_MEMBERS];
	//The current number of tokens
	u_int8_t token_num;
	//The size of the type
	u_int32_t size;
};


/**
 * A generic, global type that could be any of the core types that we've defined.
 */

#endif
