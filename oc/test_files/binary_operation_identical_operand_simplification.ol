/**
* Author: Jack Robbins
* Test out identical operand simplification for binary operations
*/

pub fn identical_plus(x:i32) -> i32 {
	ret x + x;
}


pub fn identical_plus_float(x:f32) -> f32 {
	ret x + x;
}

pub fn identical_minus(x:i32) -> i32 {
	ret x - x;
}

pub fn identical_minus_float(x:f32) -> f32 {
	ret x - x;
}

pub fn identical_xor(x:i32) -> i32 {
	ret x ^ x;
}

pub fn identical_and(x:i32) -> i32 {
	ret x & x;
}

pub fn identical_or(x:i32) -> i32 {
	ret x | x;
}

//Dummy
pub fn main() -> i32 {
	ret 0;
}
