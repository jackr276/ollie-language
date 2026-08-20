/**
* Author: Jack Robbins
* Test our handling of static array types
*/


pub fn static_array(i:i32, new_val:i32) -> i32 {
	let static x:mut i32[10] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

	//Assign over the new value
	x[i] = new_val;

	ret x[i];
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 18]
	ret @static_array(2, 18);
}
