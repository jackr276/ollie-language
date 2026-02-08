/**
* Author: Jack Robbins
* Test how the compiler handles going from a byte to a float
*/

pub fn char_to_float(x:char) -> f32 {
	//Should trigger a conversion
	ret x;
}

pub fn byte_to_float(x:u8) -> f32 {
	//Should trigger a conversion
	ret x;
}

//Dummy
pub fn main() -> i32 {
	ret 0;
}
