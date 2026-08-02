/**
 * Author: Jack Robbins
 * Test an invalid use of a function pointer that has been called before
 * being initialized
 */


pub fn main() -> i32 {
	let x:i32 = 5;
	let y:i32 = 6;

	declare add:fn(i32, i32) -> i32;

	OUNIT: [fail_to_compile]
	ret @add(x, 5);
}
