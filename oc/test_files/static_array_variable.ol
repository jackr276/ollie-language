/**
* Author: Jack Robbins
* Test the case where we have a static array variable. This kind of variable
* skips the usual stack allocations and instead acts like a global array 
* variable
*/


pub fn static_array(input:i32, i:i32) -> i32 {
	declare static x:mut i32[10];

	x[i] = input;

	ret x[i + 1];
}


pub fn main() -> i32 {
	ret @static_array(3, 1);
}
