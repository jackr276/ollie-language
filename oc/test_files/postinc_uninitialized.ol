/**
* Author: Jack Robbins
* Attempt to postincrement an uninitialized variable
*/

pub fn main() -> i32 {
	declare mut c:i32;

	//Should fail - we never initialized
	c++;

	ret 0;
}
