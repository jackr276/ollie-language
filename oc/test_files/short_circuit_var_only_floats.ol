/**
* Author: Jack Robbins
* Test the short circuit optimizer's ability to test when we have: variables only, float variables
*/


pub fn short_circuit_float_only(x:f32, y:f64) -> i32 {
	if(x && y) {
		ret 1;
	} else {
		ret 0;
	}
}


pub fn short_circuit_float_only_v2(x:f32, y:f64) -> i32 {
	if(x || y) {
		ret 1;
	} else {
		ret 0;
	}
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
