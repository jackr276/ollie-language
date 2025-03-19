/**
 * Author: Jack Robbins
 * This file is meant to stress test the dynamic array implementation. This will
 * be run as a CI/CD job upon each push
*/

#include "dynamic_array.h"


/**
 * Run the test for the entire dynamic array
*/
int main(){
	//Allocate the array
	dynamic_array_t* array = dynamic_array_alloc();

	//Fill it up with some junk - just a bunch of int pointers

	//Destroy it
	dynamic_array_dealloc(array);
}
