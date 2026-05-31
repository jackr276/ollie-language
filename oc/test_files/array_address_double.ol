/**
* Author: Jack Robbins
* Test what happens if we take the address of an array twice
*/

pub fn access_array(a:mut i32[10]*) -> i32{
	(*a)[1] = 4;
	(*a)[2] = 5;

	ret (*a)[2];
}


pub fn main() -> i32 {
	//The compiler should detect and count the number
	//in the array initializer list.
	let arr:mut i32[] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

	//Grab this by just taking the array's address
	@access_array(&arr);

	//Now let's grab it again - we already have a pointer to
	//the address in the stack, so we shouldn't need to add it
	//back in
	@access_array(&arr);

	//Should return 5 post modifications
	OUNIT:[console = 5]
	ret arr[2];
}
