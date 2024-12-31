/**
 * The symbol table that is build by the compiler. This is implemented as a hash table
*/


#ifndef SYMTAB_H
#define SYMTAB_H

#include <sys/types.h>

//We define that each lexical scope can have 1000 symbols at most
#define KEYSPACE 1000 

typedef struct symtab_t symtab_t;
typedef struct symtab_record_t symtab_record_t;

struct symtab_record_t{
	//The name that we are storing. This is used to derive the hash
	char* name;
	//The hash that we have
	u_int16_t hash;
	//In case of collisions, we can chain these records
	symtab_record_t* next;

};


struct symtab_t{
	//How many records(names) we can have
	symtab_record_t records[KEYSPACE];
};

/**
 * Initialize the symbol table
 */
symtab_t* initialize_symtab();



/**
 * Deinitialize the symbol table
 */
void destroy_symtab(symtab_t* symtab);


#endif /* SYMTAB_H */
