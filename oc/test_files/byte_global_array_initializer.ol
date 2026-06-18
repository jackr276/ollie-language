/**
* Author: Jack Robbins
* Test initializing a static array with bytes
*/


pub fn byte_static_array(x:bool) -> i32 {
	let static arr:i8[10] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

	if(x) {
		ret arr[3];
	} else {
		ret arr[2];
	}
}


pub fn main() -> i32 {
	OUNIT:[console = 3]
	ret @byte_static_array(false);
}
