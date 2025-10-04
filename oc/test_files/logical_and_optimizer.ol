/**
* Author: Jack Robbins
* Test the optimization of logical and with zero & non-zero values
*/


//Should trigger the 0 pattern
pub fn logical_and_zero(x:i32) -> i32 {
	ret x && 0;
}

//Should trigger the 0 pattern
pub fn logical_and_zero_reverse(x:i32) -> i32 {
	ret 0 && x;
}

//Should trigger the non-zero pattern
pub fn logical_and_nonzero(x:i32) -> i32 {
	ret x && 3;
}

//Should trigger the non-zero pattern
pub fn logical_and_nonzero_reverse(x:i32) -> i32 {
	//ret 3 && x;
	ret 0;
}

pub fn main() -> i32 {
	ret 0;
}
