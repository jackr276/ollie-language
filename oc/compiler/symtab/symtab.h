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
#include "../utils/dynamic_set/dynamic_set.h"
#include "../utils/dynamic_array/dynamic_array.h"
#include "../utils/constants.h"
//Every function record has one of these
#include "../stack_data_area/stack_data_area.h"

//Variables and types have a new sheaf added upon every new lexical scope. As such,
//we don't need enormous sizes to hold all of them
#define VARIABLE_KEYSPACE 128

//Type keyspace is made larger to accomodate more basic type classes, aliases & variants
#define TYPE_KEYSPACE 256

//The macro keyspace is also one per program
#define MACRO_KEYSPACE 256 

//There's only one function keyspace per program, so it can be a bit larger
#define FUNCTION_KEYSPACE 1024 

//The maximum number of function paramaters
#define MAX_FUNCTION_PARAMS 6

//A variable symtab
typedef struct variable_symtab_t variable_symtab_t;
//A function symtab
typedef struct function_symtab_t function_symtab_t;
//A type symtab
typedef struct type_symtab_t type_symtab_t;
//A symtab for #macro directives
typedef struct macro_symtab_t macro_symtab_t;

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
//The records in a macro symtab
typedef struct symtab_macro_record_t symtab_macro_record_t;

//================================ Utility Macros ============================
/**
 * Determine whether a given variable is a function parameter itself or is
 * just the alias of one
 */
#define IS_ORIGINAL_FUNCTION_PARAMETER(parameter)\
	((parameter->alias == NULL) ? TRUE : FALSE)
//================================ Utility Macros ============================


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
	RETURNED_VARIABLE = 7, //Is this returned by a function?
} variable_membership_t;


/**
 * Is a given function public or private? Simple enum for
 * this 
 */
typedef enum {
	FUNCTION_VISIBILITY_PRIVATE,
	FUNCTION_VISIBILITY_PUBLIC
} function_visibility_t;


/**
 * The symtab function record. This stores data about the function's name, parameter
 * numbers, parameter types, return types, etc.
 *
 * To enable call-graph functionality, the symtab function record itself also stores
 * an adjacency list of all the functions that it calls. This allows us to have just one
 * single source of truth for everything relating to a given function
 */
struct symtab_function_record_t{
	//The hash that we have
	u_int64_t hash;
	//In case of collisions, we can chain these records
	symtab_function_record_t* next;
	//All of the basic blocks that make up this function
	dynamic_array_t function_blocks;
	//The parameters for the function
	symtab_variable_record_t* func_params[MAX_FUNCTION_PARAMS];
	//The name of the function
	dynamic_string_t func_name;
	//The data area for the whole function
	stack_data_area_t data_area;
	//The type of the function
	generic_type_t* signature;
	//The list of all functions that this function calls out to
	dynamic_set_t called_functions;
	//What's the return type?
	generic_type_t* return_type;
	//The line number
	u_int32_t line_number;
	//A bitmap for all assigned general purpose registers
	u_int32_t assigned_general_purpose_registers;
	//A bitmap for all assigned SSE registers
	u_int32_t assigned_sse_registers;
	//How many functions call this function?
	u_int32_t called_by_count;
	//Unique identifier that is not a name
	u_int32_t function_id;
	//Number of parameters
	u_int8_t number_of_params;
	//Has it been defined?(done to allow for predeclaration)(0 = declared only, 1 = defined)
	u_int8_t defined;
	//Has it ever been called?
	u_int8_t called;
	//Has this function been inlined?
	u_int8_t inlined;
	//Is this function public or private
	function_visibility_t function_visibility;
};


/**
 * The symtab variable record. This stores data about the variable's name,
 * lexical level, line_number, parent function, etc.
 */
struct symtab_variable_record_t{
	//The hash of it
	u_int64_t hash;
	//The next hashtable record
	symtab_variable_record_t* next;
	//The variable name
	dynamic_string_t var_name;
	//For SSA renaming
	lightstack_t counter_stack;
	//What function was it declared in?
	symtab_function_record_t* function_declared_in;
	//What type is it?
	generic_type_t* type_defined_as;
	//We are able to alias variables as other variables. This is
	//typically only used for function parameters in the presaving step
	symtab_variable_record_t* alias;
	//The associate region that this variable is stored in
	stack_region_t* stack_region;
	//The line number
	u_int32_t line_number;
	//What is the enum member value
	int32_t enum_member_value;
	//The current generation of the variable - FOR SSA in CFG
	u_int16_t current_generation;
	//The lexical level of it
	int16_t lexical_level;
	//Current generation level(for SSA)
	u_int16_t counter;
	//What is the struct offset for this variable
	u_int16_t struct_offset;
	//What is the parameter order for this value?
	u_int16_t absolute_function_parameter_order;
	//What is the relative parameter order for this value? In other words,
	//what is the SSE parameter number or the general purpose parameter number.
	//This is what really matters to us in the register allocator
	u_int16_t class_relative_function_parameter_order;
	//Was it initialized?
	u_int8_t initialized;
	//Has this been mutated
	u_int8_t mutated;
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
	//The hash of it
	u_int64_t hash;
	//The next hashtable record
	symtab_type_record_t* next;
	//What type is it?
	generic_type_t* type;
	//THe link number
	u_int32_t line_number;
	//The lexical level of it
	int16_t lexical_level;
};


/**
 * This struct represents a specific macro record in the compiler. This is
 * how we will keep references to macros as they're defined by the user
 */
