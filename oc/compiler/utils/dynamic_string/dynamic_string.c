/**
 * Author: Jack Robbins
 * This file contains the implementations for the APIs defined in dynamic_string.h
*/

#include "dynamic_string.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include "../constants.h"

/**
 * Perform a resize on our dynamic string if we determine that it's needed. We will resize when
 * the proposed index is at or above our current index
 */
static inline void dynamic_resize_if_needed(dynamic_string_t* dynamic_string, u_int32_t proposed_index){
	//Nothing to do here, it's less than the length
	if(proposed_index < dynamic_string->length){
		return;
	}

	//Double the new index for our new length and reallocate
	dynamic_string->length = proposed_index * 2;
	dynamic_string->string = realloc(dynamic_string->string, sizeof(char) * dynamic_string->length);
}


/**
 * Allocate a dynamic string on the heap
 */
dynamic_string_t dynamic_string_alloc(){
	//String stack allocated
	dynamic_string_t string;

	//Set the length to be the default length
	string.length = DEFAULT_DYNAMIC_STRING_LENGTH;

	//Now we'll allocate this using the default strategy
	string.string = calloc(string.length, sizeof(char));

	//Set the current length to be zero
	string.current_length = 0;

	//Give back the stack allocated version
	return string;
}


/**
 * Heap allocate the entire dynamic_string. This includes the control
 * structure and the string itself. We should only be doing this if absolutely
 * necessary
 *
 * This version requires 2 allocations on the heap and is thus more inefficient, so
 * again only use if we have a use case that requires it
 */
dynamic_string_t* dynamic_string_heap_alloc(){
	//Heap allocate
	dynamic_string_t* string = calloc(1, sizeof(dynamic_string_t));

	//Set the length to be the default length
	string->length = DEFAULT_DYNAMIC_STRING_LENGTH;

	//Now we'll allocate this using the default strategy
	string->string = calloc(string->length, sizeof(char));

	//Set the current length to be zero
	string->current_length = 0;

	//Give back the stack allocated version
	return string;
}


/**
 * Clone a dynamic string into a new one
 */
dynamic_string_t clone_dynamic_string(dynamic_string_t* dynamic_string){
	dynamic_string_t new = {NULL, 0, 0};

	//Copy these values over
	new.current_length = dynamic_string->current_length;
	new.length = dynamic_string->length;

	//Now we'll allocate and copy the string over
	new.string = calloc(dynamic_string->length, sizeof(char));

	//And we'll copy it over
	strncpy(new.string, dynamic_string->string, dynamic_string->length);

	//And give back the new one
	return new;
}


/**
 * Insert a given string *into* an already allocated dynamic string at a given
 * index. This function will fail if the index given is greater than the current
 * highest index
 *
 * This function also handles any/all reallocation that we need to do
 */
void dynamic_string_insert_string_at_index(dynamic_string_t* dynamic_string, char* insertee, int32_t index){
	//Fail case that we just bail out for
	if(index > (int32_t)(dynamic_string->current_length)){
		fprintf(stderr, "Attempt to insert at index %d in a string that is only length %d\n", index, dynamic_string->current_length);
		exit(1);
	}

	//Grab this one's length
	u_int32_t insertee_length = strlen(insertee);

	/**
	 * Get what our new length would be and allow the dynamic resize rule
	 * to handle it
	 */
	u_int32_t new_length = dynamic_string->current_length + insertee_length + 1;
	dynamic_resize_if_needed(dynamic_string, new_length);

	/**
	 * First step: run through the string backwards and shift everything
	 * over by the "insertee_length" in order to make room. We'll do this
	 * up until we hit the index
	 */
	if(dynamic_string->current_length > 0){
		for(int32_t i = dynamic_string->current_length; i >= index; i--){
			dynamic_string->string[i + insertee_length] = dynamic_string->string[i];
		}
	}

	//Now update the current length 
	dynamic_string->current_length += insertee_length;

	/**
	 * Now our final step is to go through and insert the current string using the space
	 * that we've just made for it
	 */
	for(u_int32_t i = 0; i < insertee_length; i++){
		dynamic_string->string[index + i] = insertee[i];
	}

	//Always set the NULL terminator
	dynamic_string->string[dynamic_string->current_length] = '\0';
}


