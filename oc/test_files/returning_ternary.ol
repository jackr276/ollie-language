/**
* Author: Jack Robbins
* This program tests the functionality of returning a ternary operator
*/

pub fn return_ternary(x:i32, a:i32, b:i32) -> i32 {
	ret x == 0 ? a - 1 else b + 2;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 5]
	ret @return_ternary(2, 3, 3);
}
