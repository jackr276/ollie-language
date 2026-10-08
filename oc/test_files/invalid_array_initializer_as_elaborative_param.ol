/**
 * Author: Jack Robbins
 * Test the invalid use of an array initializer as a function parameter
 *
 * This is *NOT* allowed because array types are passed by reference, not by
 * copy, so really there's no parameter region to initialize into for this
 */


pub fn use_array(y:i32, x:params i32[5]) -> i32 {
	ret x[0][1] + x[0][2];
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret @use_array(5, [1, 2, 3, 4, 5]);
}