/**
 * Set the value of a dynamic string. The function
 * will dynamically resize said string if what is passed
 * through is too big
 *
 * SET TO: Hello\0  <-- Current Length = 5
 *
 * Should have in dynamic string: 'H', 'e', 'l', 'l', 'o', '\0'
 * Current length should be 5 in the end
 * Need to resize space for at least 6 characters
 */
void dynamic_string_set(dynamic_string_t* dynamic_string, char* string){
	//Get the length of this string *WITHOUT* the NULL character
	u_int32_t paramter_length = strlen(string);

	//This represents the entire new length of the string(no null character)
	dynamic_string->current_length = paramter_length;

	//Perform a resize if needed(+ 1 to include the null terminator)
	dynamic_resize_if_needed(dynamic_string, dynamic_string->current_length + 1);

	//Copy the string over
	strncpy(dynamic_string->string, string, paramter_length);

	//Be sure to set the NULL terminator at the very end
	dynamic_string->string[dynamic_string->current_length] = '\0';
}


/**
 * Add a char to a dynamic string - this is really targeted at
 * how our lexer works
 */
void dynamic_string_add_char_to_back(dynamic_string_t* dynamic_string, char ch){
	//Perform a dynamic resize if need be
	dynamic_resize_if_needed(dynamic_string, dynamic_string->current_length + 1);

	//Set the char to be at the end
	dynamic_string->string[dynamic_string->current_length] = ch;

	//Increment this
	dynamic_string->current_length += 1;

	//Null terminate the string
	dynamic_string->string[dynamic_string->current_length] = '\0';
}


/**
 * Concatenate a string to the end of our dynamic string
 *
 * dynamic_string: 'H', 'e', 'l', 'l', 'o', '\0'
 * \0 is always at the "current_length", so current_length = 5
 * string to add: 'W', 'o', 'r', 'l', 'd', '\0'
 *
 * Need to have 5 + 5 + 1 = 11 space
 *
 * dynamic_string: 'H', 'e', 'l', 'l', 'o', 'W', 'o', 'r', 'l', 'd'
 * current_length = 10
 * Store '\0' at index 10
 * dynamic_string: 'H', 'e', 'l', 'l', 'o', 'W', 'o', 'r', 'l', 'd', '\0'
 */
void dynamic_string_concatenate(dynamic_string_t* dynamic_string, char* string){
	//How much additional length do we need
	int32_t additional_length = strlen(string);

	/**
	 * The new length is the number of characters in the current
	 * string plus the additional length
	 */
	dynamic_string->current_length = dynamic_string->current_length + additional_length;

	//Resize if needed(+1 for NULL terminator)
	dynamic_resize_if_needed(dynamic_string, dynamic_string->current_length + 1);

	//Concatenate the string here
	strncat(dynamic_string->string, string, additional_length);

	//Add the NULL terminator onto the end
	dynamic_string->string[dynamic_string->current_length] = '\0';
}


/**
 * Deallocate a dynamic string that was heap allocated
 */
void dynamic_string_heap_dealloc(dynamic_string_t* dynamic_string){
	if(dynamic_string->string != NULL){
		free(dynamic_string->string);
	}

	//This entire thing is on the heap so free it
	free(dynamic_string);
}


/**
 * Deallocate a dynamic string from the heap
 */
void dynamic_string_dealloc(dynamic_string_t* dynamic_string){
	//All we'll do here is free the string area
	if(dynamic_string->string != NULL){
		free(dynamic_string->string);
	}

	//Reset all of these parameters
	dynamic_string->string = NULL;
	dynamic_string->current_length = 0;
	dynamic_string->length = 0;
}
