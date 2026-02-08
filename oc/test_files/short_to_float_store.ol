/**
* Author: Jack Robbins
* Test the case where we are storing a short into a floating point(float/double) memory
* region
*/

//Should trigger conversion for the store
pub fn short_to_float(x:i16, arr:mut f32*) -> void {
	arr[33] = x;
}

//Should trigger conversion for the store
pub fn short_to_double(x:u16, arr:mut f64*) -> void {
	arr[33] = x;
}

//Dummy
pub fn main() -> i32 {
	ret 0;
}
