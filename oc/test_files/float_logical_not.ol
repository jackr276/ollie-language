/**
* Author: Jack Robbins
* Test handling float logical notting
*/

pub fn float_logical_not(x:f32) -> i32 {
	ret !x;
}


pub fn main() -> i32 {
	ret @float_logical_not(3.33);
}
