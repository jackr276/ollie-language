/**
* Author: Jack Robbins
* Test the case where we have a duplicate error inside of a predeclaration raises
* clause
*/

define error error1;
define error error2;
define error error3;

//Should never get past here
declare pub fn! duplicate_errors(i32) -> i32 raises(error1, error2, error2);

pub fn! duplicate_errors(x:i32) -> i32 raises(error1, error2, error2) {
	ret 0;
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
