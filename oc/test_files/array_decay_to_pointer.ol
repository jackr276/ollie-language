/**
* Author: Jack Robbins
* Test a basic array decay to pointer
*/

pub fn get_offset_value(x:i32*) -> i32 {
	ret x[3];
}

pub fn main() -> i32 {
	let x:i32[] = [1, 2, 3, 4, 5];

	//This should work just fine
	ret @get_offset_value(x);
}
