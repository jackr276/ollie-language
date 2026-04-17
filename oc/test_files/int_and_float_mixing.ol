/**
* Author: Jack Robbins
* Test our ability to handle mixing ints and floats and inserting converting moves where 
* need be
*/

pub fn float_result_int_sub(x:i32, y:i32) -> f32 {
	let float:f32 = x - y;

	ret float;
}

pub fn float_result_int_additives(x:i32, y:i32) -> f32 {
	let float:f32 = x + y;

	ret float;
}


pub fn main() -> i32 {
	let x:mut i32 = 5;
	let f:f32 = 3.33;

	//Should be int in the end
	x += f;

	ret x;
}
