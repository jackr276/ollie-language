/**
* Author: Jack Robbins
* Test the interference of those pxor_clears we use
*/


pub fn float_logical_or_expanding(x:f32, y:f32, z:f32) -> i32 {
	//Get around the short circuiting
	let result:bool = x || y;

	//Do some float math to test interference
	if(result == 0) {
		ret z + 3.33;

	} else {
		ret z - 3.33;
	}
}




//Dummy
pub fn main() -> i32 {
	ret 0;
}
