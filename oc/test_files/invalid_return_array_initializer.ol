/**
 * Author: Jack Robbins
 * Test an invalid case where we attempt to return an array initializer. This will never work
 * because arrays are return by reference, not by copy, so there is nothing to initialize into
 * and return
 */

pub fn ret_array() -> i32[5] {
	ret [1, 2, 3, 4, 5];
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret 0;
}
