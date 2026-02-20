/**
* Author: Jack Robbins
* Test how Ollie forbids variables that duplicate the paramter name inside of the 
* highest scope of the function. This is of course allowed in all lower scopes, but not the
* uppermost one
*/

pub fn tester(duplicate:i32) -> i32 {
	//Should fail - in the uppermost scope you can't
	//duplicate a parameter name
	let duplicate:i32 = 5;

	ret duplicate;
}

//Dummy
pub fn main() -> i32 {
	ret 0;
}
