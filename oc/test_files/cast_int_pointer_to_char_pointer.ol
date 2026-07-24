/**
 * Author: Jack Robbins
 * Test our ability to interpret integers are chars using casting
 */


pub fn int_ptr_to_char_ptr(x:i32*) -> char {
	//Cast to a char
	let cast:char* = <char*>x;

	//Should now only grab the first byte
	ret *cast;
}


pub fn main() -> i32 {
	//INT_MAX - all 1's
	let x:i32 = (1 << 31) - 1;

	//Should have all 1's in the end
	OUNIT: [exit_status = 255]
	ret @int_ptr_to_char_ptr(&x);
}
