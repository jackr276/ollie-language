/**
 * The symbol table that is build by the compiler. This is implemented as a hash table
*/


#ifndef SYMTAB_H
#define SYMTAB_H

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//We define that each lexical scope can have 5000 symbols at most
//Chosen because it's a prime not too close to a power of 2
#define KEYSPACE 4999 

typedef struct symtab_t symtab_t;
typedef struct symtab_record_t symtab_record_t;

struct symtab_record_t{
	//The name that we are storing. This is used to derive the hash
	char* name;
	//The hash that we have
	u_int16_t hash;
	//The lexical level of this record
	u_int16_t lexical_level;
	//Will be used later, the offset for the address in the data area
	u_int64_t offset;
	//In case of collisions, we can chain these records
	symtab_record_t* next;

};


/**
 * This struct represents a specific lexical level of a symtab
 */
struct symtab_t{
	//Link to the next level
	symtab_t* next_level;
	//Link to the prior level
	symtab_t* previous_level;
	//How many records(names) we can have
	symtab_record_t* records[KEYSPACE];
	//The level of this particular symtab
	u_int8_t lexical_level;
};


/**
 * Initialize the overall global symtab
 */
symtab_t* initialize_global_symtab();

/**
 * Initialize the symbol table
 */
symtab_t* initialize_scope(symtab_t* symtab);


/**
 * Create a record for the symbol table
 */
symtab_record_t* create_record(char* name, u_int16_t lexical_level, u_int64_t offset);


/**
 * Finalize the scope and go back a level
 */
symtab_t* finalize_scope(symtab_t* symtab);


/**
 * Insert a name into the symbol table
 */
u_int8_t insert(symtab_t* symtab, symtab_record_t* record);


/**
 * Lookup a name in the symtab
 */
symtab_record_t* lookup(symtab_t* symtab, char* name);


/**
 * A printing function for development purposes
 */
void print_record(symtab_record_t* record);

/**
 * Deinitialize the symbol table
 */
void destroy_symtab(symtab_t* symtab);


#endif /* SYMTAB_H */
