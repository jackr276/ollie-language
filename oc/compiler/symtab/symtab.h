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
#include "../type_system/type_system.h"
#include "../stack/lightstack.h"


//We define that each lexical scope can have 5000 symbols at most
//Chosen because it's a prime not too close to a power of 2
#define KEYSPACE 997
//By default we have 100 sheafs. Sheafs are dynamically resizable however
#define MAX_SHEAFS 50
//The maximum number of function paramaters
#define MAX_FUNCTION_PARAMS 6

//A variable symtab
typedef struct variable_symtab_t variable_symtab_t;
//A function symtab
typedef struct function_symtab_t function_symtab_t;
//A type symtab
typedef struct type_symtab_t type_symtab_t;
//A constants symtab for #replace directives
typedef struct constants_symtab_t constants_symtab_t;

//The sheafs in the variable symtab
typedef struct symtab_variable_sheaf_t symtab_variable_sheaf_t;
//The sheafs in the type symtab
typedef struct symtab_type_sheaf_t symtab_type_sheaf_t;

//The records in the function symtab
typedef struct symtab_function_record_t symtab_function_record_t;
//The records in a variable symtab
typedef struct symtab_variable_record_t symtab_variable_record_t;
//The records in a type symtab
typedef struct symtab_type_record_t symtab_type_record_t;
//The records in a constants symtab
typedef struct symtab_constant_record_t symtab_constant_record_t;

//Parameter type
typedef struct parameter_t parameter_t;


//The storage class of a given item
typedef enum STORAGE_CLASS_T{
	STORAGE_CLASS_STATIC,
	STORAGE_CLASS_EXTERNAL,
	STORAGE_CLASS_NORMAL,
	STORAGE_CLASS_REGISTER
} STORAGE_CLASS_T;


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
 * The symtab function record. This stores data about the function's name, parameter
 * numbers, parameter types, return types, etc.
 */
struct symtab_function_record_t{
	//The entrance CFG block to the function. There is always only one entrance
	void* entrance_block;
	//The name that we are storing. This is used to derive the hash
	char func_name[MAX_IDENT_LENGTH];
	//The module it was defined in
	char module_defined_in[MAX_IDENT_LENGTH];
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
	parameter_t func_params[MAX_FUNCTION_PARAMS];
	//What's the storage class?
	STORAGE_CLASS_T storage_class;
	//What's the return type?
	generic_type_t* return_type;
	//Has it been defined?(done to allow for predeclaration)(0 = declared only, 1 = defined)
	u_int8_t defined;
	//Has it ever been called?
	u_int8_t called;
	//The associated call graph node with this function
	void* call_graph_node;
	//In case of collisions, we can chain these records
	symtab_function_record_t* next;
};


/**
 * The symtab variable record. This stores data about the variable's name,
 * lexical level, line_number, parent function, etc.
 */
struct symtab_variable_record_t{
	//Variable name
	char var_name[MAX_IDENT_LENGTH];
	//The module it was defined in
	char module_defined_in[MAX_IDENT_LENGTH];
	//The current generation of the variable - FOR SSA in CFG
	u_int16_t current_generation;
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
	//Was it assigned to?
	u_int8_t assigned_to;
	//Is it a function parameter?
	u_int8_t is_function_paramater;
	//Is this mutable?
	u_int8_t is_mutable;
	//Is it an enumeration member?
	u_int8_t is_enumeration_member;
	//What is the enum member value
	u_int16_t enum_member_value;
	//Is it a struct member?
	u_int8_t is_construct_member;
	//Is this a label?
	u_int8_t is_label;
	//Is this variable ever required, or considered live, by a basic block?
	u_int8_t has_ever_been_live;
	//If it is, we'll store the function as a reference
	symtab_function_record_t* parent_function;
	//What's the storage class?
	STORAGE_CLASS_T storage_class;
	//What type is it?
	generic_type_t* type;
	//What struct was it defined in? For structs only
	generic_type_t* struct_defined_in;
	//What function was it declared in?
	symtab_function_record_t* function_declared_in;
	//Was it declared or letted
	u_int8_t declare_or_let; /* 0 = declare, 1 = let */
	//The next hashtable record
	symtab_variable_record_t* next;
	//For SSA renaming
	lightstack_t counter_stack;
	//Current generation level(for SSA)
	u_int16_t counter;
};


