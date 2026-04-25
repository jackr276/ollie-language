/**
* Author: Jack Robbins
* Test our worst case scenario - short circuiting with a ternary
*/


pub fn short_circuit_ternary(x:i32, y:i32, z:i32, aa:i32) -> i32 {
	if(x > 3 && (y > z ? z else y)) {
		ret 1;
	} else {
		ret 0;
	}
}


//Dummy(for now)
pub fn main() -> i32 {
	ret 0;
}
