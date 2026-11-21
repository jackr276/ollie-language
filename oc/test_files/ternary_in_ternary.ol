/**
* Author: Jack Robbins
* This program tests the functionality of nested ternaries
*/


pub fn ternary_in_ternary(x:i32, y:i32, z:i32, a:i32) -> i32 {
	ret x == 2 ? x else (x == 3 ? z else a);
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 5]
	ret @ternary_in_ternary(3, 4, 5, 6);
}
