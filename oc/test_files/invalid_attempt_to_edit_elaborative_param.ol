/**
* Author: Jack Robbins
* Part of our requirement is that elaborative params are always immutable, even if the underlying values
* or types are mutable. This test will demonstrate that we do not allow modification of these types
*/

pub fn elaborative_param(x:i32, y:params mut i32) -> i32 {
	//INVALID - cannot modify this
	y[0] = 1;
	y[1] = 2;
	
	ret x + y[0];
}

//Dummy
pub fn main() -> i32 {
	ret 0;
}
