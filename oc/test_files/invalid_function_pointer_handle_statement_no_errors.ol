/**
* Author: Jack Robbins
* Test the case where we have a handle statement on a function call where the function does not raise errors
*/

define error generic_error;

pub fn no_errors(x:i32) -> i32 {
	ret x + 2;
}


pub fn main() -> i32 {
	//Function pointer for indirect call
	let x:fn(i32) -> i32 = no_errors;

	//Test the same thing for a function pointer
	let result:i32 = @x(3) handle(generic_errror => -1, error => 1);

	ret result;
}
