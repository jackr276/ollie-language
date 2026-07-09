/**
 * Author: Jack Robbins
 * This file is meant to stress test the dynamic array implementation. This will
 * be run as a CI/CD job upon each push
*/

//Link to dynamic array
#include "../utils/dynamic_array/dynamic_array.h"
#include "../utils/dynamic_integer_array/dynamic_integer_array.h"
#include "../utils/constants.h"
#include <assert.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>


/**
 * Run through the standard test of addition and deletion for a pointer(regular/generic)
 * dynamic array type
 */
void test_dynamic_array(){
	//Allocate the array
	dynamic_array_t array = dynamic_array_alloc();

	//We should ensure that this is empty
	assert(dynamic_array_is_empty(&array) == TRUE);

	//Fill it up with some junk - just a bunch of int pointers
	for(int32_t i = 0; i < 30000; i++){
		//Allocate it
		int32_t* int_ptr = calloc(1, sizeof(int32_t));

		//Just use the value of the int
		*int_ptr = i;

		//Add it into the array
		dynamic_array_add(&array, int_ptr);
	}
	
	//Iterate over the entire thing
	for(int32_t i = 0; i < 30000; i++){
		//Grab it out here -- remember, a dynamic array returns a void*
 		int32_t grabbed = *(int32_t*)(dynamic_array_get_at(&array, i));

		assert(grabbed == i);
	}

	//Delete at the very end
	int* deleted = (int*)(dynamic_array_delete_at(&array, 29999));

	assert(*deleted == 29999);

	//This should not be empty
	assert(dynamic_array_is_empty(&array) == FALSE);

	//And free it
	free(deleted);

	//Now we'll do the exact same thing but with a deletion
	for(int32_t i = 0; i < 29999; i++){
		//Grab it out here -- remember, a dynamic array returns a void*
 		int32_t* grabbed = (int32_t*)(dynamic_array_delete_at(&array, 0));

		assert(*grabbed == i);

		//While we're here, we may as well free this one
		free(grabbed);
	}

	//This should be empty
	assert(dynamic_array_is_empty(&array) == TRUE);
	
	//Destroy it
	dynamic_array_dealloc(&array);

	printf("\n================= TESTING SETTING =================\n");

	//Allocate this one with an initial size
	array = dynamic_array_alloc_initial_size(35);

	for(int32_t i = 34; i >= 0; i--){
		int32_t* c = malloc(1 * sizeof(int32_t));

		*c = i % 2;

		dynamic_array_set_at(&array, c, i);
	}

	printf("[");

	for(int32_t i = 0; i < 35; i++){
		int* value = dynamic_array_get_at(&array, i);

		if(value == NULL){
			printf("(NULL)");
		} else {
			printf("%d", *value);
		}

		if(i != 34){
			printf(", ");
		}

		//Deallocate this when done
		free(value);
	}

	printf("]\n");

	dynamic_array_dealloc(&array);
}


/**
 * Run through the standard test of addition and deletion for an integer
 * dynamic array type
 */
void test_dynamic_integer_array(){
	//Allocate the array
	dynamic_integer_array_t array = dynamic_integer_array_alloc();

	//We should ensure that this is empty
	assert(dynamic_integer_array_is_empty(&array) == TRUE);

	//Fill this up with 30000 integers
	for(int32_t i = 0; i < 30000; i++){
		//Add it into the array
		dynamic_integer_array_add(&array, i);
	}
	
	//Iterate over the entire thing and see if we can get them out
	for(int32_t i = 0; i < 30000; i++){
		//Grab it out here -- remember, a dynamic array returns a void*
 		int32_t grabbed = dynamic_integer_array_get_at(&array, i);

		assert(grabbed == i);
	}

	//Delete at the very end
	int32_t deleted = dynamic_integer_array_delete_at(&array, 29999);
	assert(deleted == 29999);

	//This should not be empty
	assert(dynamic_integer_array_is_empty(&array) == FALSE);

	//Now we'll do the exact same thing but with a deletion
	for(int32_t i = 0; i < 29999; i++){
		//Grab it out here -- remember, a dynamic array returns a void*
 		int grabbed = dynamic_integer_array_delete_at(&array, 0);
		assert(grabbed == i);
	}

	//This should be empty
	assert(dynamic_integer_array_is_empty(&array) == TRUE);
	
	//Destroy it
	dynamic_integer_array_dealloc(&array);

	printf("\n================= TESTING SETTING =================\n");

	//Allocate this one with an initial size
	array = dynamic_integer_array_alloc_initial_size(35);

	//Set them with even/odd values
	for(int32_t i = 34; i >= 0; i--){
		dynamic_integer_array_set_at(&array, i % 2, i);
	}

	printf("[");

	for(int32_t i = 0; i < 35; i++){
		int32_t value = dynamic_integer_array_get_at(&array, i);

		printf("%d", value);

		if(i != 34){
			printf(", ");
		}
	}

	printf("]\n");

	dynamic_integer_array_dealloc(&array);
}


/**
 * The array test goes through and tests all dynamic array types
 */
int main(){
	//Test the dynamic array first
	test_dynamic_array();

	//Now the integer version
	test_dynamic_integer_array();
}
