/**
* Author: Jack Robbins
* Very basic test to test float logical and/or
*/


pub fn float_logical_and_or_expand(x:f32, y:f32, z:f32) -> i32 {
	ret x || y && z;
}


pub fn float_logical_and_or(x:f32, y:f32, z:f32) -> i8 {
	ret x && y || z;
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
