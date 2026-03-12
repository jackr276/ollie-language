/**
* Author: Jack Robbins
* Test the case where the error definition does not 
* have a terminating semicolon
*/

//We should catch this
define error my_error

pub fn main() -> i32 {
	ret 0;
}
