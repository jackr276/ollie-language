/**
* Author: Jack Robbins
* Test the ability to generate a pointer to a public function
*/


pub fn adder(x:i32, y:i32) -> i32 {
	ret x + y;
}


pub fn main() -> i32 {
	define fn(i32, i32) -> i32 as example_func;

	let x:example_func = adder;

	ret @x(3, 5);
}