/**
 * This struct represents a specific type record in the symtab. This is how we 
 * will keep references to all created types like structs, enums, etc
 */
struct symtab_type_record_t{
	u_int16_t hash;
	//The lexical level of it
	int16_t lexical_level;
	//Line number
	u_int16_t line_number;
	//Was it initialized? This is usually for forward-declared structs
	u_int8_t initialized;
	//What type is it?
	generic_type_t* type;
	//The next hashtable record
	symtab_type_record_t* next;
};


/**
 * This struct represents a specific constant record in the compiler. This is
 * how we will keep references to constants as they're defined by the user
 */
struct symtab_constant_record_t{
	u_int16_t hash;
	//Line number
	u_int16_t line_number;
	//The name
	char name[MAX_IDENT_LENGTH];
	//The module it was defined in
	char module_defined_in[MAX_IDENT_LENGTH];
	//We'll link directly to the constant node here
	void* constant_node;
	//For linked list functionality
	symtab_constant_record_t* next;
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
 * This structure represents a specific lexical level of a type symtab
 */
struct symtab_type_sheaf_t{
	//Link to the prior level
	symtab_type_sheaf_t* previous_level;
	//The hash table for our records
	symtab_type_record_t* records[KEYSPACE];
	//The lexical level of this sheaf
	u_int8_t lexical_level;
};


/**
 * This struct represents the overall collection of the sheafs of symtabs
 */
struct variable_symtab_t{
	//A global storage array for all symtab "sheaths"
	symtab_variable_sheaf_t** sheafs;
	
	//The number of sheafs that we currently have
	u_int16_t num_sheafs;

	//The current maximum number of sheafs
	u_int16_t max_sheafs;
	
	//The current symtab sheaf
	symtab_variable_sheaf_t* current;

	//The current lexical scope
	u_int16_t current_lexical_scope;
};


/**
 * This struct represents the overall collection of the sheafs of symtabs
 */
struct type_symtab_t{
	//A global storage array for all symtab "sheaths"
	symtab_type_sheaf_t** sheafs;
	
	//The number of sheafs that we currently have
	u_int16_t num_sheafs;

	//The current maximum number of sheafs
	u_int16_t max_sheafs;

	//The current symtab sheaf
	symtab_type_sheaf_t* current;

