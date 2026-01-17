/**
* Author: Jack Robbins
* This test file covers loading floating point numbers and the conversions
* that can happen with that
*/

pub fn longs_from_double_spot(arr:mut f64*) -> i64 {
	let x:i64 =  arr[1];
	let y:i64 =  arr[2];

	ret x + y;
}

pub fn doubles_from_long_spot(arr:mut i64*) -> f64 {
	let x:f64 = arr[1];
	let y:f64 = arr[2];

	ret x + y;
}

pub fn ints_from_float_spot(arr:mut f32*) -> i32 {
	let x:i32 = arr[1];
	let y:i32 = arr[2];

	ret x + y;
}

pub fn floats_from_int_spot(arr:mut i32*) -> f32 {
	let x:f32 = arr[1];
	let y:f32 = arr[2];

	ret x + y;
}


pub fn load_doubles(arr:mut f64*) -> f64 {
	let x:f64 = arr[1];
	let y:f64 = arr[2];

	ret x + y;
}


pub fn load_floats(arr:mut f32*) -> f32 {
	let x:f32 = arr[1];
	let y:f32 = arr[2];

	ret x + y;
}


pub fn main() -> i32 {
	ret 0;
}
