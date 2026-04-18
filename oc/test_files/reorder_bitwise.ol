/**
* Author: Jack Robbins
* Test the parser's ability to automatically reorder bitwise instructions
*/

pub fn reorder_xor(x:i32) -> i32 {
	ret 4 ^ x;
}


pub fn reorder_or(x:i32) -> i32 {
	ret 4 | x;
}


pub fn reorder_and(x:i32) -> i32 {
	ret 4 & x;
}


pub fn main() -> i32 {
	ret 0;
}
