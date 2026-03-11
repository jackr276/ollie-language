/**
* Author: Jack Robbins
* Test the case where we try to raise an error in an unraisable function
*/

define error error1;


pub fn not_raisable(x:i32) -> i32 {
	if(x < 0) {
		//Should fail - function is not errorable
		raise error1;
	}

	ret 0;
}


pub fn main() -> i32 {
	ret 0;
}
