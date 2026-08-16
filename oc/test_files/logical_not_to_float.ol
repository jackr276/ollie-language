/**
* Author: Jack Robbins
* Test the case where we have a logical not going to a float
*/


pub fn logical_not_to_float_int(x:i32) -> f32 {
	let y:f32 = 3.3;

	ret y + <f32>!x;
}


pub fn logical_not_to_float(f:i32) -> f32 {
	let f_not:f32 = !f;
	ret f_not;
}


pub fn main() -> i32 {
	OUNIT:[exit_status = 4]
	ret @logical_not_to_float_int(0);
}
