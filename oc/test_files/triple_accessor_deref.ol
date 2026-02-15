/**
* Author: Jack Robbins
* Test a more extreme case of dereferencing when using the [] operation
*/

pub fn triple_pointer_access(x:char***) -> i32 {
	ret x[1][2][3];
}

pub fn main() -> i32 {
	ret 0;
}
