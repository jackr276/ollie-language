/**
* Author: Jack Robbins
* Test a converting move basic case where we have a load combined with an add instruction
*/


pub fn load_in_add(x:i32*, y:i8*) -> i32 {
	ret *x + *y;
}


pub fn main() -> i32 {
	let x:i32 = 5;
	let y:i8 = 7;

	OUNIT: [exit_status = 12]
	ret @load_in_add(&x, &y);
}
