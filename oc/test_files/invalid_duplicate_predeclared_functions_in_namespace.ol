/**
* Author: Jack Robbins
* Test our error messaging capabilities when we have duplicate functions within the same namespace
*/


namespace tester
{
	//Declared twice - will fail
	declare pub fn duplicate() -> i32;
	declare pub fn duplicate() -> i32;

	pub fn duplicate() -> f32 {
		ret 3.33;
	}
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
