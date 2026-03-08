/**
* Author: Jack Robbins
* Test some invalid error type duplication
*/

define error first_error;
define error second_error;
//Duplicate should fail
define error first_error;

pub fn main() -> i32 {
	ret 0;
}
