/**
* Author: Jack Robbins
* Test the case where we are loading a byte into a floating point(float/double) variable
*/

//Should trigger conversion for the load 
pub fn byte_to_float(arr:u8*) -> f32{
	ret arr[2];
}

//Should trigger conversion for the load
pub fn byte_to_double(arr:u8*) -> f64 {
	ret arr[2];
}

//Dummy
pub fn main() -> i32 {
	ret 0;
}
