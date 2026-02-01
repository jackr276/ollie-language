/**
* Author: Jack Robbins
* Test the use of the function type without any define/as keyword
* added into it
*/


pub fn adder(x:i32, y:i32) -> i32 {
	ret x + y;
}


pub fn main() -> i32 {
	let func:fn(i32, i32) -> i32 = adder;

	ret *func(3, 4);
}
