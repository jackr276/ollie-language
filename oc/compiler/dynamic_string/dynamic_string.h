/**
 * Author: Jack Robbins
 * This file defines the APIs available for Ollie's custom string
 * data structure
*/

//Include guards
#ifndef DYNAMIC_STRING_H
#define DYNAMIC_STRING_H

#include <sys/types.h>

//Default length of the string is 60 characters
#define DEFAULT_STRING_LENGTH 60

typedef struct dynamic_string_t dynamic_string_t;

/**
 * A dynamic string itself contains the true length of the string(with \0 included)
 * and the pointer itself
 */
struct dynamic_string_t {
	//The string itself
	char* string;
	//The current length of the string
	u_int16_t current_length;
	//The length of said string
	u_int16_t length;
};


/**
 * Allocate a dynamic string on the heap. The actual structure itself
 * will be stack allocated
 */
void dynamic_string_alloc(dynamic_string_t* dynamic_string);

/**
 * Set the value of a dynamic string. The function
 * will dynamically resize said string if what is passed
 * through is too big
 */
dynamic_string_t* dynamic_string_set(dynamic_string_t* dynamic_string, char* string);

/**
 * Add a char to a dynamic string - this is really targeted at
 * how our lexer works
 */
dynamic_string_t* dynamic_string_add_char_to_back(dynamic_string_t* dynamic_string, char ch);

/**
 * Deallocate a dynamic string from the heap
 */
void dynamic_string_dealloc(dynamic_string_t* dynamic_string);

#endif /* DYNAMIC_STRING_H */
