/**
* Author: Jack Robbins
* Test the case where we have a logical not going to a float
*/


pub fn logical_not_to_float_int(x:i32) -> f32 {
	let x_not:f32 = !x;
	ret x_not;
}


pub fn logical_not_to_float(f:i32) -> f32 {
	let f_not:f32 = !f;
	ret f_not;
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
