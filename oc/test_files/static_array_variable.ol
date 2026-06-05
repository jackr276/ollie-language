/**
* Author: Jack Robbins
* Test the case where we have a static array variable. This kind of variable
* skips the usual stack allocations and instead acts like a global array 
* variable
*/


pub fn static_array(input:i32, i:i32) -> i32 {
	let static x:mut i32[10] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

	x[i] = input;

	ret x[i + 1];
}


pub fn main() -> i32 {
	OUNIT: [console = 3]
	ret @static_array(3, 1);
}
