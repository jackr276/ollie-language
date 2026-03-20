/**
* Author: Jack Robbins
* Test the case where we have an invalid elaborative function parameter for a function pointer
*/

//This is wrong
define fn(i32, params i32[]) -> i32 as my_invalid_fn;

//Dummy
pub fn main() -> i32 {
	ret 0;
}
