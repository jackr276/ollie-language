/**
* Author: Jack Robbins
* Test an attempt to assign arrays with an index mismatch
*/

pub fn get_array_value(x:i32[5]) -> i32 {
	ret x[4];
}


pub fn main() -> i32 {
	let x:i32[] = [1, 2, 3, 4];

	//Should fail - size mismatch
	ret @get_array_value(x);
}
