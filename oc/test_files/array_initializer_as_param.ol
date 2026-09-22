/**
 * Author: Jack Robbins
 * Test the use of an array initializer as a function parameter
 */


pub fn use_array(x:i32[5]) -> i32 {
	ret x[1] + x[2];
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 5]
	ret @use_array([1, 2, 3, 4, 5]);
}
