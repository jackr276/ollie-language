/**
* Author: Jack Robbins
* This test file covers storing floating point numbers
*/

pub fn ints_in_double_spot(arr:mut f64*, x:i32, y:i32) -> void {
	arr[1] = x;
	arr[2] = y;

	//Should both be converted
	arr[3] = 3;
	arr[4] = 4;
}

pub fn doubles_in_long_spot(arr:mut i64*, x:f64, y:f64) -> void {
	arr[1] = x;
	arr[2] = y;

	//Should both be converted
	arr[3] = 3.333;
	arr[4] = 4.15;
}


pub fn ints_in_float_spot(arr:mut f32*, x:i32, y:i32) -> void {
	arr[1] = x;
	arr[2] = y;

	//Should both be converted
	arr[3] = 3;
	arr[4] = 4;
}


pub fn floats_in_int_spot(arr:mut i32*, x:f32, y:f32) -> void {
	arr[1] = x;
	arr[2] = y;

	//Should both be converted
	arr[3] = 3.333;
	arr[4] = 4.444;
}


pub fn store_doubles(arr:mut f64*, x:f32, y:f64) -> void {
	arr[1] = x;
	arr[2] = y;

	//Storing a constant
	arr[3] = 3.333d;

	//Converting move
	arr[4] = 4.444;
}


pub fn store_floats(arr:mut f32*, x:f32, y:f32) -> void {
	arr[1] = x;
	arr[2] = y;

	//Storing a constant
	arr[3] = 3.333;
}


pub fn main() -> i32 {
	ret 0;
}
