/**
* Author: Jack Robbins
* Test the most basic case for float multiplication 
*/

pub fn multilpy_int_by_float(x:f32, y:i32) -> f32{
	ret y * x;
}

pub fn multiply_const_by_float(x:f32) -> f32{
	ret 3 * x;
}

pub fn multilpy_float_by_int(x:f32, y:i32) -> f32{
	ret x * y;
}

pub fn multiply_float_by_const(x:f32) -> f32{
	ret x * 3;
}

pub fn main() -> i32 {
	let x:f32 = 5.22;
	let y:f32 = 5.555;

	//Implicit conversion to int after
	let result:i32 = x * y;

	ret result;
}
