/**
* Author: Jack Robbins
* Test the case where we have a handle statement on a function call where the function does not raise errors
*/

define error generic_error;

pub fn no_errors(x:i32) -> i32 {
	ret x + 2;
}


pub fn main() -> i32 {
	//Invalid - no errors so nothing to handle
	let result:i32 = @no_errors(3) handle(generic_errror => -1, error => 1);

	ret result;
}
