/**
 * Author: Jack Robbins
 * This file contains the implementations for the APIs defined in dynamic_string.h
*/

#include "dynamic_string.h"
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include "../constants.h"

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
 * Set the value of a dynamic string. The function
 * will dynamically resize said string if what is passed
 * through is too big
 */
void dynamic_string_set(dynamic_string_t* dynamic_string, char* string){
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
	dynamic_string->current_length = new_length - 1;

	//Copy the string over
	strncpy(dynamic_string->string, string, paramter_length);
}


/**
 * Add a char to a dynamic string - this is really targeted at
 * how our lexer works
 */
void dynamic_string_add_char_to_back(dynamic_string_t* dynamic_string, char ch){
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
}


/**
 * Concatenate a string to the end of our dynamic string
 */
void dynamic_string_concatenate(dynamic_string_t* dynamic_string, char* string){
	//Grab the string length here
	u_int16_t additional_length = strlen(string) + 1;

	//Now found the overall new length
	u_int16_t new_length = dynamic_string->current_length + additional_length;

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

	//Concatenate the string here
	strncat(dynamic_string->string, string, additional_length);

	//Store the new length
	dynamic_string->current_length = new_length - 1;
}


/**
 * Are two dynamic strings identical?
 */
u_int8_t dynamic_strings_equal(dynamic_string_t* a, dynamic_string_t* b){
	//Mismatched lengths mean that they can't be equal
	if(a->current_length != b->current_length){
		return FALSE;
	}

	//Now we do a string compare. TRUE if they're equal, false if not
	if(strncmp(a->string, b->string, a->current_length) == 0){
		return TRUE;
	} else {
		return FALSE;
	}
}


/**
 * Completely wipe a dynamic string. This allows us to use the same memory that
 * we've allocated once over and over again. This is particularly useful in the lexer
 */
void clear_dynamic_string(dynamic_string_t* dynamic_string){
	//Wipe the entire memory region out
	memset(dynamic_string->string, 0, dynamic_string->length * sizeof(char));

	//And the current length is now just 0
	dynamic_string->current_length = 0;
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
