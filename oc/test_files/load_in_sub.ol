/**
* Author: Jack Robbins
* Test a very basic case where we have a load combined with a sub instruction
*/


pub fn load_in_sub(x:i32*, y:i32*) -> i32 {
	ret *x - *y;
}


pub fn main() -> i32 {
	let x:i32 = 7;
	let y:i32 = 5;

	OUNIT: [exit_status = 2]
	ret @load_in_sub(&x, &y);
}
