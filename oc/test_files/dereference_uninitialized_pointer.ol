/**
* Author: Jack Robbins
* This test file checks for the case where we attempt to dereference an 
* uninitialized pointer
*/

/**
* This should work fine, because we *assume*
* that function paramaters come to use initialized
*/
pub fn pointer_deref(x:i32*) -> i32 {
	ret *x;
}


pub fn main() -> i32 {
	//Declare it
	declare c:i32*;

	//This should *fail*. We're trying to use
	//C without having ever initialized it
	*c = 3;
	
	ret 0;
}
