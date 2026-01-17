/**
* Author: Jack Robbins
* This test file covers storing floating point numbers
*/


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
