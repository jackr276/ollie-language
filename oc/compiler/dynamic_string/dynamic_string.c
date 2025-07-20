/**
 * Author: Jack Robbins
 * This file contains the implementations for the APIs defined in dynamic_string.h
*/

#include "dynamic_string.h"
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

/**
 * Allocate a dynamic string on the heap
 */
void dynamic_string_alloc(dynamic_string_t* dynamic_string){
	//Set the length to be the default length
	dynamic_string->length = DEFAULT_STRING_LENGTH;

	//Now we'll allocate this using the default strategy
	dynamic_string->string = calloc(dynamic_string->length, sizeof(char));
}


/**
 * Set the value of a dynamic string. The function
 * will dynamically resize said string if what is passed
 * through is too big
 */
dynamic_string_t* dynamic_string_set(dynamic_string_t* dynamic_string, char* string){
	//Measure the length of this string *with* the null character included
	u_int16_t paramter_length = strlen(string) + 1;

	//This represents the new length of the string
	u_int16_t new_length = dynamic_string->current_length + paramter_length;

	//If the current length of the string, plus the length of the new string, plus
	//1 for the null character exceeds our current length, we need to resize
	if(new_length >= dynamic_string->length){
		//Is this string's new length less than double the old length? This
		//will trigger our default behavior of doubling it
		if(new_length < dynamic_string->length * 2){
			//Double the length
			dynamic_string->length = dynamic_string->length * 2;

		//Otherwise, we need to go more than double. This is a rare case, but it can happen. If this does happen,
		//we'll set the new length to be double the current length
		} else {
			dynamic_string->length = new_length * 2;
		}

		//Realloc the string with this length 
		dynamic_string->string = realloc(dynamic_string->string, dynamic_string->length * sizeof(char));
	}

	//Set the current length to be this new length here
	dynamic_string->current_length = new_length;

	//Copy the string over
	strncpy(dynamic_string->string, string, paramter_length);

	//For convenience we return the string pointer, but the user need not use it
	return dynamic_string;
}


/**
 * Add a char to a dynamic string - this is really targeted at
 * how our lexer works
 */
dynamic_string_t* dynamic_string_add_char_to_back(dynamic_string_t* dynamic_string, char ch){
	//Dynamic resize if needed
	if(dynamic_string->current_length + 1 >= dynamic_string->length){
		//Double the length
		dynamic_string->length *= 2;

		//Realloc with the new length
		dynamic_string->string = realloc(dynamic_string->string, dynamic_string->length * sizeof(char));
	}

	//Set the char to be at the end
	dynamic_string->string[dynamic_string->current_length] = ch;

	//Increment this
	dynamic_string->current_length += 1;

	//Null terminate the string
	dynamic_string->string[dynamic_string->current_length] = '\0';

	//We return this for the user's convenience, but they don't need to use it
	return dynamic_string;
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

