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


typedef enum SYMTAB_RECORD_TYPE{
	FUNCTION,
	VARIABLE
} SYMTAB_RECORD_TYPE;


/**
 * A struct that represents a symtab record
 */
struct symtab_function_record_t{
	//The name that we are storing. This is used to derive the hash
	char* func_name;
	//The hash that we have
	u_int16_t hash;
	//The lexical level of this record
	int16_t lexical_level;
	//The line number
	u_int16_t line_number;
	//Will be used later, the offset for the address in the data area
	u_int64_t offset;
	//Parameter-list of some kind here TODO

	//In case of collisions, we can chain these records
	symtab_function_record_t* next;
};

/**
 * This struct represents a specific lexical level of a symtab
 */
struct symtab_variable_record_t{
	//Variable name
	char* var_name;
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
	//What type is it?
	char* type; //Char * for now TODO
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
void* create_variable_record(char* name, u_int16_t lexical_level, u_int64_t offset);

/**
 * Make a function record
 */
void* create_function_record(char* name, u_int16_t lexical_level, u_int64_t offset);


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
void print_record(void* record);

/**
 * Deinitialize the symbol table
 */
void destroy_symtab(symtab_t*);


#endif /* SYMTAB_H */
