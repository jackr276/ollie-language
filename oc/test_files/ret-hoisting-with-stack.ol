/**
* Author: Jack Robbins
* Test the case where we have ret hoisting that will have a stack deallocation statement
* with it
*/


pub fn ret_hoisting(x:i32) -> i32 {
	let ret_val:mut i32 = 0;
	let array:i32[] = [1, 2, 3, 4, 5];
	
	switch(x){
		case 1 -> {
			ret_val = array[1];
		}

		case 2 -> {
			ret_val = array[2];
		}

		case 3 -> {
			ret_val = array[4];
		}

		default -> {
			ret_val = array[4] + array[3];
		}
	}

	//What should happen - there will be a stack deallocation statement
	//here, and we should be able to hoist the whole thing up into all of
	//the child statements
	ret ret_val;
}
