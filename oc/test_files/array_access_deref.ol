/**
* Author: Jack Robbins
* Testing array initializers in ollie
*/

pub fn access_array(a:mut i32[10]*) -> i32{
	(*a)[1] = 6;
	(*a)[2] = 7;

	ret (*a)[2];
}


pub fn access_pointer(a:mut i32**) -> i32{
	//Test our tracking abilities
	let x:mut i32* = *a;

	x[1] = 3;
	x[2] = 3;

	ret 0;
}


pub fn main() -> i32 {
	//The compiler should detect and count the number
	//in the array initializer list.
	let arr:mut i32[] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

	//Grab this by just taking the array's address
	@access_array(&arr);

	//Should return 7 post modification
	OUNIT: [exit_status = 7]
	ret arr[2];
}
