/**
* Author: Jack Robbins
* Test the case where we are storing a byte into a floating point(float/double) memory
* region
*/

//Should trigger conversion for the store
pub fn byte_to_float(x:u8, arr:mut f32*) -> void {
	arr[33] = x;
}

//Should trigger conversion for the store
pub fn byte_to_double(x:u8, arr:mut f64*) -> void {
	arr[33] = x;
}

//Dummy
pub fn main() -> i32 {
	ret 0;
}
