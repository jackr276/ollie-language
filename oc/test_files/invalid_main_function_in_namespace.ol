/**
* Author: Jack Robbins
* Test a case where we've tried to declare the name function inside of a namespace. This is invalid and must be blocked
*/


namespace invalid
{
	//This should fail - you may never declare a function named "main" inside of a namespace
	pub fn main() -> i32 {
		ret 0;
	}
}
