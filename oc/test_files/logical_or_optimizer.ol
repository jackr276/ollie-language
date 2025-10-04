/**
* Author: Jack Robbins
* Test the optimization of logical or with zero & non-zero values
*/


//Should trigger the 0 pattern
pub fn logical_or_zero(x:i32) -> i32 {
	ret x || 0;
}

//Should trigger the 0 pattern
pub fn logical_or_zero_constant(x:i32) -> i32 {
	ret 3 || 0;
}

//Should trigger the 0 pattern
pub fn logical_or_nonzero_constant(x:i32) -> i32 {
	ret 3 || 3;
}

//Should trigger the non-zero pattern
pub fn logical_or_nonzero(x:i32) -> i32 {
	ret x || 3;
}

pub fn main() -> i32 {
	ret 0;
}
