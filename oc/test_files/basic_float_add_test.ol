/**
* Author: Jack Robbins
* Test the most basic level of adding an f32 and an f64
*/

pub fn add_floats(x:f32) -> f32 {
	ret x + 3.33;
}


pub fn add_doubles(x:f64) -> f32 {
	//Hard force to double
	ret x + .333333D;
}

//Dummy function
pub fn main() -> i32 {
	ret 0;
}
