/**
 * Author: Jack Robbins
 * Test an invalid case where we attempt to return a string initializer. This will never work
 * because strings are return by reference, not by copy, so there is nothing to initialize into
 * and return
 */

pub fn ret_string() -> char[5] {
	ret "bonk";
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret 0;
}
