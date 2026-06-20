/**
* Author: Jack Robbins
* Test our handling of static array types
*/


pub fn static_array(i:i32) -> i32 {
	let static x:i32[10] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

	ret x[i];
}


pub fn main() -> i32 {
	OUNIT: [console = 3]
	ret @static_array(2);
}
