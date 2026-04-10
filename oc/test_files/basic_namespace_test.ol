/**
* Author: Jack Robbins
* Test basic uses of the ollie namespace. This is a valid case and should compile
*/


namespace tester
{
//Simple dummy function
pub fn my_fn() -> i32 {
	ret 5;
}
}


//This should work and return 5
pub fn main() -> i32 {
	ret @tester::my_fn();
}
