/**
* Author: Jack Robbins
* This test file checks for the case where we attempt to dereference an 
* uninitialized pointer
*/


pub fn main() -> i32 {
	//Declare it
	declare c:i32*;

	//This should *fail*. We're trying to use
	//C without having ever initialized it
	*(c + 1) = 3;
	
	ret 0;
}
