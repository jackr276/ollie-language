/**
 * Author: Jack Robbins
 * This file is meant to stress test the dynamic array implementation. This will
 * be run as a CI/CD job upon each push
*/

#include "dynamic_array.h"
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
		fprintf(stderr, "Is empty check fails");
		exit(-1);
	}


	
	//Fill it up with some junk - just a bunch of int pointers
	for(int i = 0; i < 5000; i++){
		//Allocate it
		int* int_ptr = calloc(1, sizeof(int));

		//Just use the value of the int
		*int_ptr = i;

		//Add it into the array
		dynamic_array_add(array, int_ptr);
	}
	

	/*
	//Iterate over the entire thing
	for(u_int16_t i = 0; i < 50000; i++){
		//Grab it out here -- remember, a dynamic array returns a void*
 		int grabbed = *(int*)(dynamic_array_get_at(array, i));

	}

	//So we now know that we can add
	//We should ensure that this is not empty
	if(dynamic_array_is_empty(array) == TRUE){
		fprintf(stderr, "Is empty check fails");
		exit(-1);
	}
	*/
	
	//Destroy it
	dynamic_array_dealloc(array);
}
