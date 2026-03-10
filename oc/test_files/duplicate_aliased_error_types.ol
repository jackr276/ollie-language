/**
* Author: Jack Robbins
* We are able to alias error types. We must validate the case where we have duplicate error types that have been
* aliased
*/

define error error1;
define error error2;
define error error3;

//Alias it up
alias error1 as first_error;

//Should fail as we have a duplicate error
pub fn! duplicate_declaration(x:i32) -> i32 raises(error1, first_error, error2) {
	ret 0;
}


pub fn main() -> i32 {
	ret 0;
}
