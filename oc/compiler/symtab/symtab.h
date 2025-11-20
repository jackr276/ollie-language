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
#include "../utils/stack/lightstack.h"
#include "../utils/dynamic_array/dynamic_array.h"
#include "../utils/constants.h"
//Every function record has one of these
#include "../stack_data_area/stack_data_area.h"


//We define that each lexical scope can have 5000 symbols at most
//Chosen because it's a prime not too close to a power of 2
#define KEYSPACE 997
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
//The definition of a local constant(.LCx) block
typedef struct local_constant_t local_constant_t;


/**
 * What is the membership that a variable has?
 */
typedef enum variable_membership_t {
	NO_MEMBERSHIP = 0, //Generic var, no type/function members
	STRUCT_MEMBER = 1,
	UNION_MEMBER = 2,
	ENUM_MEMBER = 3,
	GLOBAL_VARIABLE = 4,
	FUNCTION_PARAMETER = 5,
	LABEL_VARIABLE = 6,
} variable_membership_t;


/**
 * A local constant(.LCx) is a value like a string that is intended to 
 * be used by a function. We define them separately because they have many less
 * fields than an actual basic block
 */
struct local_constant_t{
	//The actual string value of it
	dynamic_string_t value;
	//And the ID of it
	u_int16_t local_constant_id;
	//The reference count of the local constant
	u_int16_t reference_count;
};


/**
 * The symtab function record. This stores data about the function's name, parameter
 * numbers, parameter types, return types, etc.
 */
struct symtab_function_record_t{
	//The parameters for the function
	symtab_variable_record_t* func_params[MAX_FUNCTION_PARAMS];
	//What registers does the function assign to in its body
	u_int8_t assigned_registers[K_COLORS_GEN_USE];
	//The name of the function
	dynamic_string_t func_name;
	//The data area for the whole function
	stack_data_area_t data_area;
	//The associated call graph node with this function
	void* call_graph_node;
	//In case of collisions, we can chain these records
	symtab_function_record_t* next;
	//The type of the function
	generic_type_t* signature;
	//What's the return type?
	generic_type_t* return_type;
	//The local constants array. Not all functions 
	//have this populated
	dynamic_array_t* local_constants;
	//The hash that we have
	u_int16_t hash;
	//The line number
	u_int16_t line_number;
	//Number of parameters
	u_int8_t number_of_params;
	//Has it been defined?(done to allow for predeclaration)(0 = declared only, 1 = defined)
	u_int8_t defined;
	//Has it ever been called?
	u_int8_t called;
};


/**
 * The symtab variable record. This stores data about the variable's name,
 * lexical level, line_number, parent function, etc.
 */
struct symtab_variable_record_t{
	//The variable name
	dynamic_string_t var_name;
	//For SSA renaming
	lightstack_t counter_stack;
	//What function was it declared in?
	symtab_function_record_t* function_declared_in;
	//What type is it?
	generic_type_t* type_defined_as;
	//The next hashtable record
	symtab_variable_record_t* next;
	//What is the enum member value
	u_int64_t enum_member_value;
	//The associate region that this variable is stored in
	stack_region_t* stack_region;
	//The current generation of the variable - FOR SSA in CFG
	u_int16_t current_generation;
	//The hash of it
	u_int16_t hash;
	//The lexical level of it
	int16_t lexical_level;
	//Current generation level(for SSA)
	u_int16_t counter;
	//Line number
	u_int16_t line_number;
	//What is the struct offset for this variable
	u_int16_t struct_offset;
	//Was it initialized?
	u_int8_t initialized;
	//Has this been mutated
	u_int8_t mutated;
	//What is the parameter order for this value?
	u_int8_t function_parameter_order;
	//Is this mutable?
	u_int8_t is_mutable;
	//What type structure or language concept does this variable belong to?
	variable_membership_t membership;
	//Where does this variable get stored? By default we assume register, so
	//this flag will only be set if we have a memory address value
	u_int8_t stack_variable;
	//Was it declared or letted
	u_int8_t declare_or_let; /* 0 = declare, 1 = let */
};


/**
 * This struct represents a specific type record in the symtab. This is how we 
 * will keep references to all created types like structs, enums, etc
 */
struct symtab_type_record_t{
	//The next hashtable record
	symtab_type_record_t* next;
	//What type is it?
	generic_type_t* type;
	u_int16_t hash;
	//The lexical level of it
	int16_t lexical_level;
	//Line number
	u_int16_t line_number;
};


/**
 * This struct represents a specific constant record in the compiler. This is
 * how we will keep references to constants as they're defined by the user
 */
struct symtab_constant_record_t{
	//The name as a dynamic string
	dynamic_string_t name;
	//We'll link directly to the constant node here
	void* constant_node;
	//For linked list functionality
	symtab_constant_record_t* next;
	u_int16_t hash;
	//Line number
	u_int16_t line_number;
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
	//Dynamic array of sheafs
	dynamic_array_t* sheafs;
	//The current symtab sheaf
	symtab_variable_sheaf_t* current;
	//The current lexical scope
	u_int16_t current_lexical_scope;
};


/**
 * This struct represents the overall collection of the sheafs of symtabs
 */
struct type_symtab_t{
	//Dynamic array of sheafs
	dynamic_array_t* sheafs;
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
symtab_variable_record_t* create_variable_record(dynamic_string_t name);

/**
 * Create a ternary variable record
 */
symtab_variable_record_t* create_ternary_variable(generic_type_t* type, variable_symtab_t* variable_symtab, u_int32_t temp_id);

/**
 * Add a parameter to a function and perform all internal bookkeeping needed
 */
u_int8_t add_function_parameter(symtab_function_record_t* function_record, symtab_variable_record_t* variable_record);

/**
 * Make a function record
 */
symtab_function_record_t* create_function_record(dynamic_string_t name, u_int8_t is_public, u_int32_t line_number);

/**
 * Create a type record for the symbol table
 */
symtab_type_record_t* create_type_record(generic_type_t* type);

/**
 * Create a type record for the constant table. Unlike our
 * other rules, this rule will actually have most of it's processing
 * done by the client
 */
symtab_constant_record_t* create_constant_record(dynamic_string_t name);

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
 * Initialize the global stack pointer variable for us to use
 */
symtab_variable_record_t* initialize_stack_pointer(type_symtab_t* types);

/** 
 * Create the instruction pointer(rip) variable for us to use throughout
 */
symtab_variable_record_t* initialize_instruction_pointer(type_symtab_t* types);

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
symtab_type_record_t* lookup_type(type_symtab_t* symtab, generic_type_t* type);

/**
 * Lookup a type name in the symtab by the name only. This does not
 * do the array bound comparison that we need for strict equality
 */
symtab_type_record_t* lookup_type_name_only(type_symtab_t* symtab, char* name);

/**
 * Create a local constant
 */
local_constant_t* local_constant_alloc(dynamic_string_t* value);

/**
 * Add a local constant to a function
 */
void add_local_constant_to_function(symtab_function_record_t* function, local_constant_t* constant);

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
 * Print the local constants(.LCx) that are inside of a function
 */
void print_local_constants(FILE* fl, symtab_function_record_t* record);

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

/**
 * Destroy a local constant
 */
void local_constant_dealloc(local_constant_t* constant);

#endif /* SYMTAB_H */
