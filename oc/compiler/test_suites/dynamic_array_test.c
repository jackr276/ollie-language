/**
 * Author: Jack Robbins
 * This file is meant to stress test the dynamic array implementation. This will
 * be run as a CI/CD job upon each push
*/

//Link to dynamic array
#include "../utils/dynamic_array/dynamic_array.h"
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>

#define TRUE 1
#define FALSE 0

/**
 * Run the test for the entire dynamic array
*/
int main(){
	//Allocate the array
	dynamic_array_t* array = dynamic_array_alloc();

	//We should ensure that this is empty
	if(dynamic_array_is_empty(array) == FALSE){
		fprintf(stderr, "Is empty check fails\n");
		exit(-1);
	}
	
	//Fill it up with some junk - just a bunch of int pointers
	for(int i = 0; i < 30000; i++){
		//Allocate it
		int* int_ptr = calloc(1, sizeof(int));

		//Just use the value of the int
		*int_ptr = i;

		//Add it into the array
		dynamic_array_add(array, int_ptr);
	}
	
	//Iterate over the entire thing
	for(u_int16_t i = 0; i < 30000; i++){
		//Grab it out here -- remember, a dynamic array returns a void*
 		int grabbed = *(int*)(dynamic_array_get_at(array, i));

		//If this is all correct here, it should match
		if(grabbed != i){
			fprintf(stderr, "Expected %d at index %d but got: %d\n", i, i, grabbed);
		}
	}

	//Delete at the very end
	int* deleted = (int*)(dynamic_array_delete_at(array, 29999));

	//Make sure it matches
	//If this is all correct here, it should match
	if(*deleted != 29999){
		fprintf(stderr, "Expected %d at index %d but got: %d\n", 29999, 29999, *deleted);
	}

	//And free it
	free(deleted);

	//Now we'll do the exact same thing but with a deletion
	for(u_int16_t i = 0; i < 29999; i++){
		//Grab it out here -- remember, a dynamic array returns a void*
 		int* grabbed = (int*)(dynamic_array_delete_at(array, 0));

		//If this is all correct here, it should match
		if(*grabbed != i){
			fprintf(stderr, "Expected %d at index %d but got: %d\n", i, i, *grabbed);
		}

		//While we're here, we may as well free this one
		free(grabbed);
	}

	//So we now know that we can add
	//We should ensure that this is not empty
	if(dynamic_array_is_empty(array) == FALSE){
		fprintf(stderr, "Is empty check fails\n");
		exit(-1);
	}
	
	//Destroy it
	dynamic_array_dealloc(array);

	printf("\n================= TESTING SETTING =================\n");

	//Allocate this one with an initial size
	array = dynamic_array_alloc_initial_size(35);

	for(int16_t i = 34; i >= 0; i--){
		int* c = malloc(1 * sizeof(int));

		if(i % 2 == 1){
			*c = 1;
		} else {
			*c = 0;
		}

		dynamic_array_set_at(array, c, i);
	}

	printf("[");

	for(u_int16_t i = 0; i < 35; i++){
		int* value = dynamic_array_get_at(array, i);

		if(value == NULL){
			printf("(NULL)");
		} else {
			printf("%d", *value);
		}

		if(i != 34){
			printf(", ");
		}
	}

	printf("]\n");

	dynamic_array_dealloc(array);
}
