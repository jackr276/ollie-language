/**
 * Author: Jack Robbins
 * Test suite for dynamic string submodule
*/

//Link to module being tested
#include "../dynamic_string/dynamic_string.h"
#include <stdio.h>

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
}
