/**
 * Author: Jack Robbins
 * 
 * This header file contains the data structure definitions and API for the lexitem hashmap
*/

//Include guards
#ifndef LEXITEM_HASHMAP_T
#define LEXITEM_HASHMAP_T

#include <sys/types.h>
#include "../dynamic_string/dynamic_string.h"
#include "../token.h"

//Predefine our type
typedef struct lexitem_hashmap_t lexitem_hashmap_t;
typedef struct lexitem_hashmap_pair_t lexitem_hashmap_pair_t;

//A pair simply contains a string and a token
struct lexitem_hashmap_pair_t{
	dynamic_string_t lexeme;
	Token t;
};


//And we define it here
struct lexitem_hashmap_t {
	//The internal hashmap itself
	lexitem_hashmap_pair_t* internal_array;
	//The internal size
	u_int32_t size;
};

/**
 * Create a lexitem hashmap
 */
lexitem_hashmap_t* lexitem_hashmap_alloc(u_int32_t size);

/**
 * Fully populate the lexitem hashmap
 */


#endif /* LEXITEM_HASHMAP_T */
