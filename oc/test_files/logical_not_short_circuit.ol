/**
* Author: Jack Robbins
* Test logical not short circuiting for both float and GP
*/


pub fn sse_logical_not_short_circuit(x:f32, y:f32) -> i32 {
	if (!x && !y) {
		ret 22;
	} else {
		ret 0;
	}
}


pub fn gp_logical_not_short_circuit(x:i32, y:i32) -> i32 {
	if (!x && !y) {
		ret 22;
	} else {
		ret 0;
	}
}


pub fn main() -> i32 {
	ret 0;
}