	//The current lexical scope
	u_int16_t current_lexical_scope;
};


/**
 * This struct represents the constants symtab. Much like the function symtab, 
 * there is only one lexical level, so no sheafs exist here. All constants
 * declared with #replace are global across all files
 */
struct constants_symtab_t{
	//How many records(names) we can have
	symtab_constant_record_t* records[KEYSPACE];
};


/**
 * There is only one namespace for functions, that being the global namespace.
 * As such, there are no "sheafs" like we have for types or variables
 */
struct function_symtab_t{
	//How many records(names) we can have
	symtab_function_record_t* records[KEYSPACE];
	//The level of this particular symtab
	u_int8_t current_lexical_scope;
};


/**
 * Initialize a function symtab
 */
function_symtab_t* function_symtab_alloc();


/**
 * Initialize a symbol table for variables.
 */
variable_symtab_t* variable_symtab_alloc();


/**
 * Initialize a symbol table for types
 */
type_symtab_t* type_symtab_alloc();


/**
 * Initialize a symbol table for constants
 */
constants_symtab_t* constants_symtab_alloc();


/**
 * NOTE: Functions only have one scope, which is why they do not
 * have any initialize_scope routine
 */

/**
 * Initialize the variable symbol table scope
 */
void initialize_variable_scope(variable_symtab_t* symtab);

/**
 * Initialize the type symbol table scope
 */
void initialize_type_scope(type_symtab_t* symtab);

/**
 * NOTE: Functions only have one scope, which is why they do not
 * have any finalize_scope routine
 */

/**
 * Finalize the variable scope and go back a level
 */
void finalize_variable_scope(variable_symtab_t* symtab);

/**
 * Finalize the variable scope and go back a level
 */
void finalize_type_scope(type_symtab_t* symtab);

/**
 * Create a record for the symbol table
 */
symtab_variable_record_t* create_variable_record(char* name, STORAGE_CLASS_T storage_class);

/**
 * Make a function record
 */
symtab_function_record_t* create_function_record(char* name, STORAGE_CLASS_T storage_class);

/**
 * Create a type record for the symbol table
 */
symtab_type_record_t* create_type_record(generic_type_t* type);

/**
 * Create a type record for the constant table. Unlike our
 * other rules, this rule will actually have most of it's processing
 * done by the client
 */
symtab_constant_record_t* create_constant_record(char* name);

/**
 * Insert a function into the symbol table
 */
u_int8_t insert_function(function_symtab_t* symtab, symtab_function_record_t* record);

/**
 * Insert variables into the symbol table
 */
u_int8_t insert_variable(variable_symtab_t* symtab, symtab_variable_record_t* record);

/**
 * Insert types into the type symtab
 */
u_int8_t insert_type(type_symtab_t* symtab, symtab_type_record_t* record);

/**
 * Insert a constant into the symtab
 */
u_int8_t insert_constant(constants_symtab_t* symtab, symtab_constant_record_t* record);

/**
 * A helper function that adds all basic types to the type symtab
 */
void add_all_basic_types(type_symtab_t* symtab);

/**
 * Lookup a function name in the symtab
 */
symtab_function_record_t* lookup_function(function_symtab_t* symtab, char* name);

/**
 * Lookup a variable name in the symtab
 */
symtab_variable_record_t* lookup_variable(variable_symtab_t* symtab, char* name);

/**
 * Lookup a constant in the symtab
 */
symtab_constant_record_t* lookup_constant(constants_symtab_t* symtab, char* name);

/**
 * Lookup a variable name in the symtab, only one scope
 */
symtab_variable_record_t* lookup_variable_local_scope(variable_symtab_t* symtab, char* name);

/**
 * Lookup a variable in all lower scopes. This is specifically and only intended for
 * jump statements
 */
symtab_variable_record_t* lookup_variable_lower_scope(variable_symtab_t* symtab, char* name);

/**
 * Lookup a type name in the symtab
 */
symtab_type_record_t* lookup_type(type_symtab_t* symtab, char* name);


/**
 * Check for and print out any unused functions
 */
void check_for_unused_functions(function_symtab_t* symtab, u_int32_t* num_warnings);

/**
 * Run through and check for any unused vars, bad mut keywords, etc
 */
void check_for_var_errors(variable_symtab_t* symtab, u_int32_t* num_warnings);

/**
 * A printing function for development purposes
 */
void print_function_record(symtab_function_record_t* record);

/**
 * A printing function for development purposes
 */
void print_variable_record(symtab_variable_record_t* record);

/**
 * A printing function for development purposes
 */
void print_type_record(symtab_type_record_t* record);

/**
 * A helper method for function name printing
 */
void print_function_name(symtab_function_record_t* record);

/**
 * A helper method for variable name printing
 */
void print_variable_name(symtab_variable_record_t* record);

/**
 * A helper method for constant name printing
 */
void print_constant_name(symtab_constant_record_t* record);

/**
 * A helper method for type name printing
 */
void print_type_name(symtab_type_record_t* record);

/**
 * Destroy a function symtab
 */
void function_symtab_dealloc(function_symtab_t* symtab);

/**
 * Destroy a variable symtab
 */
void variable_symtab_dealloc(variable_symtab_t* symtab);

/**
 * Destroy a type symtab 
 */
void type_symtab_dealloc(type_symtab_t* symtab);

/**
 * Destroy a constants symtab
 */
void constants_symtab_dealloc(constants_symtab_t* symtab);

#endif /* SYMTAB_H */
