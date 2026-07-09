/**
* Author: Jack Robbins
* Test our error messaging capabilities when we have duplicate functions within the same namespace
*/


namespace tester
{
	//Declared twice - will fail
	declare fn duplicate() -> i32;
	declare fn duplicate() -> i32;

	pub fn duplicate() -> f32 {
		ret 3.33;
	}
}


//Dummy
pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret 0;
}
