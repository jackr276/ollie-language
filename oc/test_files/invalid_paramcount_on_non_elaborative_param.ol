/**
* Author: Jack Robbins
* Test the handling when we attempt to take the paramcount of a non-elaborative parameter or anything
* for that reason
*/

pub fn invalid_param_count(x:i32, y:i32, z:i32[5]) -> i32 {
	//Invalid - not going to work
	let invalid_param_count_var:i32 = paramcount(z);

	ret invalid_param_count_var;
}

pub fn main() -> i32 {
	ret 0;
}
