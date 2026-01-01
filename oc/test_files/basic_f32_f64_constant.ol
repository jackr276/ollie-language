/**
* Author: Jack Robbins
* Test the most basic level of float constants
*/

SABOTAGED


pub fn ret_f32() -> f32 {
	ret 3.33;
}


pub fn ret_f64() -> f64 {
	//Hard force to double
	ret 3.333333D;
}

//Dummy function
pub fn main() -> i32 {
	ret 0;
}
