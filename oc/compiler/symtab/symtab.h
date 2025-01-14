/**
 * The symbol table that is build by the compiler. This is implemented as a hash table
*/


#ifndef SYMTAB_H
#define SYMTAB_H

#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../lexer/lexer.h"
#include "../type_system//type_system.h"

//We define that each lexical scope can have 5000 symbols at most
//Chosen because it's a prime not too close to a power of 2
#define KEYSPACE 4999 
//We figure that 200 separate lexical-levels is enough
#define MAX_SHEAFS 200

typedef struct symtab_t symtab_t;
typedef struct symtab_function_sheaf_t symtab_function_sheaf_t;
typedef struct symtab_variable_sheaf_t symtab_variable_sheaf_t;
typedef struct symtab_function_record_t symtab_function_record_t;
typedef struct symtab_variable_record_t symtab_variable_record_t;
//Parameter lists for functions TODO
typedef struct parameter_list_t parameter_list_t;
//Parameter type
typedef struct parameter_t parameter_t;
//A type holder
typedef struct type_t type_t;

//The storage class of a given item
typedef enum STORAGE_CLASS_T{
	STORAGE_CLASS_STATIC,
	STORAGE_CLASS_EXTERNAL,
	STORAGE_CLASS_NORMAL,
	STORAGE_CLASS_REGISTER
} STORAGE_CLASS_T;


/**
 * Is it a function or variable symtab?
 */
typedef enum SYMTAB_RECORD_TYPE{
	FUNCTION,
	VARIABLE
} SYMTAB_RECORD_TYPE;


/**
 * We want to be able to really quickly and easily determine if 
 * the type that we have is a basic type
 */
typedef enum BASIC_TYPE{
	TYPE_STRUCTURE, /* If it's a complex type */
	TYPE_ENUMERATED,
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
	TYPE_STRING
} BASIC_TYPE;

/**
 * A generic type holder for us
 */
struct type_t{
	Lexer_item type_lex;
	//The type name
	char type_name[100];
	//Is it a basic type?
	BASIC_TYPE basic_type;
	//Is it a pointer? 0 = not, 1 = 1 star, 2 star, etc
	u_int8_t pointer_level;
	//TODO may need more stuff here
};


/**
 * A parameter has a type and a name
 */
struct parameter_t{
	//The associated variable for a parameter, since a parameter is a variable
	symtab_variable_record_t* associate_var;
	//Was it ever referenced?
	u_int8_t referenced;
};


/**
 * A struct that represents a symtab record
 */
struct symtab_function_record_t{
	//The name that we are storing. This is used to derive the hash
	char func_name[100];
	//The hash that we have
	u_int16_t hash;
	//The lexical level of this record
	int16_t lexical_level;
	//The line number
	u_int16_t line_number;
	//Will be used later, the offset for the address in the data area
	u_int64_t offset;
	//Number of parameters
	u_int8_t number_of_params;
	//The parameters
	parameter_t func_params[6];
	//What's the storage class?
	STORAGE_CLASS_T storage_class;
	//What's the return type?
	type_t return_type;
	//Has it been defined?(done to allow for predeclaration)
	u_int8_t defined;
	//In case of collisions, we can chain these records
	symtab_function_record_t* next;
};


/**
 * This struct represents a specific lexical level of a symtab
 */
struct symtab_variable_record_t{
	//Variable name
	char var_name[100];
	//The hash of it
	u_int16_t hash;
	//The lexical level of it
	int16_t lexical_level;
	//Line number
	u_int16_t line_number;
	//The offset
	u_int64_t offset;
	//Was it initialized?
	u_int8_t initialized;
	//Is it a function parameter?
	u_int8_t is_function_paramater;
	//If it is, we'll store the function as a reference
	symtab_function_record_t* parent_function;
	//What's the storage class?
	STORAGE_CLASS_T storage_class;
	//Is it a constant variable?
	u_int8_t is_constant;
	//What type is it?
	type_t type;
	//Was it declared or letted
	u_int8_t declare_or_let; /* 0 = declare, 1 = let */
	//The next hashtable record
	symtab_variable_record_t* next;
};


/**
 * This struct represents a specific lexical level of a symtab
 */
struct symtab_function_sheaf_t{
	//Link to the prior level
	symtab_function_sheaf_t* previous_level;
	//How many records(names) we can have
	symtab_function_record_t* records[KEYSPACE];
	//The level of this particular symtab
	u_int8_t lexical_level;
};


/**
 * This struct represents a specific lexical level of a symtab
 */
struct symtab_variable_sheaf_t{
	//Link to the prior level
	symtab_variable_sheaf_t* previous_level;
	//How many records(names) we can have
	symtab_variable_record_t* records[KEYSPACE];
	//The level of this particular symtab
	u_int8_t lexical_level;
};


/**
 * This struct represents the overall collection of the sheafs of symtabs
 */
struct symtab_t{
	//The next index that we'll insert into
	u_int16_t next_index;

	//Are we storing functions or variables
	SYMTAB_RECORD_TYPE type;

	//A global storage array for all symtab "sheaths"
	void* sheafs[MAX_SHEAFS];

	//The current symtab sheaf
	void* current;

	//The current lexical scope
	u_int16_t current_lexical_scope;
};


/**
 * Initialize a symbol table. In our compiler, we may have many symbol tables, so it's important
 * that we're able to initialize separate ones and keep them distinct
 */
symtab_t* initialize_symtab(SYMTAB_RECORD_TYPE record_type);


/**
 * Initialize the symbol table
 */
void initialize_scope(symtab_t* symtab);


/**
 * Finalize the scope and go back a level
 */
void finalize_scope(symtab_t* symtab);


/**
 * Create a record for the symbol table
 */
symtab_variable_record_t* create_variable_record(char* name, STORAGE_CLASS_T storage_class);

/**
 * Make a function record
 */
symtab_function_record_t* create_function_record(char* name, STORAGE_CLASS_T storage_class);


/**
 * Insert a name into the symbol table
 */
u_int8_t insert(symtab_t* symtab, void* record);


/**
 * Lookup a name in the symtab
 */
void* lookup(symtab_t* symtab, char* name);


/**
 * A printing function for development purposes
 */
void print_function_record(symtab_function_record_t* record);

/**
 * A printing function for development purposes
 */
void print_variable_record(symtab_variable_record_t* record);

/**
 * A helper method for function name printing
 */
void print_function_name(symtab_function_record_t* record);

/**
 * A helper method for variable name printing
 */
void print_variable_name(symtab_variable_record_t* record);

/**
 * Deinitialize the symbol table
 */
void destroy_symtab(symtab_t*);


#endif /* SYMTAB_H */
