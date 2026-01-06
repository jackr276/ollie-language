/**
* Author: Jack Robbins
* Test the most basic level of adding an f32 and an f64
*/



pub fn add_floats_direct() -> f32 {
	let x:f32 = 3.33;
	let y:f32 = 7.33;

	ret x + y;
}


pub fn add_floats(x:f32) -> f32 {
	ret x + 3.33;
}


pub fn add_doubles(x:f64) -> f64 {
	//Hard force to double
	ret x + .333333D;
}

//Dummy function
pub fn main() -> i32 {
	ret 0;
}
