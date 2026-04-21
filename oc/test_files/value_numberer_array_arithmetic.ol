/**
* Author: Jack Robbins
* Test a case where the value numberer should come in and optimize/reuse some array address arithmetic
* for us
*/


pub fn address_computations(x:i32[10][10], y:i32) -> i32 {
	let z1:i32 = x[1][y];
	let z2:i32 = x[1][y];

	ret z1 + z2;
}



pub fn main() -> i32 {
	ret 0;
}
