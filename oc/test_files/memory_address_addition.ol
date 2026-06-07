/**
* Author: Jack Robbins
* This test will go over the case where we are adding/subtracting to a memory address for the purposes
* of pointer arithmetic
*/


pub fn add_to_address(x:mut i32*) -> i32 {
	x += 2;

	ret *x;
}


pub fn subtract_from_address(x:mut i32*) -> i32 {
	x -= 3;

	ret *x;
}


pub fn main() -> i32 {
	let x_arr:mut i32[] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

	//Should return 4 + 5 = 9
	OUNIT: [console = 9]
	ret @add_to_address(&x_arr[1]) + @subtract_from_address(&x_arr[7]);
}
