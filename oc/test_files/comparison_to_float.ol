/**
* Author: Jack Robbins
* Test the case where we have a comparison going to a float
*/


pub fn comparison_to_float_int(x:i32, y:i32) -> f32 {
	ret x > y;
}


pub fn comparison_to_float(f:i32, y:i32) -> f32 {
	ret f < y;
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
