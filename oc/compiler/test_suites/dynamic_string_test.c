/**
 * Author: Jack Robbins
 * Test suite for dynamic string submodule
*/

//Link to module being tested
#include "../dynamic_string/dynamic_string.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

int main(){
	//Allocate the dynamic string
	dynamic_string_t string = {NULL, 0, 0};

	//Allocate the string
	dynamic_string_alloc(&string);

	//First set
	dynamic_string_set(&string, "I am a simple string");

	//Print it out
	printf("%s\n", string.string);

	//Now add less
	dynamic_string_set(&string, "I have less");

	//Print it out
	printf("%s\n", string.string);

	//Force a resize
	dynamic_string_set(&string, "The quick brown fox jumped over the lazy dog. This string is longer than the defaulted length.");

	//Print it out
	printf("%s\n", string.string);

	//Now add less
	dynamic_string_set(&string, "I have less");

	//Print it out
	printf("%s\n", string.string);

	//Deallocate it at the very end
	dynamic_string_dealloc(&string);

	printf("============ Testing char addition ================\n");

	//Recreate this string
	dynamic_string_alloc(&string);

	//We will add this char by char
	char addition_string[] = "This is a very long string that will test char addition inside of the dynamic string submodule for ollie language.";

	//Grab the length like a user would
	u_int16_t length = strlen(addition_string);

	//Add char by char
	for(u_int16_t i = 0; i < length; i++){
		dynamic_string_add_char_to_back(&string, addition_string[i]);
		
		printf("%s\n", string.string);
	}

	//Deallocate it at the very end
	dynamic_string_dealloc(&string);
}
