/**
* Author: Jack Robbins
* Test the case where we are storing a loading into a floating point(float/double) memory
* region
*/

//Should trigger conversion for the load 
pub fn short_to_float(arr:i16*) -> f32 {
	ret arr[3];
}

//Should trigger conversion for the load
pub fn short_to_double(arr:u16*) -> f64 {
	ret arr[3];
}

//Dummy
pub fn main() -> i32 {
	ret 0;
}