struct symtab_macro_record_t{
	//The hash of it
	u_int64_t hash;
	//For linked list functionality
	symtab_macro_record_t* next;
	//The name as a dynamic string
	dynamic_string_t name;
	//The array of all tokens in the macro
	ollie_token_array_t tokens;
	//All of the parameters in the macro itself are their own individual(yet small) token
	//arrays
	dynamic_array_t parameters;
	//The total token lenght, including the begin/end tokens
	u_int32_t total_token_count;
	//Line number of declaration
	u_int32_t line_number;
};


/**
 * This struct represents a specific lexical level of a symtab
 */
struct symtab_variable_sheaf_t{
	//Link to the prior level
	symtab_variable_sheaf_t* previous_level;
	//How many records(names) we can have
	symtab_variable_record_t* records[VARIABLE_KEYSPACE];
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
	symtab_type_record_t* records[TYPE_KEYSPACE];
	//The lexical level of this sheaf
	u_int8_t lexical_level;
};


/**
 * This struct represents the overall collection of the sheafs of symtabs
 */
struct variable_symtab_t{
	//Dynamic array of sheafs
	dynamic_array_t sheafs;
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
	dynamic_array_t sheafs;
	//The current symtab sheaf
	symtab_type_sheaf_t* current;
	//The current lexical scope
	u_int16_t current_lexical_scope;
};


/**
 * This struct represents the macro symtab. Much like the function symtab, 
 * there is only one lexical level, so no sheafs exist here
 */
struct macro_symtab_t{
	//How many records(names) we can have
	symtab_macro_record_t* records[MACRO_KEYSPACE];
};


/**
 * There is only one namespace for functions, that being the global namespace.
 * As such, there are no "sheafs" like we have for types or variables
 */
struct function_symtab_t{
	//How many records(names) we can have
	symtab_function_record_t* records[FUNCTION_KEYSPACE];

	//The adjacency matrix for the call graph
	u_int8_t* call_graph_matrix;

	//The transitive closure for the call graph
	u_int8_t* call_graph_transitive_closure;

	//The current function id
	u_int32_t current_function_id;

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
 * Initialize a symbol table for compiler macros 
 */
macro_symtab_t* macro_symtab_alloc();

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
 * Create a parameter alias variable record
 */
symtab_variable_record_t* create_parameter_alias_variable(symtab_variable_record_t* aliases, variable_symtab_t* variable_symtab, u_int32_t temp_id);

/**
 * Create a variable for a memory address that is not from an actual var
 */
symtab_variable_record_t* create_temp_memory_address_variable(generic_type_t* type, variable_symtab_t* variable_symtab, stack_region_t* stack_region, u_int32_t temp_id);

/**
 * Add a parameter to a function and perform all internal bookkeeping needed
 */
u_int8_t add_function_parameter(symtab_function_record_t* function_record, symtab_variable_record_t* variable_record);

/**
 * Make a function record
 */
symtab_function_record_t* create_function_record(dynamic_string_t name, u_int8_t is_public, u_int8_t is_inlined, u_int32_t line_number);

/**
 * Create a type record for the symbol table
 */
symtab_type_record_t* create_type_record(generic_type_t* type);

/**
 * Create a macro record for the macro table
 */
symtab_macro_record_t* create_macro_record(dynamic_string_t name, u_int32_t line_number);

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
 * Insert a macro into the symtab
 */
u_int8_t insert_macro(macro_symtab_t* symtab, symtab_macro_record_t* record);

/**
 * Determine whether or not a function is directly recursive using the function
 * symtab's adjacency matrix
 */
u_int8_t is_function_directly_recursive(function_symtab_t* symtab, symtab_function_record_t* record);

/**
 * Determine whether or not a function is recursive(direct or indirect) using the function
 * symtab's transitive closure 
 */
u_int8_t is_function_recursive(function_symtab_t* symtab, symtab_function_record_t* record);

/**
 * A helper function that adds all basic types to the type symtab
 */
u_int16_t add_all_basic_types(type_symtab_t* symtab);

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
 * Lookup a macro in the symtab
 */
symtab_macro_record_t* lookup_macro(macro_symtab_t* symtab, char* name);

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
 * Specifically look for a pointer type to the given type in the symtab
 */
symtab_type_record_t* lookup_pointer_type(type_symtab_t* symtab, generic_type_t* points_to, mutability_type_t mutability);

/**
 * Specifically look for a reference type to the given type in the symtab
 */
symtab_type_record_t* lookup_reference_type(type_symtab_t* symtab, generic_type_t* references, mutability_type_t mutability);

/**
 * Specifically look for an array type with the given type as a member in the symtab
 */
symtab_type_record_t* lookup_array_type(type_symtab_t* symtab, generic_type_t* member_type, u_int32_t num_members, mutability_type_t mutability);

/**
 * Lookup a type name in the symtab by the name only. This does not
 * do the array bound comparison that we need for strict equality
 *
 * Looking up a type by name only also requires that we know the mutability that we desire
 */
symtab_type_record_t* lookup_type_name_only(type_symtab_t* symtab, char* name, mutability_type_t mutability);

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
 * Record that a given source function calls the target
 *
 * This always goes as: source calls target
 */
void add_function_call(symtab_function_record_t* source, symtab_function_record_t* target);

/**
 * A helper method for variable name printing
 */
void print_variable_name(symtab_variable_record_t* record);

/**
 * A helper method for type name printing
 */
void print_type_name(symtab_type_record_t* record);

/**
 * Print the call graph's adjacency matrix out for debugging
 */
void print_call_graph_adjacency_matrix(FILE* fl, function_symtab_t* function_symtab);

/**
 * This function is intended to be called after parsing is complete.
 * Within it, we will finalize the function symtab including constructing
 * the adjacency matrix for the call graph
 */
void finalize_function_symtab(function_symtab_t* symtab);

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
 * Destroy a macro symtab
 */
void macro_symtab_dealloc(macro_symtab_t* symtab);

#endif /* SYMTAB_H */
