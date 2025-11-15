/**
* Author: Jack Robbins
* This file tests to/from memory converting moves
*/


//Test a scenario where an expanding move is needed by the first
//operand
pub fn test_add(a:i64, ptr:i32*) -> i64 {
	ret *ptr + a;
}


//Test a scenario where an expanding move is needed by the first
//operand
pub fn test_subtract(a:i64, ptr:i32*) -> i64 {
	ret *ptr - a;
}

//Test a scenario where an expanding move is needed by the first
//operand
pub fn test_subtract_array(a:i64, ptr:i32*) -> i64 {
	ret ptr[33] - a;
}


pub fn main() -> i32 {
	ret 0;
}
