/**
* Author: Jack Robbins
* Test the case where the user tries to raise a non-error type. This is not possible, only error
* types can be raised
*/

alias i64 as u_int64_t;

pub fn! invalid_raises_non_error_type(x:i32) -> i32 {
	//Not an error - not gonna work
	if(x < 0) {
		raise u_int64_t;
	}
	
	ret x;
}
