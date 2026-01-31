/**
* Author: Jack Robbins
* Test an invalid attempt to forward declare a function as inline and then omit
* inline from the formal declaration
*/

declare inline fn example(i32, i32) -> i32;


pub fn main() -> i32 {
	ret @example(3, 3);
}


fn example(x:i32, y:i32) -> i32 {
	ret x * y;
}
