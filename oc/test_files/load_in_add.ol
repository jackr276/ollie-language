/**
* Author: Jack Robbins
* Test a very basic case where we have a load combined with an add instruction
*/


pub fn load_in_add(x:i32*, y:i32*) -> i32 {
	ret *x + *y;
}


pub fn main() -> i32 {
	let x:i32 = 5;
	let y:i32 = 7;

	OUNIT: [console = 12]
	ret @load_in_add(&x, &y);
}
