/**
 * Author: Jack Robbins
 * This file is meant to stress test the dynamic set implementation. This will
 * be run as a CI/CD job upon each push
*/

#include "../utils/dynamic_set/dynamic_set.h"
#include "../utils/constants.h"
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>

/**
 * Run the test for the entire dynamic array
*/
int main(){
	//Allocate the set 
	dynamic_set_t set = dynamic_set_alloc();

	//We should ensure that this is empty
	if(dynamic_set_is_empty(&set) == FALSE){
		fprintf(stderr, "Is empty check fails\n");
		exit(1);
	}
	
	//Fill it up with some junk - just a bunch of int pointers
	for(int i = 0; i < 30000; i++){
		//Allocate it
		int* int_ptr = calloc(1, sizeof(int));

		//Just use the value of the int
		*int_ptr = i;

		//Add it into the array
		dynamic_set_add(&set, int_ptr);
	}
	
	//Iterate over the entire thing
	for(u_int16_t i = 0; i < 30000; i++){
		//Grab it out here -- remember, a dynamic array returns a void*
 		int grabbed = *(int*)(dynamic_set_get_at(&set, i));

		//If this is all correct here, it should match
		if(grabbed != i){
			fprintf(stderr, "Expected %d at index %d but got: %d\n", i, i, grabbed);
		}
	}

	//Now, let's test the dynamic set capabilities. If we try to readd the same pointers
	//over and over again, we should never do so
	for(int i = 0; i < 30000; i++){
		//Get the int pointer out
		int* int_ptr = dynamic_set_get_at(&set, i);

		//Try to add it back in. Note that this should not result in any
		//real additions
		dynamic_set_add(&set, int_ptr);
	}

	//If for some reason the size is more than 29999, this is a failure because we violated the uniqueness
	//constraint
	if(set.current_index != 29999){
		fprintf(stderr, "Exepcted a maximum size of 29999 but got %d\n", set.current_max_size);
		exit(1);
	}

	//Delete at the very end
	int* deleted = (int*)(dynamic_set_delete_at(&set, 29999));

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
 		int* grabbed = (int*)(dynamic_set_delete_at(&set, 0));

		//If this is all correct here, it should match
		if(*grabbed != i){
			fprintf(stderr, "Expected %d at index %d but got: %d\n", i, i, *grabbed);
		}

		//While we're here, we may as well free this one
		free(grabbed);
	}

	//So we now know that we can add
	//We should ensure that this is not empty
	if(dynamic_set_is_empty(&set) == FALSE){
		fprintf(stderr, "Is empty check fails\n");
		exit(-1);
	}
	
	//Destroy it
	dynamic_set_dealloc(&set);

	//All worked here
	return 0;
}
