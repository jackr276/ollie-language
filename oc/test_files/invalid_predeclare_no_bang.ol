/**
* Author: Jack Robbins
* Test the fail case where we're using the "raises" keyword with no bang
*/

define error custom_error;

//Invalid - no bang
declare pub fn error() -> i32 raises(custom_error);

pub fn main() -> i32 {
	ret 0;
}
