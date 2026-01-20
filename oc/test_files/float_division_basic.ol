/**
* Author: Jack Robbins
* Test the most basic case for float division
*/

pub fn divide_int_by_float(x:f32, y:i32) -> f32{
	ret y / x;
}

pub fn divide_const_by_float(x:f32) -> f32{
	ret 3 / x;
}

pub fn divide_float_by_int(x:f32) -> f32{
	ret x / 3;
}


pub fn main() -> i32 {
	let x:f32 = 5.22;
	let y:f32 = 5.555;

	//Implicit conversion to int after
	let result:i32 = x / y;

	ret result;
}
