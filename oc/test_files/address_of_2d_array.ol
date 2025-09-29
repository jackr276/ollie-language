/**
* Author: Jack Robbins
* Test addresses of 2d arrays in ollie
*/

/**
* Dummy for testing
*/
pub fn mutate_array(mut x:i32**) -> void {
	x[3][2] = 5;

	ret;
}


/**
* Dummy for testing
*/
pub fn mutate_value(mut x:i32*) -> void {
	*x = 11;
	ret;
}


pub fn main() -> i32 {
	//Validate that we're able to do a 2d array initializer
	let mut arr:i32[][] = [[1,2,3],[3,4,5],[6,7,8]];

	//Get a double pointer address
	let mut arr_ptr:i32** = &arr[2];

	//Use this to mutate it
	@mutate_array(arr_ptr);

	//Get a single pointer address
	let mut item_ptr:i32* = &arr[2][2];

	//Use this to mutate it
	@mutate_value(item_ptr);

	//Just give 0 back
	ret 0;
}
