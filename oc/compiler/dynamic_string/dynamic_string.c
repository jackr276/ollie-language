/**
 * Author: Jack Robbins
 * This file contains the implementations for the APIs defined in dynamic_string.h
*/

#include "dynamic_string.h"
#include <stdlib.h>

/**
 * Allocate a dynamic string on the heap
 */
void dynamic_string_alloc(dynamic_string_t* dynamic_string){
	//Set the length to be the default length
	dynamic_string->length = DEFAULT_STRING_LENGTH;

	//Now we'll allocate this using the default strategy
	dynamic_string->string = calloc(1, sizeof(dynamic_string_t));
}


/**
 * Set the value of a dynamic string. The function
 * will dynamically resize said string if what is passed
 * through is too big
 */
dynamic_string_t* dynamic_string_set(dynamic_string_t* dynamic_strig, char* string){

}


/**
 * Add a char to a dynamic string - this is really targeted at
 * how our lexer works
 */
dynamic_string_t* dynamic_string_add_char_to_back(dynamic_string_t* dynamic_string, char ch){

}


/**
 * Deallocate a dynamic string from the heap
 */
void dynamic_string_dealloc(dynamic_string_t* dynamic_string){
	//All we'll do here is free the string area
	if(dynamic_string->string != NULL){
		free(dynamic_string->string);
	}
}

